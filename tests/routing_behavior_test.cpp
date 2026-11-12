#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "fiasco/fiasco.hpp"

namespace {

fiasco::request make_request(std::string_view method, std::string url) {
    fiasco::request req;
    req.method = fiasco::detail::string_to_method(std::string(method));
    req.url = std::move(url);
    req.reanchor_url_views();
    return req;
}

}  // namespace

TEST_CASE("405 responses carry an Allow header", "[routing][http]") {
    fiasco::app app;
    app.get("/only-get", [] { return std::string{"ok"}; });

    fiasco::test_client client{app};
    const auto response = client.handle(make_request("POST", "/only-get"));

    REQUIRE(response.status_code == 405);
    REQUIRE(response.headers.contains("Allow"));
    CHECK(response.headers.at("Allow") == "GET");
}

TEST_CASE("trailing slash policy redirects to the registered path", "[routing][http]") {
    fiasco::app app;
    app.get("/slash/", [] { return std::string{"ok"}; });

    fiasco::test_client client{app};

    const auto redirect = client.handle(make_request("GET", "/slash?x=1"));
    REQUIRE(redirect.status_code == 307);
    REQUIRE(redirect.headers.contains("Location"));
    CHECK(redirect.headers.at("Location") == "/slash/?x=1");

    const auto direct = client.handle(make_request("GET", "/slash/"));
    REQUIRE(direct.status_code == 200);
}

TEST_CASE("trailing slash redirect can be disabled", "[routing][http]") {
    fiasco::app_options options;
    fiasco::router_options router_options;
    router_options.redirect_slashes = false;

    fiasco::app app{options, router_options};
    app.get("/slash/", [] { return std::string{"ok"}; });

    fiasco::test_client client{app};
    const auto response = client.handle(make_request("GET", "/slash"));

    CHECK(response.status_code == 404);
}
