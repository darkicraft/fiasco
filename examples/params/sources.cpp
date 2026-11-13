// Explicit parameter sources: path, query, body, header, and cookie.
//
//   cmake --build build --target param_sources
//   ./build/examples/param_sources
//
//   curl -X POST 'localhost:8080/items/7?search=cats' \
//     -H 'X-Token: abc' \
//     -H 'Cookie: session_id=xyz' \
//     -H 'Content-Type: application/json' \
//     -d '{"note":"hello"}'
//
//   curl 'localhost:8080/optional'
//   curl 'localhost:8080/optional' -H 'X-Trace: trace-1'

#include <fiasco/fiasco.hpp>

#include <optional>
#include <string>

using fiasco::body;
using fiasco::cookie;
using fiasco::get;
using fiasco::header;
using fiasco::json;
using fiasco::path;
using fiasco::post;
using fiasco::query;

namespace {

struct payload {
    std::string note;
};

}  // namespace

namespace routes {

struct payload {
    std::string note;
};

[[=post("/items/{id}")]]
auto create_item(
    int id [[=path{.ge = 1}]],
    std::string q [[=query{.min_length = 2, .alias = "search"}]],
    std::string token [[=header{.alias = "X-Token"}]],
    std::string session [[=cookie{.alias = "session_id"}]],
    payload value [[=body{}]]) -> json {
    return json{
        {"id", id},
        {"search", q},
        {"token", token},
        {"session", session},
        {"note", value.note},
    };
}

[[=get("/optional")]]
auto optional_values(
    std::optional<std::string> trace [[=header{.alias = "X-Trace"}]],
    std::optional<int> page [[=query{.ge = 1}]]) -> json {
    return json{
        {"trace", trace ? json(*trace) : json(nullptr)},
        {"page", page ? json(*page) : json(nullptr)},
    };
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
