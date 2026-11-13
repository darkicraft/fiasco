#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include "fiasco/contract/exchange.hpp"
#include "fiasco/http/tcp_server.hpp"
#include "fiasco/internal/config.hpp"

namespace fiasco::detail {

/// The transport: owns the socket and the worker threads. It drives the
/// type-erased application handler and never registers routes.
class server {
  public:
    explicit server(application_handler handler, fiasco::server_config config = {})
        : m_handler(std::move(handler)),
          m_config(std::move(config)) {}

    server(const server&) = delete;
    server& operator=(const server&) = delete;

    void run() {
        const auto& network = m_config.network;
        // run() may be called on a different thread than stop(); guard m_server.
        tcp_server* running = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_server_mutex);
            m_run_finished = false;
            m_server = std::make_unique<tcp_server>(
                network.port, network.host, std::move(m_handler), network.workers);
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
    }

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
    application_handler m_handler;
    fiasco::server_config m_config;
    mutable std::mutex m_server_mutex;
    std::condition_variable m_server_cv;
    std::unique_ptr<tcp_server> m_server;
    bool m_run_finished = false;
};

}  // namespace fiasco::detail
