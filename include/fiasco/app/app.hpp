#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fiasco/app/app_options.hpp"
#include "fiasco/app/docs.hpp"
#include "fiasco/app/openapi.hpp"
#include "fiasco/app/static_files.hpp"
#include "fiasco/contract/exchange.hpp"
#include "fiasco/contract/result.hpp"
#include "fiasco/internal/http/http_error.hpp"
#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/http/response.hpp"
#include "fiasco/internal/json.hpp"
#include "fiasco/internal/meta/reflect.hpp"
#include "fiasco/internal/middleware.hpp"
#include "fiasco/internal/router.hpp"

namespace fiasco {
struct server_config;
}  // namespace fiasco

namespace fiasco::detail {

/// The pure application object: routes, middleware, exception handlers, and
/// metadata. It owns no transport. `server` owns the socket and drives
/// `handle`; `app` is the seam the in-process test client uses.
class app {
  public:
    using exception_handler = std::move_only_function<response(std::exception_ptr)>;

    app() = default;
    explicit app(fiasco::app_options options, fiasco::router_options routes = {})
        : m_options(std::move(options)),
          m_router(std::move(routes)) {}

    void print_routes() const { m_router.print_routes(); }

    [[nodiscard]] const fiasco::app_options& options() const { return m_options; }

    /// The OpenAPI document, projected from the registered route descriptors.
    [[nodiscard]] json_type openapi() const { return build_openapi(m_options, m_router); }

    template <typename F>
    void add_exception_handler(F&& handler) {
        using traits = meta::callable_traits<std::decay_t<F>>;
        static_assert(traits::arity == 1,
                      "an exception handler must take exactly one exception parameter");
        using E = std::remove_cvref_t<std::tuple_element_t<0, typename traits::args_tuple>>;
        static_assert(std::derived_from<E, std::exception>,
                      "the exception handler parameter must derive from std::exception");
        add_exception_handler_impl<E>(std::forward<F>(handler));
    }

    template <std::derived_from<std::exception> E, typename F>
    void add_exception_handler(F&& handler) {
        add_exception_handler_impl<E>(std::forward<F>(handler));
    }

    template <typename F>
    void get(const std::string& path, F&& f) {
        m_router.add(http_method::get, path, std::forward<F>(f));
    }

    template <typename F>
    void post(const std::string& path, F&& f) {
        m_router.add(http_method::post, path, std::forward<F>(f));
    }

    template <typename F>
    void put(const std::string& path, F&& f) {
        m_router.add(http_method::put, path, std::forward<F>(f));
    }

    template <typename F>
    void del(const std::string& path, F&& f) {
        m_router.add(http_method::del, path, std::forward<F>(f));
    }

    template <typename F>
    void patch(const std::string& path, F&& f) {
        m_router.add(http_method::patch, path, std::forward<F>(f));
    }

    void include_router(router sub, const std::string& prefix = "") {
        m_router.include_router(std::move(sub), prefix);
    }

    template <std::meta::info Ns>
    void register_api() {
        m_router.register_api<Ns>();
    }

    /// Mount a sub-application at `path`. The sub-app's own middleware and
    /// exception handlers run, and the prefix is stripped before its routes
    /// are matched. Starlette-style: the mount path is a path-segment prefix.
    void mount(const std::string& path, app& sub) {
        m_sub_apps.push_back({path, &sub});
    }

    /// Serve files from `directory` under `prefix`.
    void mount_static(std::string prefix,
                      std::filesystem::path directory,
                      static_options options = {}) {
        m_static_mounts.push_back(static_mount{
            .prefix = normalize_static_prefix(std::move(prefix)),
            .directory = std::move(directory),
            .options = options});
    }

    template <typename F>
    void middleware(F&& fn) {
        m_middleware.add(middleware_fn{std::forward<F>(fn)});
    }

    /// The application seam. No socket, no logging, no threads.
    request_result handle(request req);

    /// Type-erased handler for the transport.
    application_handler handler() {
        return [this](exchange& ex) { return handle(ex); };
    }

    /// Exchange seam. Adapts the internal `handle(request)` fast path to the
    /// sink; the async result is awaited before the response is written.
    task<void> handle(exchange& ex);

    /// Single-server sugar; implemented by the umbrella target.
    void run();
    void run(server_config config);

  private:
    struct sub_app {
        std::string path;
        app* target;
    };

    static bool path_matches(std::string_view prefix, std::string_view path) {
        if (prefix.empty()) {
            return true;
        }
        if (path == prefix) {
            return true;
        }
        if (path.starts_with(prefix)) {
            return prefix.back() == '/' || path.size() > prefix.size() &&
                                              path[prefix.size()] == '/';
        }
        return false;
    }

    fiasco::app_options m_options;
    router m_router;
    middleware_chain m_middleware;
    std::vector<sub_app> m_sub_apps;
    std::vector<static_mount> m_static_mounts;
    std::unordered_map<std::type_index, exception_handler> m_exception_handlers;

    template <std::derived_from<std::exception> E, typename F>
    void add_exception_handler_impl(F&& handler) {
        m_exception_handlers.insert_or_assign(
            std::type_index(typeid(E)),
            [fn = std::forward<F>(handler)](std::exception_ptr error) mutable -> response {
                try {
                    std::rethrow_exception(error);
                } catch (const E& e) {
                    return fn(e);
                } catch (...) {
                    std::rethrow_exception(error);
                }
            });
    }

    [[nodiscard]] std::optional<response> try_builtin(const request& req) const {
        if (m_options.openapi_url.empty() || req.method != http_method::get) {
            return std::nullopt;
        }
        if (req.path == m_options.openapi_url) {
            return response::json(openapi().dump());
        }
        if (!m_options.docs_url.empty() && req.path == m_options.docs_url) {
            return response::html(swagger_ui_html(m_options));
        }
        if (!m_options.redoc_url.empty() && req.path == m_options.redoc_url) {
            return response::html(redoc_html(m_options));
        }
        return std::nullopt;
    }

    [[nodiscard]] response method_not_allowed(const request& req) const {
        response res = response::error("Method Not Allowed", 405);
        const auto methods = m_router.allowed_methods(req.path);
        if (!methods.empty()) {
            std::string allow;
            for (auto method : methods) {
                if (!allow.empty()) {
                    allow += ", ";
                }
                allow += method_to_string(method);
            }
            res.headers["Allow"] = std::move(allow);
        }
        return res;
    }

    [[nodiscard]] std::optional<response> try_redirect(const request& req) const {
        if (!m_router.options().redirect_slashes || req.path.empty() || req.path == "/") {
            return std::nullopt;
        }

        std::string alternate(req.path);
        if (alternate.back() == '/') {
            alternate.pop_back();
        } else {
            alternate.push_back('/');
        }
        if (!m_router.any_method_matches(alternate)) {
            return std::nullopt;
        }

        response res;
        res.status_code = 307;
        std::string location = std::move(alternate);
        if (!req.query_string.empty()) {
            location += '?';
            location.append(req.query_string);
        }
        res.headers["Location"] = std::move(location);
        return res;
    }

    [[nodiscard]] static response make_http_error_response(const http_error& error) {
        json_type body = json_type::object();
        body["detail"] = error.detail();
        response res = response::json(body.dump(), error.status_code());
        for (const auto& [name, value] : error.headers()) {
            res.headers[name] = value;
        }
        return res;
    }

    [[nodiscard]] response translate_known_exception(std::exception_ptr error) {
        try {
            std::rethrow_exception(error);
        } catch (const request_validation_error& e) {
            return response::json(to_fastapi_error_json(e.errors()), 422);
        } catch (const http_error& e) {
            if (auto it = m_exception_handlers.find(std::type_index(typeid(e)));
                it != m_exception_handlers.end()) {
                return invoke_exception_handler(it->second, error);
            }
            return make_http_error_response(e);
        } catch (const json::exception& e) {
            return response::error(e.what(), 422);
        } catch (const std::exception& e) {
            if (auto it = m_exception_handlers.find(std::type_index(typeid(e)));
                it != m_exception_handlers.end()) {
                return invoke_exception_handler(it->second, error);
            }
            throw;
        } catch (...) {
            throw;
        }
    }

    [[nodiscard]] response translate_exception(std::exception_ptr error) {
        try {
            std::rethrow_exception(error);
        } catch (const request_validation_error& e) {
            return response::json(to_fastapi_error_json(e.errors()), 422);
        } catch (const http_error& e) {
            if (auto it = m_exception_handlers.find(std::type_index(typeid(e)));
                it != m_exception_handlers.end()) {
                return invoke_exception_handler(it->second, error);
            }
            return make_http_error_response(e);
        } catch (const json::exception& e) {
            return response::error(e.what(), 422);
        } catch (const std::exception& e) {
            if (auto it = m_exception_handlers.find(std::type_index(typeid(e)));
                it != m_exception_handlers.end()) {
                return invoke_exception_handler(it->second, error);
            }
            return response::error(e.what(), 500);
        } catch (...) {
            return response::error("Internal Server Error", 500);
        }
    }

    [[nodiscard]] static response invoke_exception_handler(exception_handler& handler,
                                                           std::exception_ptr error) {
        try {
            return handler(error);
        } catch (const std::exception& e) {
            return response::error(e.what(), 500);
        } catch (...) {
            return response::error("Internal Server Error", 500);
        }
    }

    async dispatch_async(request req, async_handler_fn& handler);

    asio::awaitable<response> endpoint_handler(request& req);

    async dispatch_pipeline(request req);

    static task<void> write_response(response_sink& sink, response res);

    request_result dispatch(request req);

    std::optional<request_result> try_sub_app(request& req);
    std::optional<response> try_static(const request& req) const;
};

inline std::optional<request_result> app::try_sub_app(request& req) {
    for (const auto& sub : m_sub_apps) {
        if (!path_matches(sub.path, req.path)) {
            continue;
        }
        auto sub_req = req;
        std::string new_url = sub_req.url.substr(sub.path.size());
        if (new_url.empty() || new_url.front() != '/') {
            new_url.insert(new_url.begin(), '/');
        }
        sub_req.url = std::move(new_url);
        sub_req.reanchor_url_views();
        return sub.target->handle(std::move(sub_req));
    }
    return std::nullopt;
}

inline std::optional<response> app::try_static(const request& req) const {
    for (const auto& mount : m_static_mounts) {
        if (auto result = serve_static(mount, req)) {
            return std::move(*result);
        }
    }
    return std::nullopt;
}

inline request_result app::dispatch(request req) {
    req.reanchor_url_views();

    if (auto sub_result = try_sub_app(req)) {
        return std::move(*sub_result);
    }

    if (auto builtin = try_builtin(req)) {
        return std::move(*builtin);
    }

    if (auto static_result = try_static(req)) {
        return std::move(*static_result);
    }

    auto match = m_router.match(req.method, req.path);
    if (!match.matched) {
        if (m_router.any_method_matches(req.path)) {
            return method_not_allowed(req);
        }
        if (auto redirect = try_redirect(req)) {
            return std::move(*redirect);
        }
        return response::error("Not Found", 404);
    }

    if (match.is_async) {
        return dispatch_async(std::move(req), *match.async_handler);
    }

    req.path_params.clear();
    req.ordered_path_params.clear();
    for (const auto& [name, value] : match.path_params) {
        req.path_params.emplace(name, value);
        req.ordered_path_params.push_back(value);
    }
    req.query_params = parse_query(req.query_string);

    response res;
    try {
        res = (*match.handler)(req);
    } catch (...) {
        res = translate_exception(std::current_exception());
    }
    return res;
}

inline request_result app::handle(request req) {
    if (!m_middleware.empty()) {
        return dispatch_pipeline(std::move(req));
    }
    return dispatch(std::move(req));
}

inline task<void> app::handle(exchange& ex) {
    auto result = handle(std::move(ex.req));
    if (auto* res = std::get_if<response>(&result)) {
        co_await write_response(ex.res, std::move(*res));
        co_return;
    }
    auto res = co_await std::get<async>(std::move(result));
    co_await write_response(ex.res, std::move(res).take());
    co_return;
}

inline task<void> app::write_response(response_sink& sink, response res) {
    sink.status(res.status_code);
    for (const auto& [key, value] : res.headers) {
        sink.header(key, value);
    }
    for (const auto& cookie : res.set_cookie_headers) {
        sink.header("Set-Cookie", cookie);
    }
    if (!res.body.empty()) {
        co_await sink.write(
            std::as_bytes(std::span<const char>(res.body.data(), res.body.size())));
    }
    co_await sink.end();
    co_return;
}

inline async app::dispatch_async(request req, async_handler_fn& handler) {
    req.reanchor_url_views();
    auto match = m_router.match(req.method, req.path);
    req.path_params.clear();
    req.ordered_path_params.clear();
    for (const auto& [name, value] : match.path_params) {
        req.path_params.emplace(name, value);
        req.ordered_path_params.push_back(value);
    }
    req.query_params = parse_query(req.query_string);

    response res;
    try {
        auto result = co_await handler(req);
        res = std::move(result).take();
    } catch (...) {
        res = translate_exception(std::current_exception());
    }
    co_return res;
}

inline asio::awaitable<response> app::endpoint_handler(request& req) {
    if (auto sub_result = try_sub_app(req)) {
        if (auto* res = std::get_if<response>(&*sub_result)) {
            co_return *res;
        }
        auto res = co_await std::get<async>(std::move(*sub_result));
        co_return std::move(res).take();
    }

    if (auto builtin = try_builtin(req)) {
        co_return std::move(*builtin);
    }

    if (auto static_result = try_static(req)) {
        co_return std::move(*static_result);
    }

    auto match = m_router.match(req.method, req.path);
    if (!match.matched) {
        if (m_router.any_method_matches(req.path)) {
            co_return method_not_allowed(req);
        }
        if (auto redirect = try_redirect(req)) {
            co_return std::move(*redirect);
        }
        co_return response::error("Not Found", 404);
    }

    req.path_params.clear();
    req.ordered_path_params.clear();
    for (const auto& [name, value] : match.path_params) {
        req.path_params.emplace(name, value);
        req.ordered_path_params.push_back(value);
    }
    req.query_params = parse_query(req.query_string);

    if (match.is_async) {
        try {
            auto result = co_await (*match.async_handler)(req);
            co_return std::move(result).take();
        } catch (...) {
            co_return translate_known_exception(std::current_exception());
        }
    }

    try {
        co_return (*match.handler)(req);
    } catch (...) {
        co_return translate_known_exception(std::current_exception());
    }
}

inline async app::dispatch_pipeline(request req) {
    req.reanchor_url_views();

    response res;
    try {
        endpoint_fn endpoint = [this](request& request) -> asio::awaitable<response> {
            co_return co_await endpoint_handler(request);
        };
        res = co_await m_middleware.run(req, std::move(endpoint));
    } catch (...) {
        res = translate_exception(std::current_exception());
    }
    co_return res;
}

}  // namespace fiasco::detail
