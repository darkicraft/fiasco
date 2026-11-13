#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <stdexcept>
#include <string>

#include "fiasco/fiasco.hpp"
#include "support/http_client.hpp"
#include "support/test_routes.hpp"
#include "support/test_server.hpp"

namespace {

struct middleware_user {
    std::string name;
    int age;
};

}  // namespace

TEST_CASE("middleware runs last-added-outermost", "[middleware]") {
    fiasco_test::test_server server{
        fiasco::server_config{},
    };
    auto& app = server.app();

    app.middleware([](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
        req.headers["X-Seen"] += "A";
        auto res = co_await next(req);
        res.headers["X-Out"] += "A";
        co_return res;
    });
    app.middleware([](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
        req.headers["X-Seen"] += "B";
        auto res = co_await next(req);
        res.headers["X-Out"] += "B";
        co_return res;
    });

    fiasco_test::get(
        app, "/order", [](const fiasco::request& req) { return req.header("X-Seen"); });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /order HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(fiasco_test::extract_body(resp) == "BA\n");
    REQUIRE(resp.find("X-Out: AB\r\n") != std::string::npos);
}

TEST_CASE("middleware can short-circuit", "[middleware]") {
    fiasco_test::test_server server{
        fiasco::server_config{},
    };
    auto& app = server.app();
    std::atomic<bool> handler_called{false};

    app.middleware([](fiasco::request&, const fiasco::next&) -> fiasco::async {
        co_return fiasco::response::text("blocked", 403);
    });
    fiasco_test::get(app, "/blocked", [&handler_called] {
        handler_called = true;
        return "handler";
    });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "GET /blocked HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 403);
    REQUIRE(fiasco_test::extract_body(resp) == "blocked\n");
    REQUIRE_FALSE(handler_called.load());
}

TEST_CASE("middleware can mutate request and response", "[middleware]") {
    fiasco_test::test_server server{
        fiasco::server_config{},
    };
    auto& app = server.app();

    app.middleware([](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
        req.headers["X-Injected"] = "yes";
        auto res = co_await next(req);
        res.status_code = 201;
        res.headers["X-After"] = "yes";
        co_return res;
    });
    fiasco_test::get(
        app, "/mutate", [](const fiasco::request& req) { return req.header("X-Injected"); });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /mutate HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 201);
    REQUIRE(fiasco_test::extract_body(resp) == "yes\n");
    REQUIRE(resp.find("X-After: yes\r\n") != std::string::npos);
}

TEST_CASE("middleware sees 404, 405, and 422 responses", "[middleware]") {
    fiasco_test::test_server server{
        fiasco::server_config{},
    };
    auto& app = server.app();

    app.middleware([](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
        auto res = co_await next(req);
        res.headers["X-Middleware"] = "seen";
        co_return res;
    });
    fiasco_test::post(app, "/only-post", [] { return "ok"; });
    fiasco_test::get(
        app, "/items/{id}", [](int id [[=fiasco::path{.ge = 1}]]) { return std::to_string(id); });

    server.start();

    auto not_found = fiasco_test::send_request(server.port(),
                                               "GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(not_found) == 404);
    REQUIRE(not_found.find("X-Middleware: seen\r\n") != std::string::npos);

    auto method_not_allowed = fiasco_test::send_request(
        server.port(), "GET /only-post HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(method_not_allowed) == 405);
    REQUIRE(method_not_allowed.find("X-Middleware: seen\r\n") != std::string::npos);

    auto validation = fiasco_test::send_request(server.port(),
                                                "GET /items/0 HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(validation) == 422);
    REQUIRE(validation.find("X-Middleware: seen\r\n") != std::string::npos);
}

TEST_CASE("middleware can catch an unhandled handler exception", "[middleware]") {
    fiasco_test::test_server server{
        fiasco::server_config{},
    };
    auto& app = server.app();

    app.middleware([](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
        try {
            co_return co_await next(req);
        } catch (const std::runtime_error& error) {
            co_return fiasco::response::error(error.what(), 409);
        }
    });
    fiasco_test::get(app, "/boom", []() -> std::string { throw std::runtime_error("caught"); });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /boom HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 409);
    REQUIRE(fiasco_test::extract_body(resp).find("caught") != std::string::npos);
}

TEST_CASE("middleware can throw http_error", "[middleware]") {
    fiasco_test::test_server server{
        fiasco::server_config{},
    };
    auto& app = server.app();

    app.middleware([](fiasco::request&, const fiasco::next&) -> fiasco::async {
        throw fiasco::http_error(401, "Not authenticated");
        co_return fiasco::response::empty();
    });
    fiasco_test::get(app, "/private", [] { return "secret"; });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "GET /private HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 401);
    REQUIRE(fiasco_test::extract_body(resp) == R"({"detail":"Not authenticated"})"
                                               "\n");
}

TEST_CASE("middleware cannot call next twice", "[middleware]") {
    fiasco_test::test_server server{
        fiasco::server_config{},
    };
    auto& app = server.app();

    app.middleware([](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
        (void)co_await next(req);
        co_return co_await next(req);
    });
    fiasco_test::get(app, "/twice", [] { return "ok"; });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /twice HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 500);
}

TEST_CASE("middleware wraps async handlers", "[middleware]") {
    fiasco_test::test_server server{
        fiasco::server_config{},
    };
    auto& app = server.app();

    app.middleware([](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
        auto res = co_await next(req);
        res.headers["X-Async-Middleware"] = "yes";
        co_return res;
    });
    fiasco_test::get(
        app, "/async", []() -> fiasco::async { co_return middleware_user{"ada", 36}; });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /async HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(fiasco_test::extract_body(resp) == R"({"name":"ada","age":36})"
                                               "\n");
    REQUIRE(resp.find("X-Async-Middleware: yes\r\n") != std::string::npos);
}
