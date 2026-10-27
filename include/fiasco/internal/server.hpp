#pragma once

#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include "fiasco/internal/core/tcp_server.hpp"
#include "fiasco/internal/http/http_error.hpp"
#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/http/response.hpp"
#include "fiasco/internal/json.hpp"
#include "fiasco/internal/logging.hpp"
#include "fiasco/internal/router.hpp"

namespace fiasco {

using log_level = detail::log_level;
using log_format = detail::log_format_type;

struct network_config {
    std::string host = "0.0.0.0";
    std::uint16_t port = 8080;
    unsigned int workers = 0;  // 0 = hardware concurrency, at least 2
};

struct logging_config {
    bool enabled = true;
    bool access = true;
    log_level level = log_level::info;
    log_format format = log_format::human;
    bool colors = true;
    log_sink sink;  // empty => std::cout
};

struct server_config {
    network_config network{};
    logging_config logging{};
};

}  // namespace fiasco

namespace fiasco::detail {

class server {
  public:
    using exception_handler = std::move_only_function<response(std::exception_ptr)>;

    void print_routes() const { m_router.print_routes(); }

    template <std::derived_from<std::exception> E, typename F>
    void add_exception_handler(F&& handler) {
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

    explicit server(fiasco::server_config config = {})
        : m_config(std::move(config)),
          m_logging(m_config.logging.enabled,
                    m_config.logging.access,
                    m_config.logging.level,
                    m_config.logging.format,
                    m_config.logging.colors,
                    std::move(m_config.logging.sink)) {}

    logger& get_logger(const std::string& name) { return m_logging.get(name); }

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

    void run() {
        const auto& network = m_config.network;
        const uint16_t port = network.port;
        const std::string& host = network.host;
        const unsigned int workers = network.workers;
        const unsigned int actual_threads =
            workers == 0 ? std::max(std::thread::hardware_concurrency(), 2u) : workers;
        {
            auto& log = m_logging.get("fiasco");
            log.info("starting server with " + std::to_string(actual_threads) + " threads");
            if (host == "0.0.0.0" || host == "::") {
                log.info("listening on port " + std::to_string(port) +
                         " (all interfaces, host: " + host + ")");
            } else {
                log.info("listening on " + host + ":" + std::to_string(port));
            }
        }
        // run() may be called on a different thread than stop(); guard m_server.
        tcp_server* running = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_server_mutex);
            m_run_finished = false;
            m_server = std::make_unique<tcp_server>(
                port, host, [this](request req) { return dispatch(std::move(req)); }, workers);
            running = m_server.get();
            m_server_cv.notify_all();
        }
        running->run();
        {
            std::lock_guard<std::mutex> lock(m_server_mutex);
            m_server.reset();
            m_run_finished = true;
            m_server_cv.notify_all();
        }
        m_logging.get("fiasco").info("server stopped");
    }

    void flush_logs() { m_logging.flush(); }

    [[nodiscard]] std::uint16_t port() const {
        std::lock_guard lock(m_server_mutex);
        return m_server ? m_server->port() : 0;
    }

    void wait_until_started() {
        std::unique_lock lock(m_server_mutex);
        m_server_cv.wait(lock, [this] { return m_server != nullptr || m_run_finished; });
        if (!m_server) {
            throw std::runtime_error("server did not start");
        }
    }

    void stop() {
        std::lock_guard<std::mutex> lock(m_server_mutex);
        if (m_server) {
            m_server->stop();
        }
    }

  private:
    fiasco::server_config m_config;
    log_registry m_logging;
    router m_router;
    std::unordered_map<std::type_index, exception_handler> m_exception_handlers;
    mutable std::mutex m_server_mutex;
    std::condition_variable m_server_cv;
    std::unique_ptr<tcp_server> m_server;
    bool m_run_finished = false;

    [[nodiscard]] static response make_http_error_response(const http_error& error) {
        json_type body = json_type::object();
        body["detail"] = error.detail();
        response res = response::json(body.dump(), error.status_code());
        for (const auto& [name, value] : error.headers()) {
            res.headers[name] = value;
        }
        return res;
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

    void log_access(access_log alog,
                    const response& res,
                    std::chrono::steady_clock::time_point start) {
        if (!m_logging.access_log_enabled() || !m_logging.logging_enabled()) {
            return;
        }
        auto end = std::chrono::steady_clock::now();
        alog.status_code = res.status_code;
        alog.duration_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        m_logging.get("fiasco.access").info(alog.build_message());
    }

    response dispatch(request req) {
        const bool access_logging = m_logging.access_log_enabled() && m_logging.logging_enabled();
        auto start = access_logging ? std::chrono::steady_clock::now()
                                    : std::chrono::steady_clock::time_point{};

        access_log alog;
        if (access_logging) {
            alog.client = req.client_address;
            alog.method = req.method;
            alog.path = req.path;
        }

        auto match = m_router.match(req.method, req.path);
        response res;

        if (!match.matched) {
            if (m_router.any_method_matches(req.path)) {
                res = response::error("Method Not Allowed", 405);
            } else {
                res = response::error("Not Found", 404);
            }
        } else {
            req.path_params.clear();
            req.ordered_path_params.clear();
            for (const auto& [name, value] : match.path_params) {
                req.path_params.emplace(name, value);
                req.ordered_path_params.push_back(value);
            }
            req.query_params = parse_query(req.query_string);

            try {
                res = (*match.handler)(std::move(req));
            } catch (...) {
                res = translate_exception(std::current_exception());
            }
        }

        if (access_logging) {
            log_access(std::move(alog), res, start);
        }
        return res;
    }
};

}  // namespace fiasco::detail
