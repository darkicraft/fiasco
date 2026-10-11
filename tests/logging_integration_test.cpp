#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <sstream>
#include <string>
#include <thread>

#include <asio.hpp>

#include "fiasco/fiasco.hpp"

using asio::ip::tcp;
using namespace fiasco::detail;

// ---- helpers (from integration_test.cpp) ------------------------

static uint16_t find_free_port() {
    asio::io_context ioc;
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 0));
    uint16_t port = acceptor.local_endpoint().port();
    acceptor.close();
    return port;
}

static void wait_for_server(uint16_t port) {
    for (int i = 0; i < 50; ++i) {
        try {
            asio::io_context ioc;
            tcp::socket socket(ioc);
            socket.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
            socket.close();
            return;
        } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

struct ServerGuard {
    fiasco::server& app;
    std::thread& t;
    ~ServerGuard() {
        app.stop();
        if (t.joinable()) {
            t.join();
        }
    }
};

static std::string send_and_receive(uint16_t port, const std::string& raw) {
    asio::io_context ioc;
    tcp::socket socket(ioc);
    socket.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));

    asio::write(socket, asio::buffer(raw));

    std::string response;
    std::array<char, 4096> buf;
    asio::error_code ec;

    while (response.find("\r\n\r\n") == std::string::npos) {
        size_t n = socket.read_some(asio::buffer(buf), ec);
        if (ec) {
            break;
        }
        response.append(buf.data(), n);
    }

    auto header_end = response.find("\r\n\r\n");
    auto headers = response.substr(0, header_end);
    size_t content_length = 0;
    auto cl = headers.find("Content-Length: ");
    if (cl != std::string::npos) {
        auto start = cl + 16;
        auto end = headers.find("\r\n", start);
        content_length = std::stoul(headers.substr(start, end - start));
    }

    while (response.size() < header_end + 4 + content_length) {
        size_t n = socket.read_some(asio::buffer(buf), ec);
        if (ec) {
            break;
        }
        response.append(buf.data(), n);
    }

    return response;
}

// ---- Log capture helper ----------------------------------------

struct LogCapture {
    std::ostringstream captured;

    LogCapture() { log_registry::instance().set_sink(captured); }

    ~LogCapture() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        log_registry::instance().reset_sink();
    }

    std::string str() const { return captured.str(); }
};

// ============================================================
// Integration tests
// ============================================================

TEST_CASE("server logs startup messages", "[logging][integration]") {
    LogCapture capture;
    fiasco::server app;
    app.get("/", []() { return "ok"; });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto out = capture.str();
    REQUIRE(out.find("starting server") != std::string::npos);
    REQUIRE(out.find("listening on") != std::string::npos);
}

TEST_CASE("server logs access line on GET", "[logging][integration]") {
    LogCapture capture;
    fiasco::server app;
    app.get("/hello", []() { return "world"; });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(port, "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(resp.find("200") != std::string::npos);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto out = capture.str();
    REQUIRE(out.find("GET") != std::string::npos);
    REQUIRE(out.find("/hello") != std::string::npos);
    REQUIRE(out.find("200") != std::string::npos);
}

TEST_CASE("server logs 404 access line", "[logging][integration]") {
    LogCapture capture;
    fiasco::server app;
    app.get("/exists", []() { return "ok"; });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(port, "GET /nonexistent HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(resp.find("404") != std::string::npos);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto out = capture.str();
    REQUIRE(out.find("404") != std::string::npos);
}

TEST_CASE("server logs 405 access line", "[logging][integration]") {
    LogCapture capture;
    fiasco::server app;
    app.get("/resource", []() { return "ok"; });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(
        port, "POST /resource HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n");
    REQUIRE(resp.find("405") != std::string::npos);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto out = capture.str();
    REQUIRE(out.find("405") != std::string::npos);
}

TEST_CASE("access log can be disabled", "[logging][integration]") {
    LogCapture capture;
    fiasco::server app;
    app.enable_access_log(false);
    app.get("/", []() { return "ok"; });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);
    ServerGuard guard{app, t};

    auto resp = send_and_receive(port, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(resp.find("200") != std::string::npos);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto out = capture.str();
    // Should have startup messages but no access logs
    REQUIRE(out.find("fiasco.access") == std::string::npos);
}

TEST_CASE("server logs shutdown message", "[logging][integration]") {
    LogCapture capture;
    fiasco::server app;
    app.get("/", []() { return "ok"; });

    auto port = find_free_port();
    std::thread t([&] { app.run(port, "127.0.0.1"); });
    wait_for_server(port);

    app.stop();
    t.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto out = capture.str();
    REQUIRE(out.find("server stopped") != std::string::npos);
}
