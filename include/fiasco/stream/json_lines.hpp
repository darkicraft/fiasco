#pragma once

// JSON Lines helper built on top of fiasco::stream.
//
//   auto items() -> fiasco::stream<item> { ... }
//   return fiasco::json_lines(items());

#include <type_traits>
#include <utility>

#include "fiasco/stream/detail/serialized_source.hpp"
#include "fiasco/types/json.hpp"

namespace fiasco {

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
