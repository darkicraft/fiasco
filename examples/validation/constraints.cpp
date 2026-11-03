// Pydantic-style field constraints, including pattern and alias.
//
//   cmake --build build --target constraints
//   FIASCO_PORT=8080 ./build/examples/constraints
//
//   curl -X POST localhost:8080/constrained \
//     -H 'Content-Type: application/json' \
//     -d '{
//       "percent": 50,
//       "boxes": 9,
//       "damping": 1.5,
//       "code": "abc",
//       "slug": "hello-world",
//       "tags": ["x"],
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

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

struct constrained_item {
    int percent [[= fiasco::field{.gt = 0, .le = 100}]];
    int boxes [[= fiasco::field{.multiple_of = 3}]] = 3;
    double damping [[= fiasco::field{.ge = 0.0, .lt = 10.0}]] = 0.5;
    std::string code [[= fiasco::field{.min_length = 2, .max_length = 8}]];
    std::string slug [[= fiasco::field{.pattern = "^[a-z0-9-]+$"}]];
    std::vector<int> tags [[= fiasco::field{.min_length = 1, .max_length = 4}]];
    std::string username [[= fiasco::field{.alias = "user-name"}]] = "anon";
};

}  // namespace

int main() {
    fiasco::server app{
        fiasco::server_config{.network = {.port = example_port()}},
    };

    app.post("/constrained", [](constrained_item input) { return input; });

    app.run();
}
