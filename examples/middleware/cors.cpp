// CORS middleware.
//
//   cmake --build build --target cors
//   FIASCO_PORT=8080 ./build/examples/cors
//
//   curl -i localhost:8080/hello -H 'Origin: https://example.com'
//
//   curl -i -X OPTIONS localhost:8080/hello \
//     -H 'Origin: https://example.com' \
//     -H 'Access-Control-Request-Method: POST' \
//     -H 'Access-Control-Request-Headers: x-token'

#include <fiasco/fiasco.hpp>

#include <cstdint>
#include <cstdlib>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

}  // namespace

int main() {
    fiasco::app app;

    app.middleware(fiasco::middleware::cors({
        .allow_origins = {"https://example.com"},
        .allow_methods = {"GET", "POST"},
        .allow_headers = {"X-Token"},
        .allow_credentials = true,
        .expose_headers = {"X-Request-ID"},
        .max_age = 600,
    }));

    app.get("/hello", [] { return "hello"; });
    app.post("/hello", [] { return "created"; });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
