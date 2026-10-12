#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <optional>
#include <string>
#include <vector>

#include "fiasco/internal/json.hpp"

using namespace fiasco::detail;
using Catch::Approx;
using Catch::Matchers::ContainsSubstring;

// Structs live in fiasco::detail so ADL from json_type's template constructor
// and get<T>() finds the reflection serialisers.
namespace fiasco::detail {

// ============================================================
// Basic struct with all primitive types
// ============================================================

struct AllPrimitives {
    int i = 0;
    long l = 0;
    long long ll = 0;
    float f = 0.0f;
    double d = 0.0;
    bool b = false;
    std::string s;
};

TEST_CASE("reflection model roundtrip all primitives", "[model]") {
    AllPrimitives v{42, 99L, 100LL, 3.14f, 2.718, true, "hello"};

    json_type j;
    to_json(j, v);

    REQUIRE(j.is_object());
    REQUIRE(j["i"].get<int>() == 42);
    REQUIRE(j["l"].get<long>() == 99);
    REQUIRE(j["ll"].get<long long>() == 100);
    REQUIRE(j["f"].get<float>() == Approx(3.14f));
    REQUIRE(j["d"].get<double>() == Approx(2.718));
    REQUIRE(j["b"].get<bool>() == true);
    REQUIRE(j["s"].get<std::string>() == "hello");

    AllPrimitives v2;
    from_json(j, v2);
    REQUIRE(v2.i == 42);
    REQUIRE(v2.l == 99);
    REQUIRE(v2.ll == 100);
    REQUIRE(v2.f == Approx(3.14f));
    REQUIRE(v2.d == Approx(2.718));
    REQUIRE(v2.b == true);
    REQUIRE(v2.s == "hello");
}

TEST_CASE("reflection model template constructor", "[model]") {
    AllPrimitives v{1, 2, 3, 1.0f, 2.0, false, "test"};
    json_type j(v);
    REQUIRE(j.is_object());
    REQUIRE(j["i"].get<int>() == 1);
    REQUIRE(j["s"].get<std::string>() == "test");
}

// ============================================================
// Nested structs
// ============================================================

struct Inner {
    int x = 0;
    std::string label;
};

struct Outer {
    int id = 0;
    Inner inner;
};

TEST_CASE("reflection model nested structs", "[model]") {
    Outer v{42, {7, "nested"}};

    json_type j;
    to_json(j, v);

    REQUIRE(j["id"].get<int>() == 42);
    REQUIRE(j["inner"]["x"].get<int>() == 7);
    REQUIRE(j["inner"]["label"].get<std::string>() == "nested");

    Outer v2;
    from_json(j, v2);
    REQUIRE(v2.id == 42);
    REQUIRE(v2.inner.x == 7);
    REQUIRE(v2.inner.label == "nested");
}

// ============================================================
// Optional fields
// ============================================================

struct WithOptional {
    int id = 0;
    std::optional<std::string> name = std::nullopt;
    std::optional<int> age = std::nullopt;
};

TEST_CASE("reflection model optional fields present", "[model]") {
    WithOptional v{1, std::optional<std::string>("Alice"), std::optional<int>(30)};

    json_type j;
    to_json(j, v);
    REQUIRE(j["id"].get<int>() == 1);
    REQUIRE(j["name"].get<std::string>() == "Alice");
    REQUIRE(j["age"].get<int>() == 30);

    WithOptional v2;
    from_json(j, v2);
    REQUIRE(v2.id == 1);
    REQUIRE(v2.name.has_value());
    REQUIRE(v2.name.value() == "Alice");
    REQUIRE(v2.age.has_value());
    REQUIRE(v2.age.value() == 30);
}

TEST_CASE("reflection model optional fields absent in json", "[model]") {
    json_type j = json_type::object();
    j["id"] = json_type(42);

    // Absent fields keep their default (Pydantic validates into a fresh model),
    // so we start from a freshly default-constructed instance.
    WithOptional v{};
    from_json(j, v);
    REQUIRE(v.id == 42);
    REQUIRE_FALSE(v.name.has_value());
    REQUIRE_FALSE(v.age.has_value());
}

struct RequiredOptional {
    std::optional<int> v;
};

TEST_CASE("reflection model: optional without default is required", "[model]") {
    // Exact 1:1 with Pydantic v2: `std::optional<T>` is *nullable*, not
    // optional; requiredness comes from a default member initializer.
    json_type j = json_type::object();
    try {
        (void)j.get<RequiredOptional>();
        FAIL("expected request_validation_error");
    } catch (const request_validation_error& e) {
        auto detail = to_fastapi_error_json(e.errors());
        REQUIRE_THAT(detail, ContainsSubstring(R"("type":"missing")"));
        REQUIRE_THAT(detail, ContainsSubstring(R"("loc":["v"])"));
    }
}

// ============================================================
// Vector fields
// ============================================================

struct WithVector {
    int id = 0;
    std::vector<int> scores;
    std::vector<std::string> tags;
};

TEST_CASE("reflection model vector fields", "[model]") {
    WithVector v{1, {90, 95, 88}, {"fast", "secure"}};

    json_type j;
    to_json(j, v);

    REQUIRE(j["id"].get<int>() == 1);
    REQUIRE(j["scores"].is_array());
    REQUIRE(j["scores"].size() == 3);
    REQUIRE(j["scores"][0].get<int>() == 90);
    REQUIRE(j["scores"][1].get<int>() == 95);
    REQUIRE(j["scores"][2].get<int>() == 88);
    REQUIRE(j["tags"].is_array());
    REQUIRE(j["tags"].size() == 2);
    REQUIRE(j["tags"][0].get<std::string>() == "fast");
    REQUIRE(j["tags"][1].get<std::string>() == "secure");

    WithVector v2;
    from_json(j, v2);
    REQUIRE(v2.id == 1);
    REQUIRE(v2.scores.size() == 3);
    REQUIRE(v2.scores[1] == 95);
    REQUIRE(v2.tags.size() == 2);
    REQUIRE(v2.tags[1] == "secure");
}

// ============================================================
// Single-field struct
// ============================================================

struct SingleField {
    std::string value;
};

TEST_CASE("reflection model single field", "[model]") {
    SingleField v{"only"};
    json_type j;
    to_json(j, v);
    REQUIRE(j["value"].get<std::string>() == "only");

    SingleField v2;
    from_json(j, v2);
    REQUIRE(v2.value == "only");
}

// ============================================================
// get<T>() with reflection model types
// ============================================================

TEST_CASE("reflection model get<T> from json_type", "[model]") {
    AllPrimitives v{1, 2, 3, 1.5f, 2.5, true, "get_test"};
    json_type j(v);

    auto v2 = j.get<AllPrimitives>();
    REQUIRE(v2.i == 1);
    REQUIRE(v2.s == "get_test");
    REQUIRE(v2.b == true);
}

}  // namespace fiasco::detail
