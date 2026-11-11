// Multiple body parameters are embedded by parameter name, like FastAPI.
//
//   cmake --build build --target multiple_bodies
//   FIASCO_PORT=8080 ./build/examples/multiple_bodies
//
//   curl -X POST localhost:8080/search \
//     -H 'Content-Type: application/json' \
//     -d '{"request":{"query":"cats"},"options":{"limit":10}}'
//
// The body is one JSON object whose keys are the parameter names:
//   request -> search_request
//   options -> search_options

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

struct search_request {
    std::string query;
};

struct search_options {
    int limit = 10;
};

}  // namespace

int main() {
    fiasco::app app;

    app.post("/search", [](search_request request, search_options options) {
        return fiasco::json{
            {"query", request.query},
            {"limit", options.limit},
        };
    });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
