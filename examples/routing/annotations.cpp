// Namespace-annotated routes.
//
//   cmake --build build --target annotation_routes
//   ./build/examples/annotation_routes
//
//   curl localhost:8080/
//   curl localhost:8080/items/42
//   curl -X POST localhost:8080/items -H 'Content-Type: application/json' \
//        -d '{"name":"widget","quantity":3}'
//   curl localhost:8080/files/reports/2026.txt
//   curl localhost:8080/openapi.json
//
// Handlers are ordinary free functions in a namespace. `register_api<^^ns>()`
// reads their annotations and registers them with the app.

#include <fiasco/fiasco.hpp>

#include <string>

using fiasco::get;
using fiasco::json;
using fiasco::path;
using fiasco::post;

namespace api {

struct item_input {
    std::string name;
    int quantity = 1;
};

[[=get("/")]]
auto root() -> std::string {
    return "annotation routing";
}

[[=get("/items/{id}", {.summary = "Read an item", .operation_id = "read_item"})]]
auto read_item(
    long id [[=path{.ge = 1}]]) -> json {
    return json{{"id", id}, {"name", "item-" + std::to_string(id)}};
}

[[=post("/items")]]
auto create_item(item_input item) -> json {
    return json{{"name", item.name}, {"quantity", item.quantity}};
}

// :path captures the remaining path, including slashes.
[[=get("/files/{rest:path}")]]
auto read_file(std::string rest) -> std::string {
    return "requested: " + rest;
}

}  // namespace api

int main() {
    fiasco::app app;
    app.register_api<^^api>();

    app.print_routes();
    app.run();
}
