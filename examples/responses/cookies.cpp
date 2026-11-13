// Response cookies.
//
//   cmake --build build --target response_cookies
//   ./build/examples/responses/cookies
//
//   curl -i localhost:8080/set
//   curl -i localhost:8080/prefs
//   curl -i localhost:8080/delete

#include <fiasco/fiasco.hpp>

#include <chrono>
#include <string>

using fiasco::get;
using fiasco::response;

namespace routes {

[[=get("/set")]]
auto set() -> response {
    const auto expires = std::chrono::system_clock::now() + std::chrono::hours{1};
    return response::text("cookie set")
        .set_cookie("session",
                    "demo-token",
                    {.max_age = 3600, .expires = expires, .httponly = true, .samesite = "lax"});
}

[[=get("/prefs")]]
auto prefs() -> response {
    auto res = response::text("preferences set");
    res.set_cookie("theme", "dark");
    res.set_cookie("language", "en");
    return res;
}

[[=get("/delete")]]
auto remove() -> response {
    return response::text("cookie deleted").delete_cookie("session");
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
