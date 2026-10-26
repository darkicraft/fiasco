#include <catch2/catch_test_macros.hpp>

#include <string>

#include "fiasco/fiasco.hpp"
#include "support/http_client.hpp"
#include "support/test_server.hpp"

namespace fiasco::detail {

struct SumInput {
    int a = 0;
    int b = 0;
};

// No default member initializers => both required (exact Pydantic 1:1).
struct RequiredInput {
    int a;
    int b;
};

struct ConstrainedInput {
    [[= field{.gt = 0, .le = 10}]] int a = 0;
};

struct ExtraInput {
    int c = 0;
};

}  // namespace fiasco::detail

// -------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------

TEST_CASE("integration GET returns 200", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/hello", []() -> std::string { return "world"; });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(fiasco_test::extract_body(resp) == "world\n");
}

TEST_CASE("integration 404 not found", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/exists", []() -> std::string { return "ok"; });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "GET /nonexistent HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 404);
    REQUIRE(fiasco_test::extract_body(resp).find("Not Found") != std::string::npos);
}

TEST_CASE("integration 405 method not allowed", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/resource", []() -> std::string { return "ok"; });

    server.start();

    auto resp = fiasco_test::send_request(
        server.port(), "POST /resource HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 405);
}

TEST_CASE("integration invalid JSON body returns 422", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.post("/sum", [](const fiasco::detail::SumInput&) -> std::string { return ""; });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "POST /sum HTTP/1.1\r\n"
                                          "Host: localhost\r\n"
                                          "Content-Type: application/json\r\n"
                                          "Content-Length: 14\r\n"
                                          "\r\n"
                                          "{invalid json}");
    REQUIRE(fiasco_test::extract_status(resp) == 422);
    REQUIRE(fiasco_test::extract_body(resp).find("JSON decode error") != std::string::npos);
}

TEST_CASE("integration 500 internal error from handler", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/crash", []() -> std::string { throw std::runtime_error("boom"); });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /crash HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 500);
}

TEST_CASE("integration path parameters", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/users/{uid}", [](int uid) -> std::string { return "user:" + std::to_string(uid); });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "GET /users/42 HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(fiasco_test::extract_body(resp) == "user:42\n");
}

TEST_CASE("integration JSON body deserialization", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.post("/sum", [](const fiasco::detail::SumInput& input) -> std::string {
        return std::to_string(input.a + input.b);
    });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "POST /sum HTTP/1.1\r\n"
                                          "Host: localhost\r\n"
                                          "Content-Type: application/json\r\n"
                                          "Content-Length: 15\r\n"
                                          "\r\n"
                                          R"({"a":10,"b":20})");
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(fiasco_test::extract_body(resp) == "30\n");
}

TEST_CASE("integration POST empty body with JSON model returns 422", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.post("/sum", [](const fiasco::detail::SumInput&) -> std::string { return ""; });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "POST /sum HTTP/1.1\r\n"
                                          "Host: localhost\r\n"
                                          "Content-Type: application/json\r\n"
                                          "Content-Length: 0\r\n"
                                          "\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 422);
    REQUIRE(fiasco_test::extract_body(resp).find(R"("loc":["body"])") != std::string::npos);
    REQUIRE(fiasco_test::extract_body(resp).find(R"("msg":"Field required")") != std::string::npos);
}

TEST_CASE("integration POST missing field returns 422 with the field name", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.post("/sum", [](const fiasco::detail::RequiredInput&) -> std::string { return ""; });

    server.start();

    // "b" is missing
    auto resp = fiasco_test::send_request(server.port(),
                                          "POST /sum HTTP/1.1\r\n"
                                          "Host: localhost\r\n"
                                          "Content-Type: application/json\r\n"
                                          "Content-Length: 7\r\n"
                                          "\r\n"
                                          R"({"a":1})");
    REQUIRE(fiasco_test::extract_status(resp) == 422);
    REQUIRE(fiasco_test::extract_body(resp).find(R"("loc":["body","b"])") != std::string::npos);
    REQUIRE(fiasco_test::extract_body(resp).find(R"("msg":"Field required")") != std::string::npos);
}

TEST_CASE("integration POST constraint violation returns Pydantic 422 with ctx", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.post("/c", [](const fiasco::detail::ConstrainedInput&) -> std::string { return ""; });

    server.start();

    auto resp = fiasco_test::send_request(server.port(),
                                          "POST /c HTTP/1.1\r\n"
                                          "Host: localhost\r\n"
                                          "Content-Type: application/json\r\n"
                                          "Content-Length: 7\r\n"
                                          "\r\n"
                                          R"({"a":0})");
    REQUIRE(fiasco_test::extract_status(resp) == 422);
    REQUIRE(fiasco_test::extract_body(resp).find(R"("type":"greater_than")") != std::string::npos);
    REQUIRE(fiasco_test::extract_body(resp).find(R"("loc":["body","a"])") != std::string::npos);
    REQUIRE(fiasco_test::extract_body(resp).find(R"("ctx":{"gt":0})") != std::string::npos);
}

TEST_CASE("integration query parameter", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/greet", [](std::string name) -> std::string { return "hi " + name; });

    server.start();

    auto resp = fiasco_test::send_request(
        server.port(), "GET /greet?name=alice HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(fiasco_test::extract_body(resp) == "hi alice\n");
}

TEST_CASE("integration missing query parameter returns 422", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.get("/greet", [](std::string name) -> std::string { return "hi " + name; });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /greet HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 422);
    REQUIRE(fiasco_test::extract_body(resp).find(R"("loc":["query","name"])") != std::string::npos);
}

TEST_CASE("integration path params bind by name, not position", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    // parameter order is the reverse of the path placeholders
    app.get("/{a}/{b}", [](int b, int a) -> std::string {
        return "a=" + std::to_string(a) + " b=" + std::to_string(b);
    });

    server.start();

    auto resp =
        fiasco_test::send_request(server.port(), "GET /10/20 HTTP/1.1\r\nHost: localhost\r\n\r\n");
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(fiasco_test::extract_body(resp) == "a=10 b=20\n");
}

TEST_CASE("integration multiple body params are embedded by name", "[integration]") {
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.post("/embed", [](fiasco::detail::SumInput s, fiasco::detail::ExtraInput o) -> std::string {
        return std::to_string(s.a + s.b + o.c);
    });

    server.start();

    const std::string body = R"({"s":{"a":1,"b":2},"o":{"c":3}})";
    auto resp = fiasco_test::send_request(server.port(),
                                          "POST /embed HTTP/1.1\r\nHost: localhost\r\n"
                                          "Content-Type: application/json\r\nContent-Length: " +
                                              std::to_string(body.size()) + "\r\n\r\n" + body);
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(fiasco_test::extract_body(resp) == "6\n");
}

TEST_CASE("integration path scalar + body model is not embedded", "[integration]") {
    // A scalar (std::string) path param must not count as a body param, so the
    // single model still receives the whole body rather than body[label].
    fiasco_test::test_server server{
        fiasco::server_config{.logging = {.enabled = false}},
    };
    auto& app = server.app();
    app.post("/sum/{label}", [](std::string label, fiasco::detail::SumInput input) -> std::string {
        return label + ":" + std::to_string(input.a + input.b);
    });

    server.start();

    const std::string body = R"({"a":10,"b":20})";
    auto resp = fiasco_test::send_request(server.port(),
                                          "POST /sum/x HTTP/1.1\r\nHost: localhost\r\n"
                                          "Content-Type: application/json\r\nContent-Length: " +
                                              std::to_string(body.size()) + "\r\n\r\n" + body);
    REQUIRE(fiasco_test::extract_status(resp) == 200);
    REQUIRE(fiasco_test::extract_body(resp) == "x:30\n");
}
