// HTTP method routing and automatic 404/405 responses.
//
//   cmake --build build --target routing_methods
//   FIASCO_PORT=8080 ./build/examples/routing_methods
//
//   curl localhost:8080/items
//   curl -X POST localhost:8080/items
//   curl -X PUT localhost:8080/items/7
//   curl -X PATCH localhost:8080/items/7
//   curl -X DELETE localhost:8080/items/7
//   curl localhost:8080/missing                  # 404
//   curl -X POST localhost:8080/only-get         # 405

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

int main() {
    fiasco::server app{
        fiasco::server_config{.network = {.port = example_port()}},
    };

    app.get("/items", []() { return "list items"; });
    app.post("/items", []() { return fiasco::json{{"created", true}}; });
    app.put("/items/{id}", [](int id) { return fiasco::json{{"put", id}}; });
    app.patch("/items/{id}", [](int id) { return fiasco::json{{"patched", id}}; });
    app.del("/items/{id}", [](int id) { return fiasco::json{{"deleted", id}}; });
    app.get("/only-get", []() { return "GET only"; });

    app.run();
}
