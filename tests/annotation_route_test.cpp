#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "fiasco/fiasco.hpp"

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

namespace annotation_routes {

[[= fiasco::get("/annotation/hello")]]
std::string hello() {
    return "hello";
}

[[= fiasco::post("/annotation/echo")]]
std::string echo(std::string name) {
    return name;
}

[[= fiasco::get("/annotation/custom", {.summary = "custom", .status_code = 201})]]
fiasco::response custom() {
    return fiasco::response::text("created", 201);
}

[[= fiasco::get("/annotation/converted/{id:int}")]]
long converted(long id) {
    return id;
}

[[= fiasco::get("/annotation/files/{rest:path}")]]
std::string files(std::string rest) {
    return rest;
}

}  // namespace annotation_routes

TEST_CASE("annotation routes register with register_api", "[routing][annotations]") {
    fiasco::app app;
    app.register_api<^^annotation_routes>();

    fiasco::test_client client{app};

    const auto hello = client.handle(make_request("GET", "/annotation/hello"));
    REQUIRE(hello.status_code == 200);
    REQUIRE(hello.body == "hello\n");

    const auto echo = client.handle(make_request("POST", "/annotation/echo?name=ada"));
    REQUIRE(echo.status_code == 200);
    REQUIRE(echo.body == "ada\n");

    const auto custom = client.handle(make_request("GET", "/annotation/custom"));
    REQUIRE(custom.status_code == 201);
    REQUIRE(custom.body == "created\n");

    const auto converted =
        client.handle(make_request("GET", "/annotation/converted/42"));
    REQUIRE(converted.status_code == 200);
    REQUIRE(converted.body == "42\n");

    const auto bad_converted =
        client.handle(make_request("GET", "/annotation/converted/nope"));
    REQUIRE(bad_converted.status_code == 404);

    const auto files =
        client.handle(make_request("GET", "/annotation/files/a/b.txt"));
    REQUIRE(files.status_code == 200);
    REQUIRE(files.body == "a/b.txt\n");
}
