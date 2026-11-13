// OpenAPI metadata from app_options, router_options, and route annotations.
//
//   cmake --build build --target openapi
//   ./build/examples/openapi/basic
//
//   curl localhost:8080/items/7
//   curl localhost:8080/items/7/raw
//   curl -X POST localhost:8080/items -H 'Content-Type: application/json' \
//        -d '{"name":"widget","quantity":3}'
//   curl localhost:8080/openapi.json
//   curl localhost:8080/docs
//   curl localhost:8080/redoc
//
// The document is generated from the registered route descriptors. The
// /openapi.json route is served automatically unless openapi_url is cleared.

#include <fiasco/fiasco.hpp>

#include <optional>
#include <string>

using fiasco::get;
using fiasco::json;
using fiasco::path;
using fiasco::post;
using fiasco::query;
using fiasco::response;

namespace catalog {

struct item {
    std::string name;
    int quantity = 1;
};

[[=get("/items/{id}",
        {.summary = "Read an item",
         .description = "Returns the item for the given identifier.",
         .operation_id = "read_item"})]]
auto read_item(long id [[=path{.ge = 1}]], std::optional<int> fields [[=query{.ge = 1, .le = 10}]])
        -> item {
    (void)fields;
    return item{.name = "item-" + std::to_string(id), .quantity = 1};
}

[[=post("/items",
         {.summary = "Create an item",
          .description = "Creates an item from the JSON request body.",
          .operation_id = "create_item",
          .status_code = 201})]]
auto create_item(item input) -> item {
    return input;
}

// `response` is opaque to schema inference; use `.response_model`.
[[=get("/items/{id}/raw", {.response_model = ^^item})]]
auto raw_item(long id) -> response {
    return response::json(json{{"name", "item-" + std::to_string(id)}, {"quantity", 1}}.dump());
}

}  // namespace catalog

int main() {
    fiasco::app_options options;
    options.title = "Catalog API";
    options.version = "0.1.0";
    options.description = "A small API used to demonstrate OpenAPI projection.";
    options.contact.name = "Fiasco";
    options.contact.url = "https://github.com/darkicraft/fiasco";
    options.license.name = "MIT";
    options.swagger_ui_parameters = json{{"syntaxHighlight", false}};

    fiasco::router_options router_options;
    router_options.tags = {"catalog"};

    fiasco::app app{options, router_options};
    app.register_api<^^catalog>();

    app.print_routes();
    app.run();
}
