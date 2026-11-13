// Response factories, automatic wrapping, and response mutation.
//
//   cmake --build build --target responses
//   ./build/examples/responses
//
//   curl -i localhost:8080/auto/text
//   curl -i localhost:8080/auto/number
//   curl -i localhost:8080/auto/model
//   curl -i localhost:8080/auto/void
//   curl -i localhost:8080/text
//   curl -i localhost:8080/html
//   curl -i localhost:8080/json
//   curl -i localhost:8080/redirect
//   curl -i localhost:8080/error
//   curl -i localhost:8080/empty
//   curl -i localhost:8080/headers

#include <fiasco/fiasco.hpp>

#include <string>

using fiasco::get;
using fiasco::response;

namespace {

struct greeting {
    std::string message;
    bool automatic = true;
};

}  // namespace

namespace routes {

// Plain values are wrapped automatically.
[[=get("/auto/text")]]
auto auto_text() -> std::string {
    return "plain text";
}

[[=get("/auto/number")]]
auto auto_number() -> int {
    return 42;
}

[[=get("/auto/model")]]
auto auto_model() -> greeting {
    return greeting{"from a model"};
}

[[=get("/auto/void")]]
auto auto_void() -> void {}

// Explicit response factories.
[[=get("/text")]]
auto text() -> response {
    return response::text("plain text");
}

[[=get("/html")]]
auto html() -> response {
    return response::html("<h1>Hello</h1>");
}

[[=get("/json")]]
auto raw_json() -> response {
    return response::json(R"({"key":"value","n":[1,2,3]})");
}

[[=get("/redirect")]]
auto redirect() -> response {
    return response::redirect("/auto/text");
}

[[=get("/error")]]
auto error() -> response {
    return response::error("something went wrong", 400);
}

[[=get("/empty")]]
auto empty() -> response {
    return response::empty(204);
}

[[=get("/headers")]]
auto headers() -> response {
    auto res = response::text("check the headers");
    res.status_code = 203;
    res.headers["X-Example"] = "fiasco";
    return res;
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
