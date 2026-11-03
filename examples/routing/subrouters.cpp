// Mounting routers under prefixes.
//
//   cmake --build build --target subrouters
//   FIASCO_PORT=8080 ./build/examples/subrouters
//
//   curl localhost:8080/api/v1/status
//   curl localhost:8080/api/v1/users/42
//   curl localhost:8080/api/v1/health
//
// The same routes can also be printed at startup with app.print_routes().

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

    fiasco::router users;
    users.get("/{id}", [](int id) { return fiasco::json{{"user_id", id}}; });

    fiasco::router v1("/v1");
    v1.get("/status", []() { return fiasco::json{{"status", "ok"}}; });
    v1.get("/health", []() { return "healthy"; });
    v1.include_router(std::move(users), "/users");

    // Prefixes compose: "/api" + "/v1" + "/users/{id}".
    app.include_router(std::move(v1), "/api");

    app.print_routes();
    app.run();
}
