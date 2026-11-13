#pragma once

// JSON Schema projection for reflected request/response models.
//
// Schema generation is registration-time work: OpenAPI only consumes the
// resulting plain JSON. Nested models become components/schemas entries and
// recursive models fall back to a $ref instead of expanding forever.

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "fiasco/meta/annotations.hpp"
#include "fiasco/types/json.hpp"

namespace fiasco::detail {

struct schema_definition {
    std::string name;
    json_type schema;
};

struct json_schema {
    json_type root;
    std::vector<schema_definition> definitions;
};

inline std::string schema_name(std::string_view display) {
    std::string name;
    name.reserve(display.size());
    for (char c : display) {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '_' || c == '.' || c == '-') {
            name.push_back(c);
        } else {
            name.push_back('_');
        }
    }
    return name;
}

inline void apply_openapi_constraints(json_type& schema, const fiasco::field<>& field) {
    if (field.gt.has) {
        schema["exclusiveMinimum"] = field.gt.value;
    }
    if (field.ge.has) {
        schema["minimum"] = field.ge.value;
    }
    if (field.lt.has) {
        schema["exclusiveMaximum"] = field.lt.value;
    }
    if (field.le.has) {
        schema["maximum"] = field.le.value;
    }
    if (field.multiple_of.has) {
        schema["multipleOf"] = field.multiple_of.value;
    }
    if (field.min_length.has) {
        schema["minLength"] = field.min_length.value;
    }
    if (field.max_length.has) {
        schema["maxLength"] = field.max_length.value;
    }
    if (field.pattern.has) {
        schema["pattern"] = std::string(field.pattern.view());
    }
    if (field.title.has) {
        schema["title"] = std::string(field.title.view());
    }
    if (field.description.has) {
        schema["description"] = std::string(field.description.view());
    }
}

struct schema_context {
    std::vector<schema_definition> definitions;
    std::unordered_map<std::string, std::size_t> definition_index;
    std::unordered_set<std::string> building;

    static json_type reference(const std::string& name) {
        json_type ref = json_type::object();
        ref["$ref"] = "#/components/schemas/" + name;
        return ref;
    }

    template <typename T, std::meta::info Member>
    json_type member_schema() {
        using Field = std::remove_cvref_t<decltype(std::declval<T&>().[:Member:])>;
        return schema_for<Field>();
    }

    template <typename T>
    json_type model_schema() {
        constexpr auto display = std::meta::display_string_of(^^T);
        const std::string name = schema_name(display);

        if (building.contains(name) || definition_index.contains(name)) {
            return reference(name);
        }
        building.insert(name);

        json_type object = json_type::object();
        object["type"] = "object";

        json_type properties = json_type::object();
        json_type required = json_type::array({});

        template for (constexpr auto member :
                      std::define_static_array(std::meta::nonstatic_data_members_of(
                          ^^T, std::meta::access_context::unchecked()))) {
            if constexpr (std::meta::has_identifier(member)) {
                constexpr auto spec = fiasco::detail::meta::field_of(member);
                constexpr std::string_view key =
                    spec.alias.has ? spec.alias.view() : std::meta::identifier_of(member);
                json_type property = member_schema<T, member>();
                apply_openapi_constraints(property, spec);
                properties[std::string(key)] = property;
                if constexpr (!std::meta::has_default_member_initializer(member)) {
                    required.push_back(std::string(key));
                }
            }
        }

        object["properties"] = properties;
        if (required.size() > 0) {
            object["required"] = required;
        }

        definition_index.emplace(name, definitions.size());
        definitions.push_back(schema_definition{name, object});
        building.erase(name);
        return reference(name);
    }

    template <typename T, std::size_t... Is>
    json_type tuple_schema(std::index_sequence<Is...>) {
        json_type prefix = json_type::array({});
        (prefix.push_back(schema_for<std::tuple_element_t<Is, T>>()), ...);

        json_type out = json_type::object();
        out["type"] = "array";
        out["prefixItems"] = prefix;
        out["minItems"] = sizeof...(Is);
        out["maxItems"] = sizeof...(Is);
        return out;
    }

    template <typename T>
    json_type schema_for() {
        using D = std::remove_cvref_t<T>;

        if constexpr (std::same_as<D, json_type>) {
            return json_type::object();
        } else if constexpr (std::same_as<D, bool>) {
            json_type out = json_type::object();
            out["type"] = "boolean";
            return out;
        } else if constexpr (std::integral<D>) {
            json_type out = json_type::object();
            out["type"] = "integer";
            return out;
        } else if constexpr (std::floating_point<D>) {
            json_type out = json_type::object();
            out["type"] = "number";
            return out;
        } else if constexpr (std::same_as<D, std::string>) {
            json_type out = json_type::object();
            out["type"] = "string";
            return out;
        } else if constexpr (JsonOptional<D>) {
            json_type out = json_type::object();
            json_type inner = schema_for<typename D::value_type>();
            json_type null = json_type::object();
            null["type"] = "null";
            json_type alternatives = json_type::array({});
            alternatives.push_back(inner);
            alternatives.push_back(null);
            out["anyOf"] = alternatives;
            return out;
        } else if constexpr (JsonMap<D>) {
            json_type out = json_type::object();
            out["type"] = "object";
            out["additionalProperties"] = schema_for<typename D::mapped_type>();
            return out;
        } else if constexpr (JsonTupleLike<D>) {
            return tuple_schema<D>(std::make_index_sequence<std::tuple_size_v<D>>{});
        } else if constexpr (JsonSequence<D>) {
            json_type out = json_type::object();
            out["type"] = "array";
            out["items"] = schema_for<typename D::value_type>();
            return out;
        } else if constexpr (JsonModel<D>) {
            return model_schema<D>();
        } else {
            return json_type::object();
        }
    }
};

template <typename T>
json_schema make_json_schema() {
    schema_context context;
    json_type root = context.schema_for<T>();
    return json_schema{std::move(root), std::move(context.definitions)};
}

}  // namespace fiasco::detail
