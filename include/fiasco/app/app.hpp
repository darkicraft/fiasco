#pragma once

#include <concepts>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fiasco/app/app_options.hpp"
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
    explicit app(fiasco::app_options options) : m_options(std::move(options)) {}

    void print_routes() const { m_router.print_routes(); }

    [[nodiscard]] const fiasco::app_options& options() const { return m_options; }

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

    /// Mount a sub-application at `path`. The sub-app's own middleware and
    /// exception handlers run, and the prefix is stripped before its routes
    /// are matched. Starlette-style: the mount path is a path-segment prefix.
    void mount(const std::string& path, app& sub) {
        m_sub_apps.push_back({path, &sub});
    }

    template <typename F>
    void middleware(F&& fn) {
        m_middleware.add(middleware_fn{std::forward<F>(fn)});
    }

    /// The application seam. No socket, no logging, no threads.
    request_result handle(request req);

    /// Type-erased handler for the transport.
    request_handler handler() {
        return [this](request req) { return handle(std::move(req)); };
    }

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

    request_result dispatch(request req);

    std::optional<request_result> try_sub_app(request& req);
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

inline request_result app::dispatch(request req) {
    req.reanchor_url_views();

    if (auto sub_result = try_sub_app(req)) {
        return std::move(*sub_result);
    }

    auto match = m_router.match(req.method, req.path);
    if (!match.matched) {
        return m_router.any_method_matches(req.path)
            ? response::error("Method Not Allowed", 405)
            : response::error("Not Found", 404);
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

    auto match = m_router.match(req.method, req.path);
    if (!match.matched) {
        co_return m_router.any_method_matches(req.path)
            ? response::error("Method Not Allowed", 405)
            : response::error("Not Found", 404);
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
