#pragma once

#include <asio/awaitable.hpp>

#include <type_traits>
#include <utility>

#include "fiasco/http/detail/response.hpp"
#include "fiasco/http/detail/response_conversion.hpp"

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

/// Typed coroutine result. The type is carried in the function signature so
/// registration can infer the OpenAPI response schema.
template <typename T>
class typed_async_result {
  public:
    using value_type = T;

    typed_async_result() = default;

    template <ResponseConvertible U>
        requires(!std::same_as<clean_type_t<U>, typed_async_result>)
    explicit typed_async_result(U&& value)
        : m_response(to_response(std::forward<U>(value))) {}

    [[nodiscard]] response take() && { return std::move(m_response); }

  private:
    response m_response;
};

/// Untyped async handler. Use this when no response schema should be inferred.
using async = asio::awaitable<async_result>;

/// Typed async handler: `fiasco::async_t<Item>`.
template <typename T>
using async_t = asio::awaitable<typed_async_result<T>>;

}  // namespace fiasco::detail
