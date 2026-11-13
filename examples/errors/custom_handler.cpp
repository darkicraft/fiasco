// Translate domain exceptions with a custom exception handler.
//
//   cmake --build build --target exception_handlers
//   ./build/examples/exception_handlers
//
//   curl -i localhost:8080/conflict
//   curl -i localhost:8080/boom
//
// The handler parameter infers the exception type. The explicit
// add_exception_handler<error_type>(...) overload is available when the type
// cannot be inferred from a lambda parameter.

#include <fiasco/fiasco.hpp>

#include <stdexcept>
#include <string>

using fiasco::get;
using fiasco::response;

namespace {

struct conflict : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct bad_request : std::runtime_error {
    using std::runtime_error::runtime_error;
};

}  // namespace

namespace routes {

[[=get("/conflict")]]
auto conflict_route() -> void {
    throw conflict("resource already exists");
}

[[=get("/boom")]]
auto boom_route() -> void {
    throw bad_request("invalid request");
}

}  // namespace routes

int main() {
    fiasco::app app;

    app.add_exception_handler(
        [](const conflict& error) { return response::error(error.what(), 409); });

    app.add_exception_handler<bad_request>(
        [](const bad_request& error) { return response::error(error.what(), 400); });

    app.register_api<^^routes>();
    app.run();
}
