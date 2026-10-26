#pragma once

#include <cstdint>
#include <stdexcept>
#include <thread>
#include <utility>

#include "fiasco/fiasco.hpp"

namespace fiasco_test {

class test_server {
  public:
    explicit test_server(fiasco::server_config config = {})
        : m_app(make_config(std::move(config))) {}

    test_server(const test_server&) = delete;
    test_server& operator=(const test_server&) = delete;

    ~test_server() { stop(); }

    fiasco::server& app() { return m_app; }

    void start() {
        if (m_thread.joinable()) {
            throw std::logic_error("test_server already started");
        }
        m_thread = std::thread([this] { m_app.run(); });
        m_app.wait_until_started();
        m_port = m_app.port();
    }

    void stop() {
        m_app.stop();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    [[nodiscard]] std::uint16_t port() const { return m_port; }

  private:
    static fiasco::server_config make_config(fiasco::server_config config) {
        config.network.host = "127.0.0.1";
        config.network.port = 0;
        return config;
    }

    fiasco::server m_app;
    std::thread m_thread;
    std::uint16_t m_port = 0;
};

}  // namespace fiasco_test
