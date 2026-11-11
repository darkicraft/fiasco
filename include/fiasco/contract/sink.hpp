#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

#include "fiasco/contract/task.hpp"

namespace fiasco::detail {

/// Push side of the ASGI-style boundary. Move-only callbacks keep the sink
/// type-erased without a virtual base.
struct response_sink {
    std::move_only_function<void(int)> status;
    std::move_only_function<void(std::string_view, std::string_view)> header;
    std::move_only_function<task<void>(std::span<const std::byte>)> write;
    std::move_only_function<task<void>()> end;
};

/// Pull side of the boundary. The buffered implementation returns the request
/// body once; a socket-backed source can replace it later.
struct body_source {
    std::move_only_function<task<std::optional<std::span<const std::byte>>>()> read;
};

}  // namespace fiasco::detail
