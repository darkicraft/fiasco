#include <catch2/catch_test_macros.hpp>

#include <string>

#include "fiasco/fiasco.hpp"
#include "support/http_client.hpp"
#include "support/test_server.hpp"

TEST_CASE("server handles sequential requests on one connection", "[http][keep-alive]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/one", []() { return "one"; });
    app.get("/two", []() { return "two"; });
    server.start();

    fiasco_test::http_connection connection(server.port());
    connection.send("GET /one HTTP/1.1\r\nHost: localhost\r\n\r\n");
    const auto first = connection.read_response();
    REQUIRE(fiasco_test::extract_status(first) == 200);
    REQUIRE(fiasco_test::extract_body(first) == "one\n");
    REQUIRE(first.find("Connection: keep-alive\r\n") != std::string::npos);

    connection.send("GET /two HTTP/1.1\r\nHost: localhost\r\n\r\n");
    const auto second = connection.read_response();
    REQUIRE(fiasco_test::extract_status(second) == 200);
    REQUIRE(fiasco_test::extract_body(second) == "two\n");
}

TEST_CASE("server closes the connection when requested", "[http][connection]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/one", []() { return "one"; });
    server.start();

    fiasco_test::http_connection connection(server.port());
    connection.send("GET /one HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");

    const auto response = connection.read_response();
    REQUIRE(fiasco_test::extract_status(response) == 200);
    REQUIRE(response.find("Connection: close\r\n") != std::string::npos);
    REQUIRE(connection.wait_for_eof());
}

TEST_CASE("server closes the connection when the response says close", "[http][connection]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/close", []() {
        auto response = fiasco::response::text("bye");
        response.headers["Connection"] = "close";
        return response;
    });
    server.start();

    fiasco_test::http_connection connection(server.port());
    connection.send("GET /close HTTP/1.1\r\nHost: localhost\r\n\r\n");

    const auto response = connection.read_response();
    REQUIRE(fiasco_test::extract_status(response) == 200);
    REQUIRE(fiasco_test::extract_body(response) == "bye\n");
    REQUIRE(response.find("Connection: close\r\n") != std::string::npos);
    REQUIRE(connection.wait_for_eof());
}
