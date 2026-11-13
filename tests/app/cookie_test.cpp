#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>

#include "fiasco/fiasco.hpp"
#include "support/test_routes.hpp"

namespace {

fiasco::request make_request(std::string_view method, std::string url) {
    fiasco::request req;
    req.method = fiasco::detail::string_to_method(std::string(method));
    req.url = std::move(url);
    req.reanchor_url_views();
    return req;
}

}  // namespace

TEST_CASE("response serializes cookie attributes", "[cookies]") {
    auto res = fiasco::response::text("ok").set_cookie("session",
                                                       "abc",
                                                       {.max_age = 3600,
                                                        .path = "/app",
                                                        .domain = "example.com",
                                                        .secure = true,
                                                        .httponly = true,
                                                        .samesite = "Strict"});

    const auto raw = res.serialize();
    CHECK(raw.find("Set-Cookie: session=abc; Path=/app; Domain=example.com; "
                   "Max-Age=3600; Secure; HttpOnly; SameSite=Strict\r\n") != std::string::npos);
}

TEST_CASE("response serializes datetime expiry as an HTTP date", "[cookies]") {
    using namespace std::chrono;

    const sys_seconds expires =
        sys_days{year{1994} / November / 6} + hours{8} + minutes{49} + seconds{37};

    auto res = fiasco::response::empty();
    res.set_cookie("session", "abc", {.expires = expires});

    const auto raw = res.serialize();
    CHECK(raw.find("Expires=Sun, 06 Nov 1994 08:49:37 GMT") != std::string::npos);
}

TEST_CASE("response emits one Set-Cookie header per cookie", "[cookies]") {
    auto res = fiasco::response::empty();
    res.set_cookie("first", "1");
    res.set_cookie("second", "2");

    const auto raw = res.serialize();
    CHECK(raw.find("Set-Cookie: first=1; Path=/; SameSite=lax\r\n") != std::string::npos);
    CHECK(raw.find("Set-Cookie: second=2; Path=/; SameSite=lax\r\n") != std::string::npos);
}

TEST_CASE("response quotes values that need quoting", "[cookies]") {
    auto res = fiasco::response::empty();
    res.set_cookie("spaced", "a b");
    res.set_cookie("empty", "");

    const auto raw = res.serialize();
    CHECK(raw.find("Set-Cookie: spaced=\"a b\";") != std::string::npos);
    CHECK(raw.find("Set-Cookie: empty=\"\";") != std::string::npos);
}

TEST_CASE("delete_cookie sends an expired empty cookie", "[cookies]") {
    auto res = fiasco::response::empty();
    res.delete_cookie("session", {.path = "/app"});

    const auto raw = res.serialize();
    CHECK(raw.find("Set-Cookie: session=\"\"; Path=/app; Max-Age=0; Expires=") !=
          std::string::npos);
}

TEST_CASE("invalid cookie input is rejected", "[cookies]") {
    fiasco::response res;

    REQUIRE_THROWS_AS(res.set_cookie("bad name", "x"), std::invalid_argument);
    REQUIRE_THROWS_AS(res.set_cookie("name", "line\nbreak"), std::invalid_argument);
    REQUIRE_THROWS_AS(res.set_cookie("name", "x", {.samesite = "sometimes"}),
                      std::invalid_argument);
}

TEST_CASE("handlers can set cookies through the application seam", "[cookies]") {
    fiasco::app app;
    fiasco_test::get(app, "/login", [] {
        return fiasco::response::text("ok").set_cookie(
            "session", "abc", {.secure = true, .httponly = true});
    });

    fiasco::test_client client{app};
    const auto res = client.handle(make_request("GET", "/login"));

    REQUIRE(res.set_cookie_headers.size() == 1);
    CHECK(res.set_cookie_headers[0] == "session=abc; Path=/; Secure; HttpOnly; SameSite=lax");
    CHECK_FALSE(res.headers.contains("Set-Cookie"));
}
