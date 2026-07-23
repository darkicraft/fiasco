#pragma once

#include <cstdint>
#include <string>

#include "fiasco/internal/http/request.hpp"

namespace fiasco::detail {

struct access_log {
    std::string client;
    http_method method;
    std::string_view path;
    int status_code;
    uint64_t duration_us;

    [[nodiscard]] std::string build_message() const;
};

} // namespace fiasco::detail
