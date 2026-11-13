// HTTP method routing and automatic 404/405 responses.
//
//   cmake --build build --target routing_methods
//   ./build/examples/routing_methods
//
//   curl localhost:8080/items
//   curl -X POST localhost:8080/items
//   curl -X PUT localhost:8080/items/7
//   curl -X PATCH localhost:8080/items/7
//   curl -X DELETE localhost:8080/items/7
//   curl localhost:8080/missing                  # 404
//   curl -X POST localhost:8080/only-get         # 405

#include <fiasco/fiasco.hpp>

#include <string>

using fiasco::del;
using fiasco::get;
using fiasco::json;
using fiasco::patch;
using fiasco::post;
using fiasco::put;

namespace routes {

[[=get("/items")]]
auto list_items() -> std::string {
    return "list items";
}

[[=post("/items")]]
auto create_item() -> json {
    return json{{"created", true}};
}

[[=put("/items/{id}")]]
auto put_item(int id) -> json {
    return json{{"put", id}};
}

[[=patch("/items/{id}")]]
auto patch_item(int id) -> json {
    return json{{"patched", id}};
}

[[=del("/items/{id}")]]
auto delete_item(int id) -> json {
    return json{{"deleted", id}};
}

[[=get("/only-get")]]
auto only_get() -> std::string {
    return "GET only";
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
