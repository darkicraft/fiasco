#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

#include "fiasco/fiasco.hpp"
#include "support/http_client.hpp"
#include "support/test_server.hpp"

TEST_CASE("server_config has stable defaults", "[server][config]") {
    const fiasco::server_config config;

    REQUIRE(config.network.host == "0.0.0.0");
    REQUIRE(config.network.port == 8080);
    REQUIRE(config.network.workers == 0);

    REQUIRE(config.logging.enabled);
    REQUIRE(config.logging.access);
    REQUIRE(config.logging.level == fiasco::log_level::info);
    REQUIRE(config.logging.format == fiasco::log_format::human);
    REQUIRE(config.logging.colors);
    REQUIRE_FALSE(config.logging.sink);
}

TEST_CASE("server can be constructed with default config", "[server][config]") {
    fiasco::server app;
    REQUIRE(app.port() == 0);
}

TEST_CASE("server binds an ephemeral port and serves requests", "[server][boundary]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    server.app().get("/health", []() { return "ok"; });
    server.start();

    REQUIRE(server.port() != 0);

    const auto response = fiasco_test::send_request(
        server.port(), "GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n");

    REQUIRE(fiasco_test::extract_status(response) == 200);
    REQUIRE(fiasco_test::extract_body(response) == "ok\n");
}

TEST_CASE("server can stop before it starts", "[server][lifecycle]") {
    fiasco_test::test_server server;
    server.stop();
    REQUIRE(server.port() == 0);
}
