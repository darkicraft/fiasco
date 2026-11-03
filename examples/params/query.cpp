// Query parameters are bound by name and converted to the parameter type.
//
//   cmake --build build --target query_params
//   FIASCO_PORT=8080 ./build/examples/query_params
//
//   curl 'localhost:8080/search?q=cats&page=2&verbose=true'
//   curl 'localhost:8080/search?q=cats'                 # page/verbose are null
//   curl 'localhost:8080/search'                        # 422: q is required
//   curl 'localhost:8080/decode?text=hello%20world%2Bc%2B%2B'
//   curl 'localhost:8080/duplicate?tag=first&tag=second'
//   curl 'localhost:8080/search?q=cats&page=abc'        # 422: parsing

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

}  // namespace

int main() {
    fiasco::server app{
        fiasco::server_config{.network = {.port = example_port()}},
    };

    app.get("/search",
            [](std::string q, std::optional<int> page, std::optional<bool> verbose) {
                return fiasco::json{
                    {"q", q},
                    {"page", page ? fiasco::json(*page) : fiasco::json(nullptr)},
                    {"verbose", verbose ? fiasco::json(*verbose) : fiasco::json(nullptr)},
                };
            });

    app.get("/decode", [](std::string text) {
        return fiasco::json{{"decoded", text}};
    });

    // The first value wins for duplicate query keys.
    app.get("/duplicate", [](std::string tag) {
        return fiasco::json{{"tag", tag}};
    });

    app.run();
}
