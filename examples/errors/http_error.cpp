// Throwing http_error for direct HTTP responses.
//
//   cmake --build build --target http_errors
//   FIASCO_PORT=8080 ./build/examples/http_errors
//
//   curl -i localhost:8080/private
//   curl -i localhost:8080/private -H 'Authorization: Bearer secret'
//   curl -i localhost:8080/structured
//
// http_error supports string details, structured JSON details, and response
// headers. It can be thrown from handlers or dependencies.

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

    app.get("/private",
            [](std::optional<std::string> authorization
                   [[= fiasco::header{.alias = "Authorization"}]]) {
                if (!authorization || *authorization != "Bearer secret") {
                    throw fiasco::http_error(
                        401,
                        "Not authenticated",
                        {{"WWW-Authenticate", "Bearer"}});
                }
                return fiasco::json{{"message", "welcome"}};
            });

    app.get("/structured", []() {
        throw fiasco::http_error(
            422,
            fiasco::json{{"field", "email"}, {"reason", "invalid"}});
    });

    app.run();
}
