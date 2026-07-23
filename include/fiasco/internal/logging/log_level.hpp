#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fiasco::detail {

enum class log_level : uint8_t {
    debug = 0,
    info = 1,
    warn = 2,
    error = 3,
};

inline const char* to_string(log_level lvl) {
    switch (lvl) {
    case log_level::debug: return "debug";
    case log_level::info:  return "info";
    case log_level::warn:  return "warn";
    case log_level::error: return "error";
    }
    return "unknown";
}

inline log_level log_level_from_string(std::string_view s) {
    if (s == "debug" || s == "0") return log_level::debug;
    if (s == "info"  || s == "1") return log_level::info;
    if (s == "warn"  || s == "2") return log_level::warn;
    if (s == "error" || s == "3") return log_level::error;
    throw std::invalid_argument("unknown log level: " + std::string(s));
}

} // namespace fiasco::detail
