// Sub-routers mounted under path prefixes.
//
//   cmake --build build --target subrouters
//   ./build/examples/subrouters
//
//   curl localhost:8080/api/v1/status
//   curl localhost:8080/api/v1/users/7
//   curl localhost:8080/admin/health

#include <fiasco/fiasco.hpp>

#include <string>

using fiasco::get;
using fiasco::json;

namespace api_routes {

[[=get("/status")]]
auto status() -> std::string {
    return "ok";
}

[[=get("/users/{id}")]]
auto user(int id) -> json {
    return json{{"id", id}};
}

}  // namespace api_routes

namespace user_routes {

[[=get("/")]]
auto all_users() -> json {
    return json{{"users", "all"}};
}

}  // namespace user_routes

namespace admin_routes {

[[=get("/health")]]
auto health() -> std::string {
    return "healthy";
}

}  // namespace admin_routes

int main() {
    fiasco::app app;

    fiasco::router api("/v1");
    api.register_api<^^api_routes>();

    // A router may itself contain another router.
    fiasco::router users("/users");
    users.register_api<^^user_routes>();
    api.include_router(std::move(users), "/v2");
    // GET /api/v1/v2/users/

    fiasco::router admin("/admin");
    admin.register_api<^^admin_routes>();

    app.include_router(std::move(api), "/api");
    app.include_router(std::move(admin));

    app.print_routes();
    app.run();
}
