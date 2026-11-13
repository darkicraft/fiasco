// Query parameters are bound by name and converted to the parameter type.
//
//   cmake --build build --target query_params
//   ./build/examples/query_params
//
//   curl 'localhost:8080/search?q=cats&page=2&verbose=true'
//   curl 'localhost:8080/search?q=cats'                 # page/verbose are null
//   curl 'localhost:8080/search'                        # 422: q is required
//   curl 'localhost:8080/decode?text=hello%20world%2Bc%2B%2B'
//   curl 'localhost:8080/duplicate?tag=first&tag=second'
//   curl 'localhost:8080/search?q=cats&page=abc'        # 422: parsing

#include <fiasco/fiasco.hpp>

#include <optional>
#include <string>

using fiasco::get;
using fiasco::json;

namespace routes {

[[=get("/search")]]
auto search(std::string q,
                                 std::optional<int> page,
                                 std::optional<bool> verbose) -> json {
    return json{
        {"q", q},
        {"page", page ? json(*page) : json(nullptr)},
        {"verbose", verbose ? json(*verbose) : json(nullptr)},
    };
}

[[=get("/decode")]]
auto decode(std::string text) -> json {
    return json{{"decoded", text}};
}

// The first value wins for duplicate query keys.
[[=get("/duplicate")]]
auto duplicate(std::string tag) -> json {
    return json{{"tag", tag}};
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
