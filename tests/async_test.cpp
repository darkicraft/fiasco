#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "fiasco/internal/http/async.hpp"
#include "fiasco/internal/http/response.hpp"

using namespace fiasco::detail;

namespace fiasco::detail {

struct async_user {
    std::string name;
    int age;
};

}  // namespace fiasco::detail

TEST_CASE("async_result converts a model to JSON", "[async]") {
    async_result result{async_user{"ada", 36}};
    const response res = std::move(result).take();

    REQUIRE(res.status_code == 200);
    REQUIRE(res.headers.at("Content-Type") == "application/json");
    REQUIRE(res.body.find(R"("name":"ada")") != std::string::npos);
    REQUIRE(res.body.find(R"("age":36)") != std::string::npos);
}

TEST_CASE("async_result converts text and primitives", "[async]") {
    const response text = std::move(async_result{"hello"}).take();
    REQUIRE(text.status_code == 200);
    REQUIRE(text.headers.at("Content-Type") == "text/plain");
    REQUIRE(text.body == "hello\n");

    const response number = std::move(async_result{42}).take();
    REQUIRE(number.status_code == 200);
    REQUIRE(number.headers.at("Content-Type") == "text/plain");
    REQUIRE(number.body == "42\n");
}

TEST_CASE("async_result accepts a response unchanged", "[async]") {
    response original = response::empty(204);
    original.headers["X-Test"] = "yes";

    const response res = std::move(async_result{std::move(original)}).take();
    REQUIRE(res.status_code == 204);
    REQUIRE(res.headers.at("X-Test") == "yes");
}

TEST_CASE("async_result converts a collection of models", "[async]") {
    const std::vector<async_user> users{{"ada", 36}, {"grace", 40}};
    const response res = std::move(async_result{users}).take();

    REQUIRE(res.status_code == 200);
    REQUIRE(res.headers.at("Content-Type") == "application/json");
    REQUIRE(res.body.find(R"("name":"ada")") != std::string::npos);
    REQUIRE(res.body.find(R"("name":"grace")") != std::string::npos);
}

TEST_CASE("typed_async_result carries its value type", "[async]") {
    static_assert(std::same_as<typename typed_async_result<async_user>::value_type, async_user>);

    typed_async_result<async_user> result{async_user{"ada", 36}};
    const response res = std::move(result).take();

    REQUIRE(res.status_code == 200);
    REQUIRE(res.headers.at("Content-Type") == "application/json");
    REQUIRE(res.body.find(R"("name":"ada")") != std::string::npos);
    REQUIRE(res.body.find(R"("age":36)") != std::string::npos);
}
