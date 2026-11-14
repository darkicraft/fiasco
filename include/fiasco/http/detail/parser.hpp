#pragma once

/// @file
/// @brief Internal llhttp-based HTTP request parser.
/// @internal

#include <memory>
#include <string>

#include "fiasco/http/detail/request.hpp"

namespace fiasco::detail {

/// Incremental HTTP/1.1 request parser.
class llhttp_parser {
  public:
    llhttp_parser();

    /// Feed a chunk of bytes. Returns false when parsing fails.
    bool feed(const char* data, size_t len);

    /// True once a complete request is available.
    [[nodiscard]] bool is_complete() const noexcept;

    /// Move the completed request out and reset the parser.
    [[nodiscard]] request take_request();

    /// Parser error text after a failed feed.
    [[nodiscard]] const std::string& error() const noexcept;

    /// Reset parser state for the next request.
    void reset();

    ~llhttp_parser();

  private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};

}  // namespace fiasco::detail
