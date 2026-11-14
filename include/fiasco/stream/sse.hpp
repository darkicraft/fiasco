#pragma once

/// @file
/// @brief Server-Sent Events streaming helper.

/// Server-Sent Events helpers.
///
/// @code
/// auto events() -> fiasco::stream<fiasco::sse_event> {
///     co_yield fiasco::sse_event{.event = "tick", .data = "1"};
///     co_return;
/// }
///
/// return fiasco::sse(events());
/// @endcode

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "fiasco/stream/detail/serialized_source.hpp"

namespace fiasco {

/// One Server-Sent Events message.
struct sse_event {
    /// Event payload. Newlines are split across multiple `data:` fields.
    std::string data;
    /// Optional event name.
    std::string event;
    /// Optional event identifier.
    std::string id;
    /// Optional reconnection delay in milliseconds.
    std::optional<std::uint32_t> retry;
};

namespace sse_detail {

inline void validate_field(std::string_view value, const char* name) {
    if (value.find('\r') != std::string_view::npos || value.find('\n') != std::string_view::npos) {
        throw std::invalid_argument(std::string(name) + " must not contain CR or LF");
    }
}

inline auto serialize(const sse_event& event) -> std::string {
    validate_field(event.event, "event");
    validate_field(event.id, "id");

    std::string out;
    if (!event.id.empty()) {
        out += "id: ";
        out += event.id;
        out += '\n';
    }
    if (!event.event.empty()) {
        out += "event: ";
        out += event.event;
        out += '\n';
    }
    if (event.retry.has_value()) {
        out += "retry: ";
        out += std::to_string(*event.retry);
        out += '\n';
    }

    std::size_t start = 0;
    while (start <= event.data.size()) {
        const auto end = event.data.find('\n', start);
        out += "data: ";
        out.append(event.data, start, end == std::string::npos ? std::string::npos : end - start);
        out += '\n';
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    out += '\n';
    return out;
}

}  // namespace sse_detail

/// Create an SSE response from a stream of events.
inline auto sse(stream<sse_event> events) -> detail::response {
    auto result = stream_detail::make_serialized_response(
        "text/event-stream", std::move(events), [](const sse_event& event) {
            return sse_detail::serialize(event);
        });
    result.headers["Cache-Control"] = "no-cache";
    result.headers["Connection"] = "keep-alive";
    result.headers["X-Accel-Buffering"] = "no";
    return result;
}

/// Serialize one event using the Server-Sent Events wire format.
inline auto serialize_sse_event(const sse_event& event) -> std::string {
    return sse_detail::serialize(event);
}

}  // namespace fiasco
