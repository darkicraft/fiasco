// Namespace-annotated routes.
//
//   cmake --build build --target annotation_routes
//   FIASCO_PORT=8080 ./build/examples/annotation_routes
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

#include <cstdint>
#include <cstdlib>
#include <string>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

}  // namespace

namespace api {

struct item_input {
    std::string name;
    int quantity = 1;
};

[[= fiasco::get("/")]]
std::string root() {
    return "annotation routing";
}

[[= fiasco::get("/items/{id}",
                {.summary = "Read an item", .operation_id = "read_item"})]]
fiasco::json read_item(long id [[= fiasco::path{.ge = 1}]]) {
    return fiasco::json{{"id", id}, {"name", "item-" + std::to_string(id)}};
}

[[= fiasco::post("/items")]]
fiasco::json create_item(item_input item) {
    return fiasco::json{{"name", item.name}, {"quantity", item.quantity}};
}

// :path captures the remaining path, including slashes.
[[= fiasco::get("/files/{rest:path}")]]
std::string read_file(std::string rest) {
    return "requested: " + rest;
}

}  // namespace api

int main() {
    fiasco::app app;
    app.register_api<^^api>();

    app.print_routes();
    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
