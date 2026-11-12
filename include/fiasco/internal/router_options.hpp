#pragma once

#include <string>
#include <vector>

namespace fiasco {

/// Router-level configuration. Cascades into the routes it owns: the prefix is
/// concatenated, tags append, scalars most-specific-wins.
struct router_options {
    std::string prefix;
    std::vector<std::string> tags;
    std::vector<std::string> responses;
    bool deprecated = false;
    bool include_in_schema = true;
    bool redirect_slashes = true;
};

}  // namespace fiasco
