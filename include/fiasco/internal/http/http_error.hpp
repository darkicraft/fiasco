#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "fiasco/internal/types/json.hpp"

namespace fiasco::detail {

/// An exception that maps directly to an HTTP response, similar to FastAPI's
/// HTTPException. Handlers and dependencies may throw it to short-circuit
/// request processing. The detail may be any JSON value.
class http_error : public std::runtime_error {
  public:
    explicit http_error(int status_code,
                        std::string detail,
                        std::unordered_map<std::string, std::string> headers = {})
        : http_error(status_code, json_type(std::move(detail)), std::move(headers)) {}

    explicit http_error(int status_code,
                        const char* detail,
                        std::unordered_map<std::string, std::string> headers = {})
        : http_error(status_code, std::string(detail), std::move(headers)) {}

    explicit http_error(int status_code,
                        json_type detail,
                        std::unordered_map<std::string, std::string> headers = {})
        : std::runtime_error(message_for(detail)),
          m_status_code(status_code),
          m_detail(std::move(detail)),
          m_headers(std::move(headers)) {}

    [[nodiscard]] int status_code() const noexcept { return m_status_code; }
    [[nodiscard]] const json_type& detail() const noexcept { return m_detail; }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& headers() const noexcept {
        return m_headers;
    }

  private:
    [[nodiscard]] static std::string message_for(const json_type& detail) {
        if (detail.is_string()) {
            return detail.get<std::string>();
        }
        return detail.dump();
    }

    int m_status_code;
    json_type m_detail;
    std::unordered_map<std::string, std::string> m_headers;
};

}  // namespace fiasco::detail
