// Pydantic's required-vs-nullable rules, exactly.
//
//   cmake --build build --target requiredness
//   FIASCO_PORT=8080 ./build/examples/requiredness
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

#include <cstdint>
#include <cstdlib>
#include <optional>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

struct requiredness_model {
    int a;                                // required, not nullable
    int b = 0;                            // optional, not nullable
    std::optional<int> c;                 // required, nullable
    std::optional<int> d = std::nullopt;  // optional, nullable
};

}  // namespace

int main() {
    fiasco::app app;

    app.post("/requiredness", [](requiredness_model input) { return input; });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
