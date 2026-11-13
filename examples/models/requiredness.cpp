// Pydantic's required-vs-nullable rules, exactly.
//
//   cmake --build build --target requiredness
//   ./build/examples/models/requiredness
//
//   curl -X POST localhost:8080/requiredness \
//     -H 'Content-Type: application/json' \
//     -d '{"a":1,"c":null}'                    # valid: b defaults, c is null
//
//   curl -X POST localhost:8080/requiredness \
//     -H 'Content-Type: application/json' \
//     -d '{"a":1,"b":2,"c":3,"d":4}'           # valid: everything present
//
//   curl -X POST localhost:8080/requiredness \
//     -H 'Content-Type: application/json' \
//     -d '{"c":null}'                          # 422: a is missing
//
//   curl -X POST localhost:8080/requiredness \
//     -H 'Content-Type: application/json' \
//     -d '{"a":null,"c":null}'                 # 422: a is not nullable

#include <fiasco/fiasco.hpp>

#include <optional>

using fiasco::post;

namespace {

struct requiredness_model {
    int a;                                // required, not nullable
    int b = 0;                            // optional, not nullable
    std::optional<int> c;                 // required, nullable
    std::optional<int> d = std::nullopt;  // optional, nullable
};

}  // namespace

namespace routes {

[[=post("/requiredness")]]
auto requiredness(requiredness_model input) -> requiredness_model {
    return input;
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
