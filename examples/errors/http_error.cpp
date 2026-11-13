// Throwing http_error for direct HTTP responses.
//
//   cmake --build build --target http_errors
//   ./build/examples/errors/http_error
//
//   curl -i localhost:8080/private
//   curl -i localhost:8080/private -H 'Authorization: Bearer secret'
//   curl -i localhost:8080/structured
//
// http_error supports string details, structured JSON details, and response
// headers. It can be thrown from handlers or dependencies.

#include <fiasco/fiasco.hpp>

#include <optional>
#include <string>

using fiasco::get;
using fiasco::header;
using fiasco::json;

namespace routes {

[[=get("/private")]]
auto private_route(
    std::optional<std::string> authorization [[=header{.alias = "Authorization"}]]) -> json {
    if (!authorization || *authorization != "Bearer secret") {
        throw fiasco::http_error(401, "Not authenticated", {{"WWW-Authenticate", "Bearer"}});
    }
    return json{{"message", "welcome"}};
}

[[=get("/structured")]]
auto structured() -> void {
    throw fiasco::http_error(422, json{{"field", "email"}, {"reason", "invalid"}});
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
