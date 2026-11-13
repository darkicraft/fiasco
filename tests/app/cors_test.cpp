#include <catch2/catch_test_macros.hpp>

#include <string>

#include "fiasco/fiasco.hpp"
#include "support/http_client.hpp"
#include "support/test_routes.hpp"
#include "support/test_server.hpp"

namespace {

fiasco::server_config quiet_config() {
    return fiasco::server_config{};
}

bool has_header(const std::string& response, const std::string& header) {
    return response.find(header) != std::string::npos;
}

}  // namespace

TEST_CASE("cors allows a configured origin on a simple request", "[cors]") {
    fiasco_test::test_server server{quiet_config()};
    auto& app = server.app();
    app.middleware(fiasco::middleware::cors({
        .allow_origins = {"https://example.com"},
    }));
    fiasco_test::get(app, "/hello", [] { return "ok"; });

    server.start();

    auto resp = fiasco_test::send_request(
        server.port(),
        "GET /hello HTTP/1.1\r\nHost: localhost\r\nOrigin: https://example.com\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(has_header(resp, "Access-Control-Allow-Origin: https://example.com\r\n"));
    REQUIRE(has_header(resp, "Vary: Origin\r\n"));
}

TEST_CASE("cors wildcard origin works without credentials", "[cors]") {
    fiasco_test::test_server server{quiet_config()};
    auto& app = server.app();
    app.middleware(fiasco::middleware::cors({
        .allow_origins = {"*"},
    }));
    fiasco_test::get(app, "/hello", [] { return "ok"; });

    server.start();

    auto resp = fiasco_test::send_request(
        server.port(),
        "GET /hello HTTP/1.1\r\nHost: localhost\r\nOrigin: https://example.com\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(has_header(resp, "Access-Control-Allow-Origin: *\r\n"));
}

TEST_CASE("cors wildcard with credentials mirrors the origin", "[cors]") {
    fiasco_test::test_server server{quiet_config()};
    auto& app = server.app();
    app.middleware(fiasco::middleware::cors({
        .allow_origins = {"*"},
        .allow_credentials = true,
    }));
    fiasco_test::get(app, "/hello", [] { return "ok"; });

    server.start();

    auto resp = fiasco_test::send_request(
        server.port(),
        "GET /hello HTTP/1.1\r\nHost: localhost\r\nOrigin: https://example.com\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(has_header(resp, "Access-Control-Allow-Origin: https://example.com\r\n"));
    REQUIRE(has_header(resp, "Access-Control-Allow-Credentials: true\r\n"));
}

TEST_CASE("cors handles preflight", "[cors]") {
    fiasco_test::test_server server{quiet_config()};
    auto& app = server.app();
    app.middleware(fiasco::middleware::cors({
        .allow_origins = {"https://example.com"},
        .allow_methods = {"GET", "POST"},
        .allow_headers = {"X-Token"},
        .max_age = 300,
    }));
    fiasco_test::get(app, "/hello", [] { return "ok"; });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "OPTIONS /hello HTTP/1.1\r\nHost: localhost\r\n"
                                          "Origin: https://example.com\r\n"
                                          "Access-Control-Request-Method: POST\r\n"
                                          "Access-Control-Request-Headers: x-token\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(fiasco_test::extract_body(resp) == "OK\n");
    REQUIRE(has_header(resp, "Access-Control-Allow-Origin: https://example.com\r\n"));
    REQUIRE(has_header(resp, "Access-Control-Allow-Methods: GET, POST\r\n"));
    REQUIRE(has_header(resp, "Access-Control-Max-Age: 300\r\n"));
    REQUIRE(has_header(resp,
                       "Access-Control-Allow-Headers: accept, accept-language, "
                       "content-language, content-type, x-token\r\n"));
}

TEST_CASE("cors rejects disallowed preflight", "[cors]") {
    fiasco_test::test_server server{quiet_config()};
    auto& app = server.app();
    app.middleware(fiasco::middleware::cors({
        .allow_origins = {"https://example.com"},
        .allow_methods = {"GET"},
    }));
    fiasco_test::get(app, "/hello", [] { return "ok"; });

    server.start();

    auto bad_origin = fiasco_test::send_request(server.port(),
                                                "OPTIONS /hello HTTP/1.1\r\nHost: localhost\r\n"
                                                "Origin: https://evil.example\r\n"
                                                "Access-Control-Request-Method: GET\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(bad_origin) == 400);
    REQUIRE(fiasco_test::extract_body(bad_origin) == "Disallowed CORS origin\n");

    auto bad_method = fiasco_test::send_request(server.port(),
                                                "OPTIONS /hello HTTP/1.1\r\nHost: localhost\r\n"
                                                "Origin: https://example.com\r\n"
                                                "Access-Control-Request-Method: DELETE\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(bad_method) == 400);
    REQUIRE(fiasco_test::extract_body(bad_method) == "Disallowed CORS method\n");
}

TEST_CASE("cors adds headers to unmatched routes", "[cors]") {
    fiasco_test::test_server server{quiet_config()};
    auto& app = server.app();
    app.middleware(fiasco::middleware::cors({
        .allow_origins = {"https://example.com"},
    }));

    server.start();

    auto resp = fiasco_test::send_request(
        server.port(),
        "GET /missing HTTP/1.1\r\nHost: localhost\r\nOrigin: https://example.com\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 404);
    REQUIRE(has_header(resp, "Access-Control-Allow-Origin: https://example.com\r\n"));
}
