#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "fiasco/internal/json.hpp"

using namespace fiasco::detail;

// Structs live in fiasco::detail so ADL finds the reflection serialisers.
namespace fiasco::detail {

// ---- constraint fixtures ---------------------------------------------------
struct CIntGt  { [[= field{.gt = 5}]]           int v = 0; };
struct CIntGe  { [[= field{.ge = 5}]]           int v = 0; };
struct CIntLt  { [[= field{.lt = 5}]]           int v = 0; };
struct CIntLe  { [[= field{.le = 5}]]           int v = 0; };
struct CIntMul { [[= field{.multiple_of = 3}]]  int v = 0; };
struct CFloatGt { [[= field{.gt = 5}]]          double v = 0.0; };

struct CStrMin  { [[= field{.min_length = 3}]] std::string v; };
struct CStrMax  { [[= field{.max_length = 2}]] std::string v; };
struct CListMin { [[= field{.min_length = 2}]] std::vector<int> v; };
struct CListMax { [[= field{.max_length = 2}]] std::vector<int> v; };

// singular bounds
struct CStrMin1  { [[= field{.min_length = 1}]] std::string v; };
struct CStrMax1  { [[= field{.max_length = 1}]] std::string v; };
struct CListMin1 { [[= field{.min_length = 1}]] std::vector<int> v; };
struct CListMax1 { [[= field{.max_length = 1}]] std::vector<int> v; };

struct COptInt { [[= field{.gt = 5}]] std::optional<int> v = std::nullopt; };

struct CWithAlias {
    [[= field{.alias = "last-name"}]] std::string lname = "";
};

struct CAll {
    [[= field{.gt = 0, .le = 100}]] int percent = 0;
    [[= field{.min_length = 2}]]    std::string code = "";
};

// ---- helper ----------------------------------------------------------------
template <class T>
std::string must_fail(const json_type& j) {
    try {
        (void)j.get<T>();
        return "<no error>";
    } catch (const request_validation_error& e) {
        return to_fastapi_error_json(e.errors());
    }
}

json_type obj_with(const char* key, json_type value) {
    json_type j = json_type::object();
    j[key] = std::move(value);
    return j;
}

}  // namespace fiasco::detail

TEST_CASE("field gt emits Pydantic greater_than", "[field]") {
    REQUIRE(must_fail<CIntGt>(obj_with("v", json_type(5))) ==
            R"({"detail":[{"type":"greater_than","loc":["v"],"msg":"Input should be greater than 5","input":5,"ctx":{"gt":5}}]})");
}

TEST_CASE("field ge emits Pydantic greater_than_equal", "[field]") {
    REQUIRE(must_fail<CIntGe>(obj_with("v", json_type(4))) ==
            R"({"detail":[{"type":"greater_than_equal","loc":["v"],"msg":"Input should be greater than or equal to 5","input":4,"ctx":{"ge":5}}]})");
}

TEST_CASE("field lt emits Pydantic less_than", "[field]") {
    REQUIRE(must_fail<CIntLt>(obj_with("v", json_type(5))) ==
            R"({"detail":[{"type":"less_than","loc":["v"],"msg":"Input should be less than 5","input":5,"ctx":{"lt":5}}]})");
}

TEST_CASE("field le emits Pydantic less_than_equal", "[field]") {
    REQUIRE(must_fail<CIntLe>(obj_with("v", json_type(6))) ==
            R"({"detail":[{"type":"less_than_equal","loc":["v"],"msg":"Input should be less than or equal to 5","input":6,"ctx":{"le":5}}]})");
}

TEST_CASE("field multiple_of emits Pydantic multiple_of", "[field]") {
    REQUIRE(must_fail<CIntMul>(obj_with("v", json_type(4))) ==
            R"({"detail":[{"type":"multiple_of","loc":["v"],"msg":"Input should be a multiple of 3","input":4,"ctx":{"multiple_of":3}}]})");
}

TEST_CASE("field bound on a float field renders ctx as a float", "[field]") {
    REQUIRE(must_fail<CFloatGt>(obj_with("v", json_type(0.0))) ==
            R"({"detail":[{"type":"greater_than","loc":["v"],"msg":"Input should be greater than 5","input":0.0,"ctx":{"gt":5.0}}]})");
}

TEST_CASE("field min_length on a string emits string_too_short", "[field]") {
    REQUIRE(must_fail<CStrMin>(obj_with("v", json_type("ab"))) ==
            R"({"detail":[{"type":"string_too_short","loc":["v"],"msg":"String should have at least 3 characters","input":"ab","ctx":{"min_length":3}}]})");
}

TEST_CASE("field max_length on a string emits string_too_long", "[field]") {
    REQUIRE(must_fail<CStrMax>(obj_with("v", json_type("abc"))) ==
            R"({"detail":[{"type":"string_too_long","loc":["v"],"msg":"String should have at most 2 characters","input":"abc","ctx":{"max_length":2}}]})");
}

TEST_CASE("field min_length on a list emits too_short", "[field]") {
    json_type arr = json_type::array({json_type(1)});
    REQUIRE(must_fail<CListMin>(obj_with("v", arr)) ==
            R"({"detail":[{"type":"too_short","loc":["v"],"msg":"List should have at least 2 items after validation, not 1","input":[1],"ctx":{"field_type":"List","min_length":2,"actual_length":1}}]})");
}

TEST_CASE("field max_length on a list emits too_long", "[field]") {
    json_type arr = json_type::array({json_type(1), json_type(2), json_type(3)});
    REQUIRE(must_fail<CListMax>(obj_with("v", arr)) ==
            R"({"detail":[{"type":"too_long","loc":["v"],"msg":"List should have at most 2 items after validation, not 3","input":[1,2,3],"ctx":{"field_type":"List","max_length":2,"actual_length":3}}]})");
}

TEST_CASE("field constraints apply to the inner value of an optional", "[field]") {
    REQUIRE(must_fail<COptInt>(obj_with("v", json_type(0))) ==
            R"({"detail":[{"type":"greater_than","loc":["v"],"msg":"Input should be greater than 5","input":0,"ctx":{"gt":5}}]})");
    // null and absent are fine
    json_type j = json_type::object();
    j["v"] = json_type(nullptr);
    REQUIRE(j.get<COptInt>().v.has_value() == false);
    REQUIRE(json_type::object().get<COptInt>().v.has_value() == false);
}

TEST_CASE("field alias changes the wire key for read and write", "[field]") {
    json_type j = obj_with("last-name", json_type("Smith"));
    auto v = j.get<CWithAlias>();
    REQUIRE(v.lname == "Smith");

    json_type out = v;
    REQUIRE(out.contains("last-name"));
    REQUIRE_FALSE(out.contains("lname"));
}

TEST_CASE("Pydantic uses singular nouns for a bound of 1", "[field]") {
    REQUIRE(must_fail<CStrMin1>(obj_with("v", json_type(""))) ==
            R"({"detail":[{"type":"string_too_short","loc":["v"],"msg":"String should have at least 1 character","input":"","ctx":{"min_length":1}}]})");
    REQUIRE(must_fail<CStrMax1>(obj_with("v", json_type("ab"))) ==
            R"({"detail":[{"type":"string_too_long","loc":["v"],"msg":"String should have at most 1 character","input":"ab","ctx":{"max_length":1}}]})");
    REQUIRE(must_fail<CListMin1>(obj_with("v", json_type::array({}))) ==
            R"({"detail":[{"type":"too_short","loc":["v"],"msg":"List should have at least 1 item after validation, not 0","input":[],"ctx":{"field_type":"List","min_length":1,"actual_length":0}}]})");
    json_type two = json_type::array({json_type(1), json_type(2)});
    REQUIRE(must_fail<CListMax1>(obj_with("v", two)) ==
            R"({"detail":[{"type":"too_long","loc":["v"],"msg":"List should have at most 1 item after validation, not 2","input":[1,2],"ctx":{"field_type":"List","max_length":1,"actual_length":2}}]})");
}

TEST_CASE("a valid value satisfying constraints passes", "[field]") {
    json_type j = json_type::object();
    j["percent"] = json_type(50);
    j["code"] = json_type("abc");
    auto v = j.get<CAll>();
    REQUIRE(v.percent == 50);
    REQUIRE(v.code == "abc");
}

TEST_CASE("all constraint failures are collected", "[field]") {
    json_type j = json_type::object();
    j["percent"] = json_type(200);
    j["code"] = json_type("a");
    std::string detail = must_fail<CAll>(j);
    REQUIRE(detail.find(R"("loc":["percent"])") != std::string::npos);
    REQUIRE(detail.find(R"("loc":["code"])") != std::string::npos);
}
