#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace fiasco::detail {

/// An exception that maps directly to an HTTP response, similar to FastAPI's
/// HTTPException. Handlers and dependencies may throw it to short-circuit
/// request processing.
class http_error : public std::runtime_error {
  public:
    explicit http_error(int status_code,
                        std::string detail,
                        std::unordered_map<std::string, std::string> headers = {})
        : std::runtime_error(detail),
          m_status_code(status_code),
          m_detail(std::move(detail)),
          m_headers(std::move(headers)) {}

    [[nodiscard]] int status_code() const noexcept { return m_status_code; }
    [[nodiscard]] const std::string& detail() const noexcept { return m_detail; }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& headers() const noexcept {
        return m_headers;
    }

  private:
    int m_status_code;
    std::string m_detail;
    std::unordered_map<std::string, std::string> m_headers;
};

}  // namespace fiasco::detail
