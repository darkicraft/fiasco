// Multiple body parameters are embedded by parameter name, like FastAPI.
//
//   cmake --build build --target multiple_bodies
//   ./build/examples/models/multiple_bodies
//
//   curl -X POST localhost:8080/search \
//     -H 'Content-Type: application/json' \
//     -d '{"request":{"query":"cats"},"options":{"limit":10}}'
//
// The body is one JSON object whose keys are the parameter names:
//   request -> search_request
//   options -> search_options

#include <fiasco/fiasco.hpp>

#include <string>

using fiasco::json;
using fiasco::post;

namespace {

struct search_request {
    std::string query;
};

struct search_options {
    int limit = 10;
};

}  // namespace

namespace routes {

[[=post("/search")]]
auto search(search_request request, search_options options) -> json {
    return json{
        {"query", request.query},
        {"limit", options.limit},
    };
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
