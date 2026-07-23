#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "fiasco/internal/core/tcp_server.hpp"
#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/http/response.hpp"
#include "fiasco/internal/json.hpp"
#include "fiasco/internal/logging.hpp"
#include "fiasco/internal/router.hpp"

namespace fiasco::detail {

class server {
  public:
    void print_routes() const { m_router.print_routes(); }

    explicit server(unsigned int num_threads = 0)
        : m_threads(num_threads) {}

    server& log_level(const std::string& lvl) {
        log_registry::instance().set_level(log_level_from_string(lvl));
        return *this;
    }

    server& log_format(const std::string& fmt) {
        auto type = (fmt == "json") ? log_format_type::json : log_format_type::human;
        log_registry::instance().set_format(type);
        return *this;
    }

    server& enable_access_log(bool en) {
        log_registry::instance().enable_access_log(en);
        return *this;
    }

    logger& logger(const std::string& name) {
        return log_registry::instance().get(name);
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

    void run(uint16_t port = 8080, const std::string& host = "0.0.0.0") {
        unsigned int actual_threads = m_threads == 0
            ? std::max(std::thread::hardware_concurrency(), 2u)
            : m_threads;
        {
            auto& log = log_registry::instance().get("fiasco");
            log.info("starting server with " + std::to_string(actual_threads) + " threads");
            if (host == "0.0.0.0" || host == "::") {
                log.info("listening on port " + std::to_string(port) +
                         " (all interfaces, host: " + host + ")");
            } else {
                log.info("listening on " + host + ":" + std::to_string(port));
            }
        }
        m_server = std::make_unique<tcp_server>(
            port, host, [this](request req) { return dispatch(std::move(req)); }, m_threads);
        m_server->run();
        m_server.reset();
        log_registry::instance().get("fiasco").info("server stopped");
    }

    void stop() {
        if (m_server) {
            m_server->stop();
        }
    }

  private:
    router m_router;
    unsigned int m_threads = 0;
    std::unique_ptr<tcp_server> m_server;

    void log_access(const request& req, const response& res, std::chrono::steady_clock::time_point start) {
        auto& reg = log_registry::instance();
        if (!reg.access_log_enabled()) {
            return;
        }
        auto end = std::chrono::steady_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        access_log alog;
        alog.client = req.client_address;
        alog.method = req.method;
        alog.path = req.path;
        alog.status_code = res.status_code;
        alog.duration_us = us;
        reg.get("fiasco.access").info(alog.build_message());
    }

    response dispatch(request req) {
        auto start = std::chrono::steady_clock::now();

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

            try {
                res = match.handler(std::move(req));
            } catch (const json::exception& e) {
                res = response::error(e.what(), 422);
            } catch (const std::exception& e) {
                res = response::error(e.what(), 500);
            } catch (...) {
                res = response::error("Internal Server Error", 500);
            }
        }

        log_access(req, res, start);
        return res;
    }
};

}  // namespace fiasco::detail