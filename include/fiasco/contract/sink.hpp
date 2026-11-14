#pragma once

/// @file
/// @brief Push sink and pull body-source contracts used by transports.
/// @internal

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

#include "fiasco/contract/task.hpp"

namespace fiasco::detail {

struct body_source;

/// Push side of the ASGI-style boundary. Move-only callbacks keep the sink
/// type-erased without a virtual base.
struct response_sink {
    /// Set the HTTP status code.
    std::move_only_function<void(int)> status;
    /// Append one response header.
    std::move_only_function<void(std::string_view, std::string_view)> header;
    /// Write one body chunk.
    ///
    /// The span remains valid until the returned task completes.
    std::move_only_function<task<void>(std::span<const std::byte>)> write;
    /// Finish the response. Transports may close or reuse the connection after
    /// this completes.
    std::move_only_function<task<void>()> end;

    /// Optional transport hooks for streamed response bodies. The application
    /// calls these around the pull loop; transports use them to watch for a
    /// disconnected peer without polling.
    std::move_only_function<void(body_source*)> begin_stream;
    std::move_only_function<void()> end_stream;
};

/// Pull side of the boundary. The buffered implementation returns the request
/// body once; a socket-backed source can replace it later.
struct body_source {
    /// Pull the next body chunk, or nullopt at end of body.
    std::move_only_function<task<std::optional<std::span<const std::byte>>>()> read;
    /// Cancel an in-flight body pull.
    std::move_only_function<void()> cancel;
};

}  // namespace fiasco::detail
