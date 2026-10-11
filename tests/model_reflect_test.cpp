#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "fiasco/internal/json.hpp"

using namespace fiasco::detail;

namespace {

struct Point {
    int x;
    int y;
};

struct Nested {
    std::string name;
    Point p;
    std::vector<int> xs;
    std::optional<int> opt;
};

}  // namespace

TEST_CASE("reflection model serialises members by name", "[model]") {
    json j = Point{3, 4};
    REQUIRE(j.contains("x"));
    REQUIRE(j.contains("y"));
    REQUIRE(j["x"].get<int>() == 3);
    REQUIRE(j["y"].get<int>() == 4);
    REQUIRE(j.dump() == R"({"x":3,"y":4})");
}

TEST_CASE("reflection model round-trips", "[model]") {
    Nested n{"hi", {1, 2}, {7, 8, 9}, std::nullopt};
    json j = n;

    REQUIRE(j["name"].get<std::string>() == "hi");
    REQUIRE(j["p"].contains("x"));

    auto back = j.get<Nested>();
    REQUIRE(back.name == "hi");
    REQUIRE(back.p.x == 1);
    REQUIRE(back.p.y == 2);
    REQUIRE(back.xs == std::vector<int>{7, 8, 9});
    REQUIRE_FALSE(back.opt.has_value());
}

TEST_CASE("reflection model honours present optionals", "[model]") {
    Nested n{"m", {5, 6}, {1}, 42};
    json j = n;
    auto back = j.get<Nested>();
    REQUIRE(back.opt.has_value());
    REQUIRE(back.opt.value() == 42);
}
