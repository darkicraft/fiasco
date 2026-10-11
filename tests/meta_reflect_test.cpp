#include <catch2/catch_test_macros.hpp>

#include <string>
#include <tuple>
#include <type_traits>

#include "fiasco/internal/meta/reflect.hpp"

using namespace fiasco::detail;

int free_fn(int x, double y);

TEST_CASE("callable_traits reflects a lambda", "[meta]") {
    auto lam = [](int id, const std::string& name) -> long { return id + (long)name.size(); };
    using traits = meta::callable_traits<decltype(lam)>;

    STATIC_REQUIRE(std::same_as<traits::args_tuple, std::tuple<int, const std::string&>>);
    STATIC_REQUIRE(std::same_as<traits::return_type, long>);
    STATIC_REQUIRE(traits::arity == 2);
    STATIC_REQUIRE(traits::has_argument_names);
}

TEST_CASE("callable_traits reflects parameter names", "[meta]") {
    auto lam = [](int user_id, std::string name) { return 0; };
    using traits = meta::callable_traits<decltype(lam)>;

    STATIC_REQUIRE(traits::argument_name(0) == "user_id");
    STATIC_REQUIRE(traits::argument_name(1) == "name");
}

TEST_CASE("callable_traits reflects a mutable lambda", "[meta]") {
    auto lam = [n = 0](int x) mutable { return n + x; };
    using traits = meta::callable_traits<decltype(lam)>;

    STATIC_REQUIRE(traits::arity == 1);
    STATIC_REQUIRE(std::same_as<traits::return_type, int>);
}

TEST_CASE("callable_traits reflects a function pointer (types, no names)", "[meta]") {
    using traits = meta::callable_traits<decltype(&free_fn)>;

    STATIC_REQUIRE(std::same_as<traits::args_tuple, std::tuple<int, double>>);
    STATIC_REQUIRE(std::same_as<traits::return_type, int>);
    STATIC_REQUIRE(traits::arity == 2);
    STATIC_REQUIRE_FALSE(traits::has_argument_names);
}

int free_fn(int x, double y) {
    return x + (int)y;
}
