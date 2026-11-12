// Synchronous and asynchronous handlers.
//
//   cmake --build build --target async_handlers
//   FIASCO_PORT=8080 ./build/examples/async_handlers
//
//   curl localhost:8080/sync
//   curl localhost:8080/async
//   curl localhost:8080/async/typed
//   curl -i localhost:8080/async/custom
//
// A normal lambda stays synchronous. Add `-> fiasco::async` only when the
// handler needs to co_await. Use `fiasco::async_t<T>` when you also want the
// response model inferred for OpenAPI.

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

struct user {
    std::string name;
    int age;
};

user load_user() {
    return {"ada", 36};
}

}  // namespace

int main() {
    fiasco::app app;

    // No coroutine machinery: the original synchronous fast path.
    app.get("/sync", []() { return load_user(); });

    // A coroutine that never suspends still uses the semantic async type.
    app.get("/async", []() -> fiasco::async { co_return load_user(); });

    // Typed async keeps the response model in the signature for OpenAPI.
    app.get("/async/typed", []() -> fiasco::async_t<user> { co_return load_user(); });

    // Advanced escape hatch: full control over status and headers.
    app.get("/async/custom", []() -> fiasco::async {
        auto response = fiasco::response::json(R"({"created":true})");
        response.status_code = 201;
        response.headers["X-Async"] = "yes";
        co_return response;
    });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
