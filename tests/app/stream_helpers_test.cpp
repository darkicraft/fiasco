#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>

#include "fiasco/fiasco.hpp"
#include "support/test_routes.hpp"

namespace {

struct json_line {
    int id = 0;
    std::string name;
};

fiasco::stream<fiasco::sse_event> event_source() {
    co_yield fiasco::sse_event{.data = "first"};
    co_yield fiasco::sse_event{.data = "second", .event = "done", .id = "2"};
    co_return;
}

fiasco::stream<json_line> line_source() {
    co_yield json_line{1, "one"};
    co_yield json_line{2, "two"};
    co_return;
}

fiasco::request make_request(std::string path) {
    fiasco::request req;
    req.method = fiasco::detail::http_method::get;
    req.url = std::move(path);
    req.reanchor_url_views();
    return req;
}

}  // namespace

TEST_CASE("SSE events serialize their fields", "[stream][sse]") {
    const fiasco::sse_event event{
        .data = "one\ntwo",
        .event = "tick",
        .id = "7",
        .retry = 1000,
    };

    CHECK(fiasco::serialize_sse_event(event) ==
          "id: 7\nevent: tick\nretry: 1000\ndata: one\ndata: two\n\n");
}

TEST_CASE("SSE events reject invalid field values", "[stream][sse]") {
    REQUIRE_THROWS_AS(
        fiasco::serialize_sse_event(fiasco::sse_event{.data = "x", .event = "bad\nname"}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        fiasco::serialize_sse_event(fiasco::sse_event{.data = "x", .id = "bad\rname"}),
        std::invalid_argument);
}

TEST_CASE("sse helper builds an event-stream response", "[stream][sse]") {
    fiasco::app app;
    fiasco_test::get(
        app, "/events", []() -> fiasco::response { return fiasco::sse(event_source()); });

    fiasco::test_client client{app};
    const auto res = client.handle(make_request("/events"));

    REQUIRE(res.status_code == 200);
    CHECK(res.headers.at("Content-Type") == "text/event-stream");
    CHECK(res.headers.at("Cache-Control") == "no-cache");
    CHECK(res.body == "data: first\n\nid: 2\nevent: done\ndata: second\n\n");
}

TEST_CASE("json_lines helper emits one JSON document per line", "[stream][json_lines]") {
    fiasco::app app;
    fiasco_test::get(
        app, "/lines", []() -> fiasco::response { return fiasco::json_lines(line_source()); });

    fiasco::test_client client{app};
    const auto res = client.handle(make_request("/lines"));

    REQUIRE(res.status_code == 200);
    CHECK(res.headers.at("Content-Type") == "application/x-ndjson");
    CHECK(res.body == R"({"id":1,"name":"one"}
{"id":2,"name":"two"}
)");
}
