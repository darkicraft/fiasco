// A tiny FastAPI-style example: name-bound path & query params and a JSON model.
#include <fiasco/fiasco.hpp>

#include <string>

struct GreetingRequest {
    std::string message;
};

struct GreetingResponse {
    std::string message;
    std::string to;
};

int main() {
    fiasco::server app;

    app.get("/", []() { return "Hello, World!"; });

    // Path parameter, bound by name (order-independent).
    app.get("/hello/{name}", [](std::string name) { return "Hello, " + name + "!"; });

    // Query parameters, typed: GET /add?a=1&b=2
    app.get("/add", [](int a, int b) { return std::to_string(a + b); });

    // Path parameter + JSON body model -> reflected JSON response model.
    app.post("/greet/{name}", [](std::string name, GreetingRequest req) -> GreetingResponse {
        return {req.message, name};
    });

    app.run();
}
