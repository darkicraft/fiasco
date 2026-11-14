#pragma once

/// @file
/// @brief JSON Lines streaming response helper.

/// JSON Lines response helper.
///
/// @code
/// auto items() -> fiasco::stream<Item> { ... }
/// return fiasco::json_lines(items());
/// @endcode

#include <type_traits>
#include <utility>

#include "fiasco/stream/detail/serialized_source.hpp"
#include "fiasco/types/json.hpp"

namespace fiasco {

/// Create a JSON Lines response from a stream of values.
///
/// Each value is serialized as one JSON value followed by a newline.
template <typename T>
    requires detail::JsonSerializable<std::remove_cvref_t<T>>
inline auto json_lines(stream<T> values) -> detail::response {
    auto result = stream_detail::make_serialized_response(
        "application/x-ndjson", std::move(values), [](const T& value) {
            return detail::json_type(value).dump() + "\n";
        });
    result.headers["Cache-Control"] = "no-cache";
    return result;
}

}  // namespace fiasco
