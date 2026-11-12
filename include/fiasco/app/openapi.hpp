#pragma once

// OpenAPI document projection. The route descriptors are the only source of
// truth here: this file never reflects handler types.

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "fiasco/app/app_options.hpp"
#include "fiasco/internal/json.hpp"
#include "fiasco/internal/router.hpp"
#include "fiasco/internal/routing/descriptor.hpp"

namespace fiasco::detail {

inline std::string openapi_method_name(http_method method) {
    std::string name = method_to_string(method);
    for (char& c : name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return name;
}

inline const char* openapi_source_name(fiasco::param_in source) {
    switch (source) {
    case fiasco::param_in::path:
        return "path";
    case fiasco::param_in::query:
        return "query";
    case fiasco::param_in::header:
        return "header";
    case fiasco::param_in::cookie:
        return "cookie";
    default:
        return nullptr;
    }
}

inline const char* openapi_scalar_type(std::string_view type) {
    if (type.find("bool") != std::string_view::npos) {
        return "boolean";
    }
    if (type.find("double") != std::string_view::npos ||
        type.find("float") != std::string_view::npos) {
        return "number";
    }
    if (type.find("int") != std::string_view::npos ||
        type.find("long") != std::string_view::npos ||
        type.find("short") != std::string_view::npos ||
        type.find("size_t") != std::string_view::npos) {
        return "integer";
    }
    return "string";
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

inline json_type build_openapi_parameter(const parameter_descriptor& parameter) {
    json_type out = json_type::object();
    out["name"] = parameter.name;
    out["in"] = openapi_source_name(parameter.source);
    out["required"] = parameter.required;

    json_type schema = json_type::object();
    if (parameter.converter == "int") {
        schema["type"] = "integer";
    } else if (parameter.converter == "path") {
        schema["type"] = "string";
    } else {
        schema["type"] = openapi_scalar_type(parameter.type);
    }
    apply_openapi_constraints(schema, parameter.constraints);
    out["schema"] = schema;

    return out;
}

inline json_type build_openapi_operation(const route_descriptor& descriptor) {
    json_type operation = json_type::object();

    if (!descriptor.summary.empty()) {
        operation["summary"] = descriptor.summary;
    }
    if (!descriptor.description.empty()) {
        operation["description"] = descriptor.description;
    }
    if (!descriptor.operation_id.empty()) {
        operation["operationId"] = descriptor.operation_id;
    }
    if (descriptor.deprecated) {
        operation["deprecated"] = true;
    }
    if (!descriptor.tags.empty()) {
        json_type tags = json_type::array({});
        for (const auto& tag : descriptor.tags) {
            tags.push_back(tag);
        }
        operation["tags"] = tags;
    }

    json_type parameters = json_type::array({});
    for (const auto& parameter : descriptor.parameters) {
        if (parameter.injected || openapi_source_name(parameter.source) == nullptr) {
            continue;
        }
        parameters.push_back(build_openapi_parameter(parameter));
    }
    if (parameters.size() > 0) {
        operation["parameters"] = parameters;
    }

    json_type response = json_type::object();
    response["description"] = descriptor.response_description;
    json_type responses = json_type::object();
    responses[std::to_string(descriptor.status_code)] = response;
    operation["responses"] = responses;

    return operation;
}

inline json_type build_openapi(const fiasco::app_options& options, const router& routes) {
    json_type document = json_type::object();
    document["openapi"] = "3.1.0";

    json_type info = json_type::object();
    info["title"] = options.title;
    info["version"] = options.version;
    if (!options.description.empty()) {
        info["description"] = options.description;
    }
    if (!options.summary.empty()) {
        info["summary"] = options.summary;
    }
    if (!options.terms_of_service.empty()) {
        info["termsOfService"] = options.terms_of_service;
    }
    if (!options.contact.name.empty() || !options.contact.url.empty() ||
        !options.contact.email.empty()) {
        json_type contact = json_type::object();
        if (!options.contact.name.empty()) {
            contact["name"] = options.contact.name;
        }
        if (!options.contact.url.empty()) {
            contact["url"] = options.contact.url;
        }
        if (!options.contact.email.empty()) {
            contact["email"] = options.contact.email;
        }
        info["contact"] = contact;
    }
    if (!options.license.name.empty() || !options.license.url.empty()) {
        json_type license = json_type::object();
        if (!options.license.name.empty()) {
            license["name"] = options.license.name;
        }
        if (!options.license.url.empty()) {
            license["url"] = options.license.url;
        }
        info["license"] = license;
    }
    document["info"] = info;

    if (!options.openapi_tags.empty()) {
        json_type tags = json_type::array({});
        for (const auto& tag : options.openapi_tags) {
            json_type entry = json_type::object();
            entry["name"] = tag;
            tags.push_back(entry);
        }
        document["tags"] = tags;
    }

    json_type paths = json_type::object();
    routes.for_each_descriptor([&](const route_descriptor& descriptor) {
        if (!descriptor.include_in_schema || descriptor.path.empty()) {
            return;
        }

        if (!paths.contains(descriptor.path)) {
            paths[descriptor.path] = json_type::object();
        }
        paths[descriptor.path][openapi_method_name(descriptor.method)] =
            build_openapi_operation(descriptor);
    });
    document["paths"] = paths;

    return document;
}

}  // namespace fiasco::detail
