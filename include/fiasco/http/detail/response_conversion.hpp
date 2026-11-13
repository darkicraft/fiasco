#pragma once

#include <concepts>
#include <string>
#include <type_traits>
#include <utility>

#include "fiasco/http/detail/response.hpp"
#include "fiasco/types/json.hpp"

namespace fiasco::detail {

template <typename T>
using clean_type_t = std::remove_cvref_t<T>;

template <typename T>
inline constexpr bool is_char_array_v =
    std::is_array_v<clean_type_t<T>> &&
    std::same_as<std::remove_cv_t<std::remove_extent_t<clean_type_t<T>>>, char>;

template <typename T>
concept ResponseConvertible =
    std::same_as<clean_type_t<T>, response> || std::same_as<clean_type_t<T>, std::string> ||
    std::same_as<clean_type_t<T>, const char*> || std::same_as<clean_type_t<T>, char*> ||
    is_char_array_v<T> || std::same_as<clean_type_t<T>, bool> || std::integral<clean_type_t<T>> ||
    std::floating_point<clean_type_t<T>> || JsonSerializable<clean_type_t<T>>;

/// Convert a handler return value into an HTTP response. This is shared by
/// synchronous handlers and the async coroutine result type so both accept the
/// same values.
template <typename T>
[[nodiscard]] response to_response(T&& value) {
    using clean = clean_type_t<T>;

    if constexpr (std::same_as<clean, response>) {
        return std::forward<T>(value);
    } else if constexpr (std::same_as<clean, std::string>) {
        return response::text(std::forward<T>(value));
    } else if constexpr (std::same_as<clean, const char*> || std::same_as<clean, char*> ||
                         is_char_array_v<T>) {
        return response::text(std::string(std::forward<T>(value)));
    } else if constexpr (std::same_as<clean, bool> || std::integral<clean> ||
                         std::floating_point<clean>) {
        return response::text(std::to_string(value));
    } else if constexpr (JsonSerializable<clean>) {
        json_type j = std::forward<T>(value);
        return response::json(j.dump());
    } else {
        static_assert(!sizeof(T),
                      "Handler return type must be a primitive, fiasco::response, "
                      "or a reflectable model type");
        return {};
    }
}

}  // namespace fiasco::detail
