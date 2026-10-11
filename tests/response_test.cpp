#include <catch2/catch_test_macros.hpp>

#include "fiasco/internal/http/response.hpp"

using namespace fiasco::detail;

// ============================================================
// Factory methods
// ============================================================

TEST_CASE("response::empty defaults to 204", "[response]") {
    auto r = response::empty();
    REQUIRE(r.status_code == 204);
    REQUIRE(r.body.empty());
}

TEST_CASE("response::empty with custom status", "[response]") {
    auto r = response::empty(200);
    REQUIRE(r.status_code == 200);
}

TEST_CASE("response::text sets content type and body", "[response]") {
    auto r = response::text("Hello");
    REQUIRE(r.status_code == 200);
    REQUIRE(r.body == "Hello\n");
    REQUIRE(r.headers.at("Content-Type") == "text/plain");
}

TEST_CASE("response::text with custom status", "[response]") {
    auto r = response::text("Created", 201);
    REQUIRE(r.status_code == 201);
}

TEST_CASE("response::json sets content type and body", "[response]") {
    auto r = response::json(R"({"key":"value"})");
    REQUIRE(r.status_code == 200);
    REQUIRE(r.body == "{\"key\":\"value\"}\n");
    REQUIRE(r.headers.at("Content-Type") == "application/json");
}

TEST_CASE("response::json with custom status", "[response]") {
    auto r = response::json("{}", 201);
    REQUIRE(r.status_code == 201);
}

TEST_CASE("response::html sets content type", "[response]") {
    auto r = response::html("<h1>Title</h1>");
    REQUIRE(r.status_code == 200);
    REQUIRE(r.body == "<h1>Title</h1>\n");
    REQUIRE(r.headers.at("Content-Type") == "text/html");
}

TEST_CASE("response::redirect sets Location header", "[response]") {
    auto r = response::redirect("/new-location");
    REQUIRE(r.status_code == 302);
    REQUIRE(r.headers.at("Location") == "/new-location");
}

TEST_CASE("response::redirect with custom status", "[response]") {
    auto r = response::redirect("/permanent", 301);
    REQUIRE(r.status_code == 301);
}

TEST_CASE("response::error creates JSON error body", "[response]") {
    auto r = response::error("Something went wrong", 400);
    REQUIRE(r.status_code == 400);
    REQUIRE(r.headers.at("Content-Type") == "application/json");
    // Body should contain {"detail":"Something went wrong"}
    REQUIRE(r.body.find("Something went wrong") != std::string::npos);
    REQUIRE(r.body.find("detail") != std::string::npos);
}

// ============================================================
// reason_phrase
// ============================================================

TEST_CASE("response::reason_phrase known codes", "[response]") {
    REQUIRE(std::string(response::reason_phrase(200)) == "OK");
    REQUIRE(std::string(response::reason_phrase(201)) == "Created");
    REQUIRE(std::string(response::reason_phrase(204)) == "No Content");
    REQUIRE(std::string(response::reason_phrase(400)) == "Bad Request");
    REQUIRE(std::string(response::reason_phrase(404)) == "Not Found");
    REQUIRE(std::string(response::reason_phrase(405)) == "Method Not Allowed");
    REQUIRE(std::string(response::reason_phrase(422)) == "Unprocessable Entity");
    REQUIRE(std::string(response::reason_phrase(500)) == "Internal Server Error");
}

TEST_CASE("response::reason_phrase unknown code", "[response]") {
    REQUIRE(std::string(response::reason_phrase(999)) == "Unknown");
    REQUIRE(std::string(response::reason_phrase(0)) == "Unknown");
}

// ============================================================
// serialize()
// ============================================================

TEST_CASE("response::serialize basic format", "[response]") {
    auto r = response::text("hi");
    auto s = r.serialize();

    // Should start with status line
    REQUIRE(s.find("HTTP/1.1 200 OK\r\n") == 0);

    // Should contain Content-Type
    REQUIRE(s.find("Content-Type: text/plain\r\n") != std::string::npos);

    // Should contain Content-Length
    REQUIRE(s.find("Content-Length: ") != std::string::npos);

    // Should contain Connection: keep-alive
    REQUIRE(s.find("Connection: keep-alive\r\n") != std::string::npos);

    // Should end with body
    REQUIRE(s.find("hi\n") != std::string::npos);
}

TEST_CASE("response::serialize with custom headers", "[response]") {
    auto r = response::text("test");
    r.headers["X-Custom"] = "value";
    auto s = r.serialize();
    REQUIRE(s.find("X-Custom: value\r\n") != std::string::npos);
}

TEST_CASE("response::serialize no duplicate Content-Length", "[response]") {
    auto r = response::text("body");
    r.headers["Content-Length"] = "4";
    auto s = r.serialize();

    // Count occurrences of Content-Length
    size_t count = 0;
    size_t pos = 0;
    while ((pos = s.find("Content-Length", pos)) != std::string::npos) {
        ++count;
        pos += 14;
    }
    REQUIRE(count == 1);
}

TEST_CASE("response::serialize content-length matches body size", "[response]") {
    auto r = response::text("12345");
    auto s = r.serialize();
    // Body is "12345\n" = 6 bytes
    REQUIRE(s.find("Content-Length: 6\r\n") != std::string::npos);
}

TEST_CASE("response::serialize of redirect has no body", "[response]") {
    auto r = response::redirect("/path");
    auto s = r.serialize();
    // The body is empty for redirect, so Content-Length should be 0
    REQUIRE(s.find("Content-Length: 0\r\n") != std::string::npos);
}

TEST_CASE("response::serialize error response", "[response]") {
    auto r = response::error("not found", 404);
    auto s = r.serialize();
    REQUIRE(s.find("HTTP/1.1 404 Not Found\r\n") == 0);
    REQUIRE(s.find("Content-Type: application/json\r\n") != std::string::npos);
}

TEST_CASE("response::serialize double CRLF separates headers from body", "[response]") {
    auto r = response::text("hello");
    auto s = r.serialize();
    // Headers end with \r\n\r\n before body
    auto header_end = s.find("\r\n\r\n");
    REQUIRE(header_end != std::string::npos);
    // Body should follow
    REQUIRE(s.substr(header_end + 4) == "hello\n");
}

TEST_CASE("response::default status is 200", "[response]") {
    response r;
    REQUIRE(r.status_code == 200);
}

TEST_CASE("response explicit Content-Length overrides auto", "[response]") {
    auto r = response::text("hello");
    r.headers["Content-Length"] = "999";
    auto s = r.serialize();
    // Should use the explicit value, not the real body size
    REQUIRE(s.find("Content-Length: 999\r\n") != std::string::npos);
}

TEST_CASE("response explicit Connection overrides auto", "[response]") {
    auto r = response::empty();
    r.headers["Connection"] = "close";
    auto s = r.serialize();
    REQUIRE(s.find("Connection: close\r\n") != std::string::npos);
    // Should NOT have the default keep-alive
    REQUIRE(s.find("Connection: keep-alive\r\n") == std::string::npos);
}

TEST_CASE("response serialize with no extra headers", "[response]") {
    auto r = response::empty();
    auto s = r.serialize();
    // Should have both auto-added headers
    REQUIRE(s.find("Content-Length: 0\r\n") != std::string::npos);
    REQUIRE(s.find("Connection: keep-alive\r\n") != std::string::npos);
}
