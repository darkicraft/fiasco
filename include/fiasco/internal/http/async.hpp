#pragma once

#include <asio/awaitable.hpp>

#include <type_traits>
#include <utility>

#include "fiasco/internal/http/response.hpp"
#include "fiasco/internal/http/response_conversion.hpp"

namespace fiasco::detail {

/// The value produced by an async handler before it is turned into an HTTP
/// response. Keeping this as an internal wrapper lets `co_return model` work
/// without exposing response construction to handler authors.
class async_result {
  public:
    async_result() = default;

    template <ResponseConvertible T>
        requires(!std::same_as<clean_type_t<T>, async_result>)
    explicit async_result(T&& value)
        : m_response(to_response(std::forward<T>(value))) {}

    [[nodiscard]] response take() && { return std::move(m_response); }

  private:
    response m_response;
};

using async = asio::awaitable<async_result>;

}  // namespace fiasco::detail
