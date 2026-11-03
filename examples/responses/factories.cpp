// Response factories, automatic wrapping, and response mutation.
//
//   cmake --build build --target responses
//   FIASCO_PORT=8080 ./build/examples/responses
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

struct greeting {
    std::string message;
    bool automatic = true;
};

}  // namespace

int main() {
    fiasco::server app{
        fiasco::server_config{.network = {.port = example_port()}},
    };

    // Plain values are wrapped automatically.
    app.get("/auto/text", []() { return "plain text"; });
    app.get("/auto/number", []() { return 42; });
    app.get("/auto/model", []() { return greeting{"from a model"}; });
    app.get("/auto/void", []() {});

    // Explicit response factories.
    app.get("/text", []() { return fiasco::response::text("plain text"); });
    app.get("/html", []() { return fiasco::response::html("<h1>Hello</h1>"); });
    app.get("/json", []() { return fiasco::response::json(R"({"key":"value","n":[1,2,3]})"); });
    app.get("/redirect", []() { return fiasco::response::redirect("/auto/text"); });
    app.get("/error", []() { return fiasco::response::error("something went wrong", 400); });
    app.get("/empty", []() { return fiasco::response::empty(204); });

    app.get("/headers", []() {
        auto response = fiasco::response::text("check the headers");
        response.status_code = 203;
        response.headers["X-Example"] = "fiasco";
        return response;
    });

    app.run();
}
