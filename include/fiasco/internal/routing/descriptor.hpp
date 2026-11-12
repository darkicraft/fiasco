#pragma once

// Registration-time description of a route. This is the projection seam
// between the application layer and OpenAPI: handlers keep their reflection
// metadata only long enough to materialise plain runtime values here.

#include <string>
#include <vector>

#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/meta/annotations.hpp"

namespace fiasco::detail {

struct parameter_descriptor {
    std::string name;
    fiasco::param_in source = fiasco::param_in::infer;
    fiasco::param_in declared_source = fiasco::param_in::infer;
    std::string type;
    // Empty for ordinary params, "int" or "path" for converted path segments.
    std::string converter;
    bool required = false;
    // request/dependency parameters are injected rather than read from the
    // wire, so they never appear in the OpenAPI parameter list.
    bool injected = false;
    fiasco::field<> constraints{};
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
};

}  // namespace fiasco::detail
