// CORS middleware.
//
//   cmake --build build --target cors
//   ./build/examples/middleware/cors
//
//   curl -i localhost:8080/hello -H 'Origin: https://example.com'
//
//   curl -i -X OPTIONS localhost:8080/hello \
//     -H 'Origin: https://example.com' \
//     -H 'Access-Control-Request-Method: POST' \
//     -H 'Access-Control-Request-Headers: x-token'

#include <fiasco/fiasco.hpp>

#include <string>

using fiasco::get;
using fiasco::post;

namespace routes {

[[=get("/hello")]]
auto hello() -> std::string {
    return "hello";
}

[[=post("/hello")]]
auto create() -> std::string {
    return "created";
}

}  // namespace routes

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

    app.register_api<^^routes>();
    app.run();
}
