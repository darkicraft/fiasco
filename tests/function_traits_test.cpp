#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "fiasco/internal/routing/function_traits.hpp"

using namespace fiasco::detail;

// ============================================================
// convert_path_param
// ============================================================

TEST_CASE("convert_path_param string", "[function_traits]") {
    auto s = convert_path_param<std::string>("hello");
    REQUIRE(s == "hello");
}

TEST_CASE("convert_path_param int", "[function_traits]") {
    auto v = convert_path_param<int>("42");
    REQUIRE(v == 42);
}

TEST_CASE("convert_path_param long", "[function_traits]") {
    const auto max = std::numeric_limits<long>::max();
    auto v = convert_path_param<long>(std::to_string(max));
    REQUIRE(v == max);
}

TEST_CASE("convert_path_param long long", "[function_traits]") {
    auto v = convert_path_param<long long>("2147483648");
    REQUIRE(v == 2147483648LL);
}

TEST_CASE("convert_path_param fixed-width integers", "[function_traits]") {
    REQUIRE(static_cast<int>(convert_path_param<std::int8_t>("-12")) == -12);
    REQUIRE(static_cast<unsigned int>(convert_path_param<std::uint8_t>("200")) == 200);
    REQUIRE(convert_path_param<std::int16_t>("-1234") == -1234);
    REQUIRE(convert_path_param<std::uint16_t>("65000") == 65000);
    REQUIRE(convert_path_param<std::int32_t>("-123456") == -123456);
    REQUIRE(convert_path_param<std::uint32_t>("4294967295") == 4294967295u);
    REQUIRE(convert_path_param<std::int64_t>("2147483648") == 2147483648LL);
    REQUIRE(convert_path_param<std::uint64_t>("18446744073709551615") ==
            18446744073709551615ULL);
}

TEST_CASE("convert_path_param float", "[function_traits]") {
    auto v = convert_path_param<float>("3.14");
    REQUIRE(v == Catch::Approx(3.14f));
}

TEST_CASE("convert_path_param double", "[function_traits]") {
    auto v = convert_path_param<double>("2.718");
    REQUIRE(v == Catch::Approx(2.718));
}

TEST_CASE("convert_path_param bool true", "[function_traits]") {
    REQUIRE(convert_path_param<bool>("true") == true);
    REQUIRE(convert_path_param<bool>("1") == true);
}

TEST_CASE("convert_path_param bool false", "[function_traits]") {
    REQUIRE(convert_path_param<bool>("false") == false);
    REQUIRE(convert_path_param<bool>("0") == false);
    REQUIRE(convert_path_param<bool>("no") == false);
    REQUIRE(convert_path_param<bool>("off") == false);
}

TEST_CASE("convert_path_param bool rejects non-boolean", "[function_traits]") {
    // Pydantic does not treat arbitrary strings as false.
    REQUIRE_THROWS(convert_path_param<bool>("anything"));
    REQUIRE_THROWS(convert_path_param<bool>("2"));
}

TEST_CASE("convert_path_param int throws on bad input", "[function_traits]") {
    REQUIRE_THROWS_AS(convert_path_param<int>("not_a_number"), std::runtime_error);
    REQUIRE_THROWS_AS(convert_path_param<int>(""), std::runtime_error);
}

// ============================================================
// serialize_return
// ============================================================

TEST_CASE("serialize_return returns response as-is", "[function_traits]") {
    auto res = response::text("hello");
    auto result = serialize_return(res);
    REQUIRE(result.status_code == 200);
    REQUIRE(result.body == "hello\n");
}

TEST_CASE("serialize_return string to text response", "[function_traits]") {
    auto result = serialize_return(std::string("test"));
    REQUIRE(result.status_code == 200);
    REQUIRE(result.headers.at("Content-Type") == "text/plain");
}

TEST_CASE("serialize_return const char*", "[function_traits]") {
    auto result = serialize_return("cstr");
    REQUIRE(result.status_code == 200);
}

TEST_CASE("serialize_return int", "[function_traits]") {
    auto result = serialize_return(42);
    REQUIRE(result.status_code == 200);
    REQUIRE(result.body.find("42") != std::string::npos);
}

TEST_CASE("serialize_return double", "[function_traits]") {
    auto result = serialize_return(3.14);
    REQUIRE(result.status_code == 200);
}

// ============================================================
// make_handler — wraps callables into handler_fn
// ============================================================

TEST_CASE("make_handler no-arg returns string", "[function_traits]") {
    auto handler = make_handler([]() -> std::string { return "hello from handler"; });

    request req;
    auto res = handler(std::move(req));
    REQUIRE(res.status_code == 200);
    REQUIRE(res.body == "hello from handler\n");
}

TEST_CASE("make_handler no-arg returns void", "[function_traits]") {
    bool called = false;
    auto handler = make_handler([&called]() { called = true; });

    request req;
    auto res = handler(std::move(req));
    REQUIRE(called);
    REQUIRE(res.status_code == 204);  // response::empty()
}

TEST_CASE("make_handler captures request", "[function_traits]") {
    auto handler =
        make_handler([](const request& req) -> std::string { return req.header("X-Test"); });

    request req;
    req.headers["X-Test"] = "captured";
    auto res = handler(std::move(req));
    REQUIRE(res.body == "captured\n");
}

TEST_CASE("make_handler binds named path params by name", "[function_traits]") {
    auto handler = make_handler([](int id, std::string name) -> std::string {
        return "id=" + std::to_string(id) + " name=" + name;
    });

    request req;
    req.path_params.emplace("id", "42");
    req.path_params.emplace("name", "alice");
    auto res = handler(std::move(req));
    REQUIRE(res.body.find("id=42") != std::string::npos);
    REQUIRE(res.body.find("name=alice") != std::string::npos);
}

TEST_CASE("make_handler throws on missing argument", "[function_traits]") {
    auto handler = make_handler([](int id) -> std::string { return std::to_string(id); });

    request req;
    // Neither a path param nor a query param named "id"
    REQUIRE_THROWS_AS(handler(std::move(req)), request_validation_error);
}

TEST_CASE("make_handler with query parameter", "[function_traits]") {
    auto handler =
        make_handler([](int value) -> std::string { return "got=" + std::to_string(value); });

    request req;
    req.query_params["value"] = "42";

    auto res = handler(std::move(req));
    REQUIRE(res.body.find("got=42") != std::string::npos);
}

// ============================================================
// make_handler with function pointers
// ============================================================

static std::string free_function(int x) {
    return "free:" + std::to_string(x);
}

TEST_CASE("make_handler with function pointer", "[function_traits]") {
    auto handler = make_handler(free_function);

    request req;
    req.ordered_path_params.push_back("77");
    auto res = handler(std::move(req));
    REQUIRE(res.body.find("free:77") != std::string::npos);
}

// ============================================================
// make_handler — move-only callables
// ============================================================

TEST_CASE("make_handler accepts move-only callables", "[function_traits]") {
    auto owned = std::make_unique<int>(41);
    auto handler = make_handler([p = std::move(owned)]() -> int { return *p + 1; });

    request req;
    auto res = handler(std::move(req));
    REQUIRE(res.body == "42\n");
}

// ============================================================
// make_handler — error propagation
// ============================================================

TEST_CASE("make_handler handler exception propagates", "[function_traits]") {
    auto handler = make_handler([]() -> std::string { throw std::runtime_error("handler error"); });

    request req;
    REQUIRE_THROWS_AS(handler(std::move(req)), std::runtime_error);
}
