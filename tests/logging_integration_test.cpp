#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

#include "fiasco/fiasco.hpp"
#include "support/http_client.hpp"
#include "support/test_server.hpp"
#include "thread_safe_sink.hpp"

// ---- Log capture helper ----------------------------------------

struct LogCapture {
    fiasco_test::thread_safe_sink captured;

    std::string str() const { return captured.str(); }
};

static fiasco::server_config test_config(fiasco::log_sink sink, bool access = true) {
    return {
        .logging = {.access = access, .sink = std::move(sink)},
    };
}

static std::string captured_logs(fiasco::server& app, LogCapture& capture) {
    app.flush_logs();
    return capture.str();
}

// ============================================================
// Integration tests
// ============================================================

TEST_CASE("server logs startup messages", "[logging][integration]") {
    LogCapture capture;
    fiasco_test::test_server server{
        test_config(fiasco::make_ostream_log_sink(capture.captured.stream()))};
    auto& app = server.app();
    app.get("/", []() { return "ok"; });

    server.start();

    auto out = captured_logs(app, capture);
    REQUIRE(out.find("starting server") != std::string::npos);
    REQUIRE(out.find("listening on") != std::string::npos);
}

TEST_CASE("server logs access line on GET", "[logging][integration]") {
    LogCapture capture;
    fiasco_test::test_server server{
        test_config(fiasco::make_ostream_log_sink(capture.captured.stream()))};
    auto& app = server.app();
    app.get("/hello", []() { return "world"; });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(resp.find("200") != std::string::npos);

    auto out = captured_logs(app, capture);
    REQUIRE(out.find("GET") != std::string::npos);
    REQUIRE(out.find("/hello") != std::string::npos);
    REQUIRE(out.find("200") != std::string::npos);
}

TEST_CASE("server logs 404 access line", "[logging][integration]") {
    LogCapture capture;
    fiasco_test::test_server server{
        test_config(fiasco::make_ostream_log_sink(capture.captured.stream()))};
    auto& app = server.app();
    app.get("/exists", []() { return "ok"; });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "GET /nonexistent HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(resp.find("404") != std::string::npos);

    auto out = captured_logs(app, capture);
    REQUIRE(out.find("404") != std::string::npos);
}

TEST_CASE("server logs 405 access line", "[logging][integration]") {
    LogCapture capture;
    fiasco_test::test_server server{
        test_config(fiasco::make_ostream_log_sink(capture.captured.stream()))};
    auto& app = server.app();
    app.get("/resource", []() { return "ok"; });

    server.start();

    auto resp = fiasco_test::send_request(
        server.port(), "POST /resource HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n");
    REQUIRE(resp.find("405") != std::string::npos);

    auto out = captured_logs(app, capture);
    REQUIRE(out.find("405") != std::string::npos);
}

TEST_CASE("access log can be disabled", "[logging][integration]") {
    LogCapture capture;
    fiasco_test::test_server server{
        test_config(fiasco::make_ostream_log_sink(capture.captured.stream()), false)};
    auto& app = server.app();
    app.get("/", []() { return "ok"; });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(resp.find("200") != std::string::npos);

    auto out = captured_logs(app, capture);
    // Should have startup messages but no access logs
    REQUIRE(out.find("fiasco.access") == std::string::npos);
}

TEST_CASE("servers have independent logging", "[logging][integration]") {
    LogCapture capture_a;
    LogCapture capture_b;

    fiasco_test::test_server server_a{
        test_config(fiasco::make_ostream_log_sink(capture_a.captured.stream()))};
    fiasco_test::test_server server_b{
        test_config(fiasco::make_ostream_log_sink(capture_b.captured.stream()))};
    auto& app_a = server_a.app();
    auto& app_b = server_b.app();
    app_a.get("/a", []() { return "a"; });
    app_b.get("/b", []() { return "b"; });

    server_a.start();
    server_b.start();

    fiasco_test::send_request(server_a.port(), "GET /a HTTP/1.1\r\nHost: localhost\r\n\r\n");
    fiasco_test::send_request(server_b.port(), "GET /b HTTP/1.1\r\nHost: localhost\r\n\r\n");

    auto out_a = captured_logs(app_a, capture_a);
    auto out_b = captured_logs(app_b, capture_b);
    REQUIRE(out_a.find("/a") != std::string::npos);
    REQUIRE(out_a.find("/b") == std::string::npos);
    REQUIRE(out_b.find("/b") != std::string::npos);
    REQUIRE(out_b.find("/a") == std::string::npos);
}

TEST_CASE("server logs shutdown message", "[logging][integration]") {
    LogCapture capture;
    fiasco_test::test_server server{
        test_config(fiasco::make_ostream_log_sink(capture.captured.stream()))};
    auto& app = server.app();
    app.get("/", []() { return "ok"; });

    server.start();
    server.stop();

    auto out = captured_logs(app, capture);
    REQUIRE(out.find("server stopped") != std::string::npos);
}
