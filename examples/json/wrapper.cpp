// The type-erased fiasco::json wrapper.
//
//   cmake --build build --target json_wrapper
//   FIASCO_PORT=8080 ./build/examples/json_wrapper
//
//   curl localhost:8080/nest/4
//   curl localhost:8080/object
//   curl localhost:8080/roundtrip
//
// fiasco::json can be built dynamically, dumped, parsed, and read back with
// get<T>(). It is the same type handlers use for ad-hoc JSON responses.

#include <fiasco/fiasco.hpp>

#include <algorithm>
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

fiasco::json nest(int depth) {
    if (depth <= 0) {
        return fiasco::json("leaf");
    }
    return fiasco::json{
        {"depth", depth},
        {"child", nest(depth - 1)},
    };
}

}  // namespace

int main() {
    fiasco::app app;

    app.get("/nest/{depth}", [](int depth) { return nest(std::clamp(depth, 0, 32)); });

    app.get("/object", []() {
        fiasco::json object = fiasco::json::object();
        object["name"] = "fiasco";
        object["numbers"] = fiasco::json::array({1, 2, 3});
        object["nested"] = fiasco::json{{"ok", true}, {"count", 2}};
        return object;
    });

    app.get("/roundtrip", []() {
        fiasco::json original{{"answer", 42}, {"items", fiasco::json::array({"a", "b"})}};
        const std::string text = original.dump();
        const fiasco::json parsed = fiasco::json::parse(text);

        return fiasco::json{
            {"dumped", text},
            {"answer", parsed["answer"].get<int>()},
            {"first_item", parsed["items"][0].get<std::string>()},
            {"has_items", parsed.contains("items")},
        };
    });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
