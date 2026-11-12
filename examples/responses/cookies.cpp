// Response cookies.
//
//   cmake --build build --target response_cookies
//   FIASCO_PORT=8080 ./build/examples/response_cookies
//
//   curl -i localhost:8080/set
//   curl -i localhost:8080/prefs
//   curl -i localhost:8080/delete

#include <fiasco/fiasco.hpp>

#include <chrono>
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

}  // namespace

int main() {
    fiasco::app app;

    app.get("/set", [] {
        const auto expires =
            std::chrono::system_clock::now() + std::chrono::hours{1};
        return fiasco::response::text("cookie set").set_cookie(
            "session", "demo-token",
            {.max_age = 3600,
             .expires = expires,
             .httponly = true,
             .samesite = "lax"});
    });

    app.get("/prefs", [] {
        auto response = fiasco::response::text("preferences set");
        response.set_cookie("theme", "dark");
        response.set_cookie("language", "en");
        return response;
    });

    app.get("/delete", [] {
        return fiasco::response::text("cookie deleted").delete_cookie("session");
    });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
