#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

#include "fiasco/internal/logging/log_level.hpp"

namespace fiasco::detail {

struct log_record {
    log_level level = log_level::info;
    std::string logger_name;
    std::string message;
    std::unordered_map<std::string, std::string> fields;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

} // namespace fiasco::detail
