// Reflected request and response models: no macros, no boilerplate.
//
//   cmake --build build --target body_model
//   ./build/examples/body_model
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

#include <string>

using fiasco::post;

namespace {

struct create_user {
    std::string username;
    int age;
};

struct user_view {
    std::string username;
    int age;
};

}  // namespace

namespace routes {

[[=post("/users")]]
auto create_user_route(create_user input) -> user_view {
    return {input.username, input.age + 1};
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
