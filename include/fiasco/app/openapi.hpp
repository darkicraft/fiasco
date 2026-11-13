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
    if (type.find("int") != std::string_view::npos || type.find("long") != std::string_view::npos ||
        type.find("short") != std::string_view::npos ||
        type.find("size_t") != std::string_view::npos) {
        return "integer";
    }
    return "string";
}

inline void merge_schema_definitions(json_type& components, const json_schema& schema) {
    for (const auto& definition : schema.definitions) {
        if (!components.contains(definition.name)) {
            components[definition.name] = definition.schema;
        }
    }
}

inline json_type build_openapi_parameter(const parameter_descriptor& parameter) {
    json_type out = json_type::object();
    out["name"] = parameter.name;
    out["in"] = openapi_source_name(parameter.source);
    out["required"] = parameter.required;

    json_type schema = json_type::object();
    if (parameter.converter == "path") {
        schema["type"] = "string";
    } else {
        schema["type"] = openapi_scalar_type(parameter.type);
    }
    apply_openapi_constraints(schema, parameter.constraints);
    out["schema"] = schema;

    return out;
}

inline json_type build_openapi_operation(const route_descriptor& descriptor,
                                         json_type& components) {
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
    std::vector<const parameter_descriptor*> body_parameters;
    for (const auto& parameter : descriptor.parameters) {
        if (parameter.source == fiasco::param_in::body && parameter.has_schema) {
            body_parameters.push_back(&parameter);
        }
        if (parameter.injected || openapi_source_name(parameter.source) == nullptr) {
            continue;
        }
        parameters.push_back(build_openapi_parameter(parameter));
    }
    if (parameters.size() > 0) {
        operation["parameters"] = parameters;
    }

    if (!body_parameters.empty()) {
        json_type request_body = json_type::object();
        json_type schema = json_type::object();
        bool required = false;

        if (body_parameters.size() == 1) {
            const auto& body = *body_parameters.front();
            schema = body.schema.root;
            required = body.required;
            merge_schema_definitions(components, body.schema);
        } else {
            schema["type"] = "object";
            json_type properties = json_type::object();
            json_type required_names = json_type::array({});
            for (const auto* body : body_parameters) {
                properties[body->name] = body->schema.root;
                if (body->required) {
                    required_names.push_back(body->name);
                }
                merge_schema_definitions(components, body->schema);
            }
            schema["properties"] = properties;
            if (required_names.size() > 0) {
                schema["required"] = required_names;
            }
            required = required_names.size() > 0;
        }

        request_body["required"] = required;
        json_type content = json_type::object();
        json_type media = json_type::object();
        media["schema"] = schema;
        content["application/json"] = media;
        request_body["content"] = content;
        operation["requestBody"] = request_body;
    }

    json_type response = json_type::object();
    response["description"] = descriptor.response_description;
    if (descriptor.has_response_schema) {
        json_type content = json_type::object();
        json_type media = json_type::object();
        media["schema"] = descriptor.response_schema.root;
        content[descriptor.response_media_type] = media;
        response["content"] = content;
        merge_schema_definitions(components, descriptor.response_schema);
    }
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
    json_type schema_components = json_type::object();
    routes.for_each_descriptor([&](const route_descriptor& descriptor) {
        if (!descriptor.include_in_schema || descriptor.path.empty()) {
            return;
        }

        if (!paths.contains(descriptor.path)) {
            paths[descriptor.path] = json_type::object();
        }
        paths[descriptor.path][openapi_method_name(descriptor.method)] =
            build_openapi_operation(descriptor, schema_components);
    });
    document["paths"] = paths;

    if (schema_components.size() > 0) {
        json_type components = json_type::object();
        components["schemas"] = schema_components;
        document["components"] = components;
    }

    return document;
}

}  // namespace fiasco::detail
