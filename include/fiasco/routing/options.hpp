#pragma once

/// @file
/// @brief Router-level configuration and cascade rules.

#include <string>
#include <vector>

namespace fiasco {

/// Router-level configuration. Cascades into the routes it owns: the prefix is
/// concatenated, tags append, scalars most-specific-wins.
struct router_options {
    /// Path prefix applied to every route in the router.
    std::string prefix;
    /// Tags appended to every route in the router.
    std::vector<std::string> tags;
    /// Shared response metadata for the router.
    std::vector<std::string> responses;
    /// Mark all routes in the router as deprecated in OpenAPI.
    bool deprecated = false;
    /// Include the router's routes in the OpenAPI document.
    bool include_in_schema = true;
    /// Reserved for future per-router trailing-slash control. The application
    /// currently uses `app_options::redirect_slashes` for redirects.
    bool redirect_slashes = true;
};

}  // namespace fiasco
