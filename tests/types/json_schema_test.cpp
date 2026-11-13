#include <catch2/catch_test_macros.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fiasco/types/json_schema.hpp"

using namespace fiasco::detail;

namespace {

struct address {
    std::string city;
};

struct profile {
    std::string name [[=fiasco::field{.min_length = 2}]];
    std::optional<int> age;
    address home;
    std::vector<std::string> tags;
    std::map<std::string, int> scores;
    std::pair<int, std::string> window;
    int level = 1;
};

const json_type* find_definition(const json_schema& schema, std::string_view name) {
    for (const auto& definition : schema.definitions) {
        if (definition.name == name) {
            return &definition.schema;
        }
    }
    return nullptr;
}

bool contains_string(const json_type& values, std::string_view value) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i].get<std::string>() == value) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("JSON Schema expands reflected models into components", "[json_schema]") {
    const json_schema schema = make_json_schema<profile>();
    const std::string profile_name = schema_name(std::meta::display_string_of(^^profile));
    const std::string address_name = schema_name(std::meta::display_string_of(^^address));

    CHECK(schema.root["$ref"] == "#/components/schemas/" + profile_name);

    const json_type* profile_schema = find_definition(schema, profile_name);
    REQUIRE(profile_schema != nullptr);
    CHECK((*profile_schema)["type"] == "object");
    CHECK((*profile_schema)["properties"]["name"]["type"] == "string");
    CHECK((*profile_schema)["properties"]["name"]["minLength"] == 2.0);
    CHECK((*profile_schema)["properties"]["age"]["anyOf"].size() == 2);
    CHECK((*profile_schema)["properties"]["age"]["anyOf"][1]["type"] == "null");
    CHECK((*profile_schema)["properties"]["home"]["$ref"] ==
          "#/components/schemas/" + address_name);
    CHECK((*profile_schema)["properties"]["tags"]["type"] == "array");
    CHECK((*profile_schema)["properties"]["tags"]["items"]["type"] == "string");
    CHECK((*profile_schema)["properties"]["scores"]["type"] == "object");
    CHECK((*profile_schema)["properties"]["scores"]["additionalProperties"]["type"] == "integer");
    CHECK((*profile_schema)["properties"]["window"]["type"] == "array");
    CHECK((*profile_schema)["properties"]["window"]["prefixItems"].size() == 2);
    CHECK((*profile_schema)["properties"]["window"]["minItems"] == 2);
    CHECK((*profile_schema)["properties"]["level"]["type"] == "integer");

    REQUIRE((*profile_schema)["required"].is_array());
    CHECK((*profile_schema)["required"].size() == 6);
    CHECK(contains_string((*profile_schema)["required"], "name"));
    CHECK(contains_string((*profile_schema)["required"], "age"));
    CHECK_FALSE(contains_string((*profile_schema)["required"], "level"));

    const json_type* address_schema = find_definition(schema, address_name);
    REQUIRE(address_schema != nullptr);
    CHECK((*address_schema)["properties"]["city"]["type"] == "string");
    REQUIRE((*address_schema)["required"].is_array());
    CHECK((*address_schema)["required"].size() == 1);
}

TEST_CASE("JSON Schema keeps scalar and container roots inline", "[json_schema]") {
    const json_schema scalar = make_json_schema<int>();
    CHECK(scalar.root["type"] == "integer");
    CHECK(scalar.definitions.empty());

    const json_schema sequence = make_json_schema<std::vector<std::string>>();
    CHECK(sequence.root["type"] == "array");
    CHECK(sequence.root["items"]["type"] == "string");
    CHECK(sequence.definitions.empty());

    const json_schema optional = make_json_schema<std::optional<std::string>>();
    CHECK(optional.root["anyOf"].size() == 2);
    CHECK(optional.root["anyOf"][0]["type"] == "string");
    CHECK(optional.root["anyOf"][1]["type"] == "null");
}
