#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <string_view>

#include "fiasco/fiasco.hpp"

namespace openapi_routes {

[[= fiasco::get("/items/{id:int}",
                {.summary = "Fetch item", .operation_id = "fetch_item"})]]
std::string fetch(long id [[= fiasco::path{.ge = 1}]],
                  std::optional<int> page [[= fiasco::query{.le = 10}]],
                  fiasco::request req) {
    (void)page;
    (void)req;
    return std::to_string(id);
}

}  // namespace openapi_routes

namespace {

fiasco::request make_request(std::string_view method, std::string url) {
    fiasco::request req;
    req.method = fiasco::detail::string_to_method(std::string(method));
    req.url = std::move(url);
    req.reanchor_url_views();
    return req;
}

}  // namespace

TEST_CASE("openapi document projects route descriptors", "[openapi]") {
    fiasco::app_options options;
    options.title = "Descriptor API";
    options.version = "9.9.9";
    options.description = "Generated from descriptors.";

    fiasco::app app{options};
    app.register_api<^^openapi_routes>();

    const auto document = app.openapi();
    CHECK(document["openapi"] == "3.1.0");
    CHECK(document["info"]["title"] == "Descriptor API");
    CHECK(document["info"]["version"] == "9.9.9");
    CHECK(document["info"]["description"] == "Generated from descriptors.");

    const auto paths = document["paths"];
    REQUIRE(paths.contains("/items/{id}"));

    const auto operation = paths["/items/{id}"]["get"];
    CHECK(operation["summary"] == "Fetch item");
    CHECK(operation["operationId"] == "fetch_item");

    const auto parameters = operation["parameters"];
    REQUIRE(parameters.size() == 2);

    const auto id = parameters[0];
    CHECK(id["name"] == "id");
    CHECK(id["in"] == "path");
    CHECK(id["required"] == true);
    CHECK(id["schema"]["type"] == "integer");
    CHECK(id["schema"]["minimum"] == 1.0);

    const auto page = parameters[1];
    CHECK(page["name"] == "page");
    CHECK(page["in"] == "query");
    CHECK(page["required"] == false);
    CHECK(page["schema"]["type"] == "integer");
    CHECK(page["schema"]["maximum"] == 10.0);

    const auto responses = operation["responses"];
    REQUIRE(responses.contains("200"));
    CHECK(responses["200"]["description"] == "Successful Response");
}

TEST_CASE("openapi document is served at app_options.openapi_url", "[openapi]") {
    fiasco::app app;
    app.register_api<^^openapi_routes>();

    fiasco::test_client client{app};
    const auto response = client.handle(make_request("GET", "/openapi.json"));

    REQUIRE(response.status_code == 200);
    const auto document = fiasco::json::parse(response.body);
    CHECK(document["openapi"] == "3.1.0");
    CHECK(document["paths"].contains("/items/{id}"));
}

TEST_CASE("openapi route can be disabled", "[openapi]") {
    fiasco::app_options options;
    options.openapi_url.clear();

    fiasco::app app{options};
    fiasco::test_client client{app};

    const auto response = client.handle(make_request("GET", "/openapi.json"));
    CHECK(response.status_code == 404);
}
