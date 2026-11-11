// Explicit parameter sources: path, query, body, header, and cookie.
//
//   cmake --build build --target param_sources
//   FIASCO_PORT=8080 ./build/examples/param_sources
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

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

struct payload {
    std::string note;
};

}  // namespace

int main() {
    fiasco::app app;

    app.post("/items/{id}",
             [](int id [[= fiasco::path{.ge = 1}]],
               std::string q [[= fiasco::query{.min_length = 2, .alias = "search"}]],
               std::string token [[= fiasco::header{.alias = "X-Token"}]],
               std::string session [[= fiasco::cookie{.alias = "session_id"}]],
               payload body [[= fiasco::body{}]]) {
                 return fiasco::json{
                     {"id", id},
                     {"search", q},
                     {"token", token},
                     {"session", session},
                     {"note", body.note},
                 };
             });

    app.get("/optional",
            [](std::optional<std::string> trace [[= fiasco::header{.alias = "X-Trace"}]],
               std::optional<int> page [[= fiasco::query{.ge = 1}]]) {
                return fiasco::json{
                    {"trace", trace ? fiasco::json(*trace) : fiasco::json(nullptr)},
                    {"page", page ? fiasco::json(*page) : fiasco::json(nullptr)},
                };
            });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
