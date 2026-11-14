#pragma once

/// @file
/// @brief Runtime route and parameter descriptors used by matching and OpenAPI.
/// @internal

// Registration-time description of a route. This is the projection seam
// between the application layer and OpenAPI: handlers keep their reflection
// metadata only long enough to materialise plain runtime values here.

#include <string>
#include <vector>

#include "fiasco/http/detail/request.hpp"
#include "fiasco/meta/annotations.hpp"
#include "fiasco/types/json_schema.hpp"

namespace fiasco::detail {

struct parameter_descriptor {
    std::string name;
    fiasco::param_in source = fiasco::param_in::infer;
    fiasco::param_in declared_source = fiasco::param_in::infer;
    std::string type;
    // Empty for ordinary params, "path" for catch-all segments.
    std::string converter;
    bool required = false;
    // request/dependency parameters are injected rather than read from the
    // wire, so they never appear in the OpenAPI parameter list.
    bool injected = false;
    fiasco::field<> constraints{};
    // Request body schema; absent for non-body parameters.
    json_schema schema;
    bool has_schema = false;
};

struct route_descriptor {
    http_method method = http_method::get;
    std::string path;
    std::string name;
    std::string summary;
    std::string description;
    std::string operation_id;
    std::vector<std::string> tags;
    int status_code = 200;
    bool deprecated = false;
    bool include_in_schema = true;
    std::vector<parameter_descriptor> parameters;
    std::string response_description = "Successful Response";
    std::string response_media_type = "application/json";
    json_schema response_schema;
    bool has_response_schema = false;
};

}  // namespace fiasco::detail
