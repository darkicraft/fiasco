#include <catch2/catch_test_macros.hpp>

#include <string>
#include <variant>

#include "fiasco/contract/request.hpp"
#include "fiasco/contract/response.hpp"
#include "fiasco/contract/result.hpp"

using namespace fiasco::detail;

TEST_CASE("contract owns the request type", "[contract]") {
    request req;
    req.method = string_to_method("GET");
    req.url = "/items/7?q=hello";
    req.reanchor_url_views();

    REQUIRE(req.method == http_method::get);
    REQUIRE(req.path == "/items/7");
    REQUIRE(req.query_string == "q=hello");
    REQUIRE(std::string(method_to_string(req.method)) == "GET");
}

TEST_CASE("contract owns the response type", "[contract]") {
    const auto text = response::text("hello");
    REQUIRE(text.status_code == 200);
    REQUIRE(text.body == "hello\n");
    REQUIRE(text.headers.at("Content-Type") == "text/plain");

    const auto error = response::error("teapot", 418);
    REQUIRE(error.status_code == 418);
    REQUIRE(error.body.find("teapot") != std::string::npos);

    REQUIRE(std::string(response::reason_phrase(404)) == "Not Found");
}

TEST_CASE("contract owns request_result", "[contract]") {
    request_result result = response::text("ok");
    REQUIRE(std::holds_alternative<response>(result));
    REQUIRE(std::get<response>(result).body == "ok\n");
}
