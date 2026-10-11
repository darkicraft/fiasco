#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <optional>
#include <string>
#include <vector>

#include "fiasco/internal/json.hpp"

using namespace fiasco::detail;
using Catch::Matchers::ContainsSubstring;

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

TEST_CASE("missing required field reports the field name", "[model]") {
    json j = json_type::object();
    j["x"] = json_type(1);  // "y" is absent
    REQUIRE_THROWS_WITH(j.get<Point>(), ContainsSubstring("missing required field 'y'"));
}

TEST_CASE("wrong field type reports the field name", "[model]") {
    json j = json_type::object();
    j["x"] = json_type("not-an-int");
    j["y"] = json_type(2);
    REQUIRE_THROWS_WITH(j.get<Point>(), ContainsSubstring("field 'x'"));
}

TEST_CASE("absent optional field is not an error", "[model]") {
    json j = json_type::object();
    j["name"] = json_type("n");
    j["p"] = json_type(Point{1, 2});
    j["xs"] = json_type(std::vector<int>{});
    // "opt" is omitted entirely
    auto back = j.get<Nested>();
    REQUIRE_FALSE(back.opt.has_value());
}
