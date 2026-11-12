#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fiasco/fiasco.hpp"

namespace openapi_routes {

struct dimensions {
    int width;
    int height;
};

struct item_input {
    std::string name [[= fiasco::field{.min_length = 2}]];
    dimensions size;
    std::optional<int> stock;
    std::vector<std::string> tags;
};

struct item_output {
    std::string name;
    int quantity;
};

[[= fiasco::get("/items/{id}",
                {.summary = "Fetch item", .operation_id = "fetch_item"})]]
std::string fetch(long id [[= fiasco::path{.ge = 1}]],
                  std::optional<int> page [[= fiasco::query{.le = 10}]],
                  fiasco::request req) {
    (void)page;
    (void)req;
    return std::to_string(id);
}

[[= fiasco::post("/items",
                 {.summary = "Create item",
                  .operation_id = "create_item",
                  .status_code = 201})]]
item_output create(item_input item) {
    return item_output{.name = item.name, .quantity = 1};
}

[[= fiasco::post("/batch")]]
std::string batch(item_input first, dimensions second) {
    return first.name + std::to_string(second.width);
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
    CHECK(responses["200"]["content"].contains("text/plain"));

    REQUIRE(paths.contains("/items"));
    const auto create_operation = paths["/items"]["post"];
    CHECK(create_operation["summary"] == "Create item");
    CHECK(create_operation["operationId"] == "create_item");

    const auto request_body = create_operation["requestBody"];
    CHECK(request_body["required"] == true);
    const auto request_schema =
        request_body["content"]["application/json"]["schema"];
    const auto components = document["components"]["schemas"];
    const std::string prefix = "#/components/schemas/";
    const auto request_ref = request_schema["$ref"].get<std::string>();
    REQUIRE(request_ref.starts_with(prefix));
    const auto input_schema = components[request_ref.substr(prefix.size())];
    CHECK(input_schema["properties"]["name"]["type"] == "string");
    CHECK(input_schema["properties"]["name"]["minLength"] == 2.0);
    CHECK(input_schema["properties"]["size"]["$ref"].get<std::string>().starts_with(prefix));
    CHECK(input_schema["properties"]["tags"]["type"] == "array");
    CHECK(input_schema["properties"]["tags"]["items"]["type"] == "string");
    CHECK(input_schema["properties"]["stock"]["anyOf"].size() == 2);
    CHECK(input_schema["properties"]["stock"]["anyOf"][1]["type"] == "null");

    const auto created = create_operation["responses"]["201"];
    CHECK(created["content"].contains("application/json"));
    const auto response_ref =
        created["content"]["application/json"]["schema"]["$ref"].get<std::string>();
    REQUIRE(response_ref.starts_with(prefix));
    const auto output_schema = components[response_ref.substr(prefix.size())];
    CHECK(output_schema["properties"]["name"]["type"] == "string");
    CHECK(output_schema["properties"]["quantity"]["type"] == "integer");
    CHECK(output_schema["required"].size() == 2);

    const auto batch_schema =
        paths["/batch"]["post"]["requestBody"]["content"]["application/json"]["schema"];
    CHECK(batch_schema["type"] == "object");
    CHECK(batch_schema["properties"]["first"]["$ref"].get<std::string>().starts_with(prefix));
    CHECK(batch_schema["properties"]["second"]["$ref"].get<std::string>().starts_with(prefix));
    CHECK(batch_schema["required"].size() == 2);
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
