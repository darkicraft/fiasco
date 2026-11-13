// The type-erased fiasco::json wrapper.
//
//   cmake --build build --target json_wrapper
//   ./build/examples/json_wrapper
//
//   curl localhost:8080/nest/4
//   curl localhost:8080/object
//   curl localhost:8080/roundtrip
//
// fiasco::json can be built dynamically, dumped, parsed, and read back with
// get<T>(). It is the same type handlers use for ad-hoc JSON responses.

#include <fiasco/fiasco.hpp>

#include <algorithm>
#include <string>

using fiasco::get;
using fiasco::json;

namespace {

auto nest(int depth) -> json {
    if (depth <= 0) {
        return fiasco::json("leaf");
    }
    return json{
        {"depth", depth},
        {"child", nest(depth - 1)},
    };
}

}  // namespace

namespace routes {

[[=get("/nest/{depth}")]]
auto nested(int depth) -> json {
    return nest(std::clamp(depth, 0, 32));
}

[[=get("/object")]]
auto object() -> json {
    json result = json::object();
    result["name"] = "fiasco";
    result["numbers"] = json::array({1, 2, 3});
    result["nested"] = json{{"ok", true}, {"count", 2}};
    return result;
}

[[=get("/roundtrip")]]
auto roundtrip() -> json {
    json original{{"answer", 42}, {"items", json::array({"a", "b"})}};
    const std::string text = original.dump();
    const json parsed = json::parse(text);

    return json{
        {"dumped", text},
        {"answer", parsed["answer"].get<int>()},
        {"first_item", parsed["items"][0].get<std::string>()},
        {"has_items", parsed.contains("items")},
    };
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
