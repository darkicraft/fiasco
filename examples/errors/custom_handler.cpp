// Translate domain exceptions with a custom exception handler.
//
//   cmake --build build --target exception_handlers
//   FIASCO_PORT=8080 ./build/examples/exception_handlers
//
//   curl -i localhost:8080/conflict
//   curl -i localhost:8080/boom
//
// The handler parameter infers the exception type. The explicit
// add_exception_handler<error_type>(...) overload is available when the type
// cannot be inferred from a lambda parameter.

#include <fiasco/fiasco.hpp>

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

struct conflict : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct bad_request : std::runtime_error {
    using std::runtime_error::runtime_error;
};

}  // namespace

int main() {
    fiasco::app app;

    app.add_exception_handler([](const conflict& error) {
        return fiasco::response::error(error.what(), 409);
    });

    app.add_exception_handler<bad_request>([](const bad_request& error) {
        return fiasco::response::error(error.what(), 400);
    });

    app.get("/conflict", []() {
        throw conflict("resource already exists");
    });

    app.get("/boom", []() {
        throw bad_request("invalid request");
    });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
