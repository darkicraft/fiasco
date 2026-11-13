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

TEST_CASE("default docs and redoc pages use CDN assets", "[docs]") {
    fiasco::app app;
    fiasco::test_client client{app};

    const auto docs = client.handle(make_request("GET", "/docs"));
    REQUIRE(docs.status_code == 200);
    CHECK(docs.headers.at("Content-Type") == "text/html");
    CHECK(docs.body.find("swagger-ui-bundle.js") != std::string::npos);
    CHECK(docs.body.find("/openapi.json") != std::string::npos);

    const auto redoc = client.handle(make_request("GET", "/redoc"));
    REQUIRE(redoc.status_code == 200);
    CHECK(redoc.headers.at("Content-Type") == "text/html");
    CHECK(redoc.body.find("redoc.standalone.js") != std::string::npos);
    CHECK(redoc.body.find("spec-url=\"/openapi.json\"") != std::string::npos);
}

TEST_CASE("docs and redoc can be disabled independently", "[docs]") {
    fiasco::app_options options;
    options.docs_url.clear();
    options.redoc_url.clear();

    fiasco::app app(options);
    fiasco::test_client client{app};

    CHECK(client.handle(make_request("GET", "/docs")).status_code == 404);
    CHECK(client.handle(make_request("GET", "/redoc")).status_code == 404);
}

TEST_CASE("disabling openapi disables the docs pages", "[docs]") {
    fiasco::app_options options;
    options.openapi_url.clear();

    fiasco::app app(options);
    fiasco::test_client client{app};

    CHECK(client.handle(make_request("GET", "/openapi.json")).status_code == 404);
    CHECK(client.handle(make_request("GET", "/docs")).status_code == 404);
    CHECK(client.handle(make_request("GET", "/redoc")).status_code == 404);
}

TEST_CASE("docs URLs and assets are configurable", "[docs]") {
    fiasco::app_options options;
    options.docs_url = "/documentation";
    options.redoc_url = "/reference";
    options.swagger_ui_js_url = "/assets/swagger-ui.js";
    options.swagger_ui_css_url = "/assets/swagger-ui.css";
    options.redoc_js_url = "/assets/redoc.js";
    options.title = "Custom <API>";

    fiasco::app app(options);
    fiasco::test_client client{app};

    CHECK(client.handle(make_request("GET", "/docs")).status_code == 404);

    const auto docs = client.handle(make_request("GET", "/documentation"));
    REQUIRE(docs.status_code == 200);
    CHECK(docs.body.find("/assets/swagger-ui.js") != std::string::npos);
    CHECK(docs.body.find("/assets/swagger-ui.css") != std::string::npos);
    CHECK(docs.body.find("Custom &lt;API&gt; - Swagger UI") != std::string::npos);

    const auto redoc = client.handle(make_request("GET", "/reference"));
    REQUIRE(redoc.status_code == 200);
    CHECK(redoc.body.find("/assets/redoc.js") != std::string::npos);
}

TEST_CASE("swagger parameters and oauth initialization are rendered", "[docs]") {
    fiasco::app_options options;
    options.swagger_ui_parameters =
        fiasco::json{{"syntaxHighlight", false}, {"displayRequestDuration", true}};
    options.swagger_ui_init_oauth = fiasco::json{{"clientId", "demo-client"}};

    fiasco::app app(options);
    fiasco::test_client client{app};

    const auto docs = client.handle(make_request("GET", "/docs"));
    REQUIRE(docs.status_code == 200);
    CHECK(docs.body.find("\"syntaxHighlight\": false") != std::string::npos);
    CHECK(docs.body.find("\"displayRequestDuration\": true") != std::string::npos);
    CHECK(docs.body.find("ui.initOAuth(") != std::string::npos);
    CHECK(docs.body.find("demo-client") != std::string::npos);
}
