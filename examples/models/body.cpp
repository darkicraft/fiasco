// Reflected request and response models: no macros, no boilerplate.
//
//   cmake --build build --target body_model
//   FIASCO_PORT=8080 ./build/examples/body_model
//
//   curl -X POST localhost:8080/users \
//     -H 'Content-Type: application/json' \
//     -d '{"username":"ada","age":36}'
//
//   curl -X POST localhost:8080/users \
//     -H 'Content-Type: application/json' \
//     -d '{"username":"ada"}'                    # 422: age is required
//
// Any aggregate struct is a model automatically. JSON keys are member names.

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

struct create_user {
    std::string username;
    int age;
};

struct user_view {
    std::string username;
    int age;
};

}  // namespace

int main() {
    fiasco::app app;

    app.post("/users", [](create_user input) -> user_view {
        return {input.username, input.age + 1};
    });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
