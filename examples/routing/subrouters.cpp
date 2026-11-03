// Sub-routers mounted under path prefixes.
//
//   cmake --build build --target subrouters
//   FIASCO_PORT=8080 ./build/examples/subrouters
//
//   curl localhost:8080/api/v1/status
//   curl localhost:8080/api/v1/users/7
//   curl localhost:8080/admin/health

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

    fiasco::router api("/v1");
    api.get("/status", []() { return "ok"; });
    api.get("/users/{id}", [](int id) { return fiasco::json{{"id", id}}; });

    // A router may itself contain another router.
    fiasco::router users("/users");
    users.get("/", []() { return fiasco::json{{"users", "all"}}; });
    api.include_router(std::move(users), "/v2");
    // GET /api/v1/v2/users

    fiasco::router admin("/admin");
    admin.get("/health", []() { return "healthy"; });

    app.include_router(std::move(api), "/api");
    app.include_router(std::move(admin));

    app.print_routes();
    app.run();
}
