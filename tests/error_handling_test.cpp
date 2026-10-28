#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>

#include "fiasco/fiasco.hpp"
#include "support/http_client.hpp"
#include "support/test_server.hpp"

namespace fiasco::detail {

struct app_error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct explicit_error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

}  // namespace fiasco::detail

TEST_CASE("http_error becomes an HTTP response", "[error_handling]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/auth", []() -> std::string {
        throw fiasco::http_error(401, "Not authenticated", {{"WWW-Authenticate", "Bearer"}});
    });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /auth HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 401);
    REQUIRE(fiasco_test::extract_body(resp).find(R"("detail":"Not authenticated")") !=
            std::string::npos);
    REQUIRE(resp.find("WWW-Authenticate: Bearer") != std::string::npos);
}

TEST_CASE("custom exception handler", "[error_handling]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.add_exception_handler([](const fiasco::detail::app_error& error) {
        return fiasco::response::error(error.what(), 409);
    });
    app.get("/conflict", []() -> std::string { throw fiasco::detail::app_error("conflict"); });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "GET /conflict HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 409);
    REQUIRE(fiasco_test::extract_body(resp) == R"({"detail":"conflict"})"
                                               "\n");
}

TEST_CASE("explicit exception handler type remains supported", "[error_handling]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.add_exception_handler<fiasco::detail::explicit_error>(
        [](const fiasco::detail::explicit_error& error) {
            return fiasco::response::error(error.what(), 410);
        });
    app.get("/gone", []() -> std::string { throw fiasco::detail::explicit_error("gone"); });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /gone HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 410);
    REQUIRE(fiasco_test::extract_body(resp) == R"({"detail":"gone"})"
                                               "\n");
}

TEST_CASE("http_error accepts structured JSON detail", "[error_handling]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/structured", []() -> std::string {
        throw fiasco::http_error(422, fiasco::json{{"field", "email"}, {"reason", "invalid"}});
    });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "GET /structured HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 422);
    REQUIRE(fiasco_test::extract_body(resp).find(
                R"("detail":{"field":"email","reason":"invalid"})") != std::string::npos);
}
