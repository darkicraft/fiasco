#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <string>

#include "fiasco/fiasco.hpp"
#include "support/test_routes.hpp"

namespace descriptor_routes {

[[=fiasco::get("/items/{id}",
                {.summary = "Fetch item",
                 .description = "Returns one item.",
                 .name = "fetch_item",
                 .operation_id = "fetch_item_operation"})]]
std::string
    fetch(long id, std::optional<int> page, fiasco::request req) {
    (void)page;
    (void)req;
    return std::to_string(id);
}

}  // namespace descriptor_routes

namespace prefixed_routes {

[[=fiasco::get("/ping")]]
std::string ping() {
    return "pong";
}

}  // namespace prefixed_routes

TEST_CASE("registration materialises a route descriptor", "[routing][descriptor]") {
    fiasco::router_options options;
    options.tags = {"api"};

    fiasco::router router{options};
    router.register_api<^^descriptor_routes>();

    const fiasco::detail::route_descriptor* found = nullptr;
    router.for_each_descriptor([&](const auto& descriptor) { found = &descriptor; });

    REQUIRE(found != nullptr);
    CHECK(found->method == fiasco::detail::http_method::get);
    CHECK(found->path == "/items/{id}");
    CHECK(found->summary == "Fetch item");
    CHECK(found->description == "Returns one item.");
    CHECK(found->name == "fetch_item");
    CHECK(found->operation_id == "fetch_item_operation");
    CHECK(found->tags == std::vector<std::string>{"api"});
    REQUIRE(found->parameters.size() == 3);

    const auto& id = found->parameters[0];
    CHECK(id.name == "id");
    CHECK(id.source == fiasco::param_in::path);
    CHECK(id.required);
    CHECK_FALSE(id.injected);

    const auto& page = found->parameters[1];
    CHECK(page.name == "page");
    CHECK(page.source == fiasco::param_in::query);
    CHECK_FALSE(page.required);
    CHECK_FALSE(page.injected);

    const auto& req = found->parameters[2];
    CHECK(req.injected);
    CHECK_FALSE(req.required);
}

TEST_CASE("router options cascade into merged routes", "[routing][descriptor]") {
    fiasco::router_options sub_options;
    sub_options.prefix = "/v1";
    sub_options.tags = {"sub"};

    fiasco::router sub{sub_options};
    sub.register_api<^^prefixed_routes>();

    fiasco::router_options parent_options;
    parent_options.tags = {"api"};

    fiasco::router parent{parent_options};
    parent.include_router(std::move(sub));

    const fiasco::detail::route_descriptor* found = nullptr;
    parent.for_each_descriptor([&](const auto& descriptor) { found = &descriptor; });

    REQUIRE(found != nullptr);
    CHECK(found->path == "/v1/ping");
    CHECK(found->tags == std::vector<std::string>{"sub", "api"});
}

TEST_CASE("duplicate method and path are rejected", "[routing][descriptor]") {
    fiasco::router router;
    fiasco_test::get(router, "/dup", [] { return std::string{"first"}; });

    REQUIRE_THROWS_AS(fiasco_test::get(router, "/dup", [] { return std::string{"second"}; }),
                      std::logic_error);
}
