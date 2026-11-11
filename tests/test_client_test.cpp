#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "fiasco/app/app.hpp"
#include "fiasco/app/test_client.hpp"
#include "fiasco/contract/request.hpp"
#include "fiasco/contract/response.hpp"
#include "fiasco/contract/result.hpp"
#include "fiasco/internal/http/http_error.hpp"

namespace fiasco {
using app = detail::app;
using async = detail::async;
using http_error = detail::http_error;
using next = detail::next_handler;
using request = detail::request;
using response = detail::response;
using test_client = detail::test_client;
}  // namespace fiasco

namespace {

fiasco::request make_request(std::string_view method, std::string url,
                             std::string body = {}) {
    fiasco::request req;
    req.method = fiasco::detail::string_to_method(std::string(method));
    req.url = std::move(url);
    req.body = std::move(body);
    req.reanchor_url_views();
    return req;
}

}  // namespace

// ============================================================
// In-process test client
// ============================================================

TEST_CASE("test client serves synchronous handlers", "[test_client]") {
    fiasco::app app;
    app.get("/hello", []() { return "Hello"; });

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/hello"));

    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "Hello\n");
}

TEST_CASE("test client serves async handlers", "[test_client]") {
    fiasco::app app;
    app.get("/async", []() -> fiasco::async { co_return 42; });

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/async"));

    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "42\n");
}

TEST_CASE("test client binds path parameters", "[test_client]") {
    fiasco::app app;
    app.get("/users/{id}", [](int id) { return id; });

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/users/7"));

    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "7\n");
}

TEST_CASE("test client returns 404 for unmatched routes", "[test_client]") {
    fiasco::app app;
    app.get("/hello", []() { return "Hello"; });

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/missing"));

    REQUIRE(res.status_code == 404);
}

TEST_CASE("test client runs middleware", "[test_client]") {
    fiasco::app app;
    app.middleware([](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
        req.headers["X-Test"] = "yes";
        co_return co_await next(req);
    });
    app.get("/hello", [](const fiasco::request& req) {
        return req.headers.at("X-Test");
    });

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/hello"));

    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "yes\n");
}

TEST_CASE("test client translates exceptions", "[test_client]") {
    fiasco::app app;
    app.get("/boom", []() -> fiasco::response {
        throw fiasco::http_error(418, "teapot");
    });

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/boom"));

    REQUIRE(res.status_code == 418);
    REQUIRE(res.body.find("teapot") != std::string::npos);
}

// ============================================================
// Mounted sub-applications
// ============================================================

TEST_CASE("test client mounts sub-applications", "[test_client][mount]") {
    fiasco::app sub;
    sub.get("/hello", []() { return "sub"; });

    fiasco::app app;
    app.mount("/api", sub);

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/api/hello"));

    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "sub\n");
}

TEST_CASE("test client strips the mount prefix", "[test_client][mount]") {
    fiasco::app sub;
    sub.get("/hello", [](const fiasco::request& req) { return std::string(req.path); });

    fiasco::app app;
    app.mount("/api", sub);

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/api/hello"));

    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "/hello\n");
}

TEST_CASE("test client requires a mount segment boundary", "[test_client][mount]") {
    fiasco::app sub;
    sub.get("/hello", []() { return "sub"; });

    fiasco::app app;
    app.mount("/api", sub);

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/apix/hello"));

    REQUIRE(res.status_code == 404);
}

TEST_CASE("test client strips a trailing-slash mount prefix", "[test_client][mount]") {
    fiasco::app sub;
    sub.get("/hello", [](const fiasco::request& req) { return std::string(req.path); });

    fiasco::app app;
    app.mount("/api/", sub);

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/api/hello"));

    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "/hello\n");
}

TEST_CASE("test client mounts at the root", "[test_client][mount]") {
    fiasco::app sub;
    sub.get("/hello", [](const fiasco::request& req) { return std::string(req.path); });

    fiasco::app app;
    app.mount("/", sub);

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/hello"));

    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "/hello\n");
}

TEST_CASE("test client runs the sub-app's middleware", "[test_client][mount]") {
    fiasco::app sub;
    sub.middleware([](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
        req.headers["X-Sub"] = "yes";
        co_return co_await next(req);
    });
    sub.get("/hello", [](const fiasco::request& req) { return req.headers.at("X-Sub"); });

    fiasco::app app;
    app.mount("/api", sub);

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/api/hello"));

    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "yes\n");
}

TEST_CASE("test client runs the parent middleware for mounted routes", "[test_client][mount]") {
    fiasco::app sub;
    sub.get("/hello", [](const fiasco::request& req) { return req.headers.at("X-Parent"); });

    fiasco::app app;
    app.middleware([](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
        req.headers["X-Parent"] = "yes";
        co_return co_await next(req);
    });
    app.mount("/api", sub);

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/api/hello"));

    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "yes\n");
}

TEST_CASE("test client runs the sub-app's exception handler", "[test_client][mount]") {
    fiasco::app sub;
    sub.add_exception_handler([](const std::runtime_error& e) {
        return fiasco::response::text(std::string("sub: ") + e.what(), 500);
    });
    sub.get("/boom", []() -> fiasco::response {
        throw std::runtime_error("boom");
    });

    fiasco::app app;
    app.mount("/api", sub);

    fiasco::test_client client{app};
    auto res = client.handle(make_request("GET", "/api/boom"));

    REQUIRE(res.status_code == 500);
    REQUIRE(res.body.find("sub: boom") != std::string::npos);
}
