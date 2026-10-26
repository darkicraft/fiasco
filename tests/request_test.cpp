#include <catch2/catch_test_macros.hpp>

#include "fiasco/internal/http/request.hpp"

using namespace fiasco::detail;

// ============================================================
// string_to_method
// ============================================================

TEST_CASE("string_to_method all methods", "[request]") {
    REQUIRE(string_to_method("GET") == http_method::get);
    REQUIRE(string_to_method("POST") == http_method::post);
    REQUIRE(string_to_method("PUT") == http_method::put);
    REQUIRE(string_to_method("DELETE") == http_method::del);
    REQUIRE(string_to_method("PATCH") == http_method::patch);
    REQUIRE(string_to_method("HEAD") == http_method::head);
    REQUIRE(string_to_method("OPTIONS") == http_method::options);
}

TEST_CASE("string_to_method unknown", "[request]") {
    REQUIRE(string_to_method("TRACE") == http_method::unknown);
    REQUIRE(string_to_method("") == http_method::unknown);
    REQUIRE(string_to_method("get") == http_method::unknown);
}

// ============================================================
// method_to_string
// ============================================================

TEST_CASE("method_to_string all methods", "[request]") {
    REQUIRE(std::string(method_to_string(http_method::get)) == "GET");
    REQUIRE(std::string(method_to_string(http_method::post)) == "POST");
    REQUIRE(std::string(method_to_string(http_method::put)) == "PUT");
    REQUIRE(std::string(method_to_string(http_method::del)) == "DELETE");
    REQUIRE(std::string(method_to_string(http_method::patch)) == "PATCH");
    REQUIRE(std::string(method_to_string(http_method::head)) == "HEAD");
    REQUIRE(std::string(method_to_string(http_method::options)) == "OPTIONS");
}

TEST_CASE("method_to_string unknown", "[request]") {
    REQUIRE(std::string(method_to_string(http_method::unknown)) == "UNKNOWN");
}

// ============================================================
// Roundtrip
// ============================================================

TEST_CASE("method roundtrip", "[request]") {
    REQUIRE(string_to_method(method_to_string(http_method::get)) == http_method::get);
    REQUIRE(string_to_method(method_to_string(http_method::post)) == http_method::post);
    REQUIRE(string_to_method(method_to_string(http_method::del)) == http_method::del);
}

// ============================================================
// request header() accessor
// ============================================================

TEST_CASE("request::header finds existing key", "[request]") {
    request req;
    req.headers["Content-Type"] = "application/json";
    REQUIRE(req.header("Content-Type") == "application/json");
}

TEST_CASE("request::header returns empty for missing key", "[request]") {
    request req;
    REQUIRE(req.header("Nonexistent").empty());
}

TEST_CASE("request::header is case-insensitive", "[request]") {
    request req;
    req.headers["Content-Type"] = "application/json";
    REQUIRE(req.header("content-type") == "application/json");
    REQUIRE(req.header("CONTENT-TYPE") == "application/json");
    REQUIRE(req.has_header("content-type"));
    REQUIRE_FALSE(req.has_header("Missing"));
    REQUIRE(req.find_header("Content-Type") != nullptr);
    REQUIRE(req.find_header("Missing") == nullptr);
}

TEST_CASE("request::content_type", "[request]") {
    request req;
    req.headers["Content-Type"] = "text/html";
    REQUIRE(req.content_type() == "text/html");
}

TEST_CASE("request default fields", "[request]") {
    request req;
    REQUIRE(req.method == http_method::unknown);
    REQUIRE(req.url.empty());
    REQUIRE(req.path.empty());
    REQUIRE(req.query_string.empty());
    REQUIRE(req.body.empty());
    REQUIRE(req.path_params.empty());
    REQUIRE(req.ordered_path_params.empty());
}
