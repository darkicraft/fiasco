// Pydantic-style field constraints, including pattern and alias.
//
//   cmake --build build --target constraints
//   ./build/examples/validation/constraints
//
//   curl -X POST localhost:8080/constrained \
//     -H 'Content-Type: application/json' \
//     -d '{
//       "percent": 50,
//       "boxes": 9,
//       "damping": 1.5,
//       "code": "abc",
//       "slug": "hello-world",
//       "tags": [1],
//       "user-name": "ada"
//     }'
//
//   curl -X POST localhost:8080/constrained \
//     -H 'Content-Type: application/json' \
//     -d '{
//       "percent": 0,
//       "boxes": 4,
//       "damping": 12.0,
//       "code": "a",
//       "slug": "Hello World",
//       "tags": []
//     }'
//
// The second request returns one FastAPI-style 422 containing all failures.

#include <fiasco/fiasco.hpp>

#include <string>
#include <vector>

using fiasco::field;
using fiasco::post;

namespace {

struct constrained_item {
    int percent [[=field{.gt = 0, .le = 100}]];
    int boxes [[=field{.multiple_of = 3}]] = 3;
    double damping [[=field{.ge = 0.0, .lt = 10.0}]] = 0.5;
    std::string code [[=field{.min_length = 2, .max_length = 8}]];
    std::string slug [[=field{.pattern = "^[a-z0-9-]+$"}]];
    std::vector<int> tags [[=field{.min_length = 1, .max_length = 4}]];
    std::string username [[=field{.alias = "user-name"}]] = "anon";
};

}  // namespace

namespace routes {

[[=post("/constrained")]]
auto constrained(constrained_item input) -> constrained_item {
    return input;
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
