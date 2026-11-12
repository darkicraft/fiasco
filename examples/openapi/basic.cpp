// OpenAPI metadata from app_options, router_options, and route annotations.
//
//   cmake --build build --target openapi
//   FIASCO_PORT=8080 ./build/examples/openapi
//
//   curl localhost:8080/items/7
//   curl localhost:8080/openapi.json
//
// The document is generated from the registered route descriptors. The
// /openapi.json route is served automatically unless openapi_url is cleared.

#include <fiasco/fiasco.hpp>

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

}  // namespace

namespace catalog {

struct item {
    std::string name;
    int quantity = 1;
};

[[= fiasco::get("/items/{id}",
                {.summary = "Read an item",
                 .description = "Returns the item for the given identifier.",
                 .operation_id = "read_item"})]]
item read_item(long id [[= fiasco::path{.ge = 1}]],
               std::optional<int> fields [[= fiasco::query{.ge = 1, .le = 10}]]) {
    (void)fields;
    return item{.name = "item-" + std::to_string(id), .quantity = 1};
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

    fiasco::router_options router_options;
    router_options.tags = {"catalog"};

    fiasco::app app{options, router_options};
    app.register_api<^^catalog>();

    app.print_routes();
    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
