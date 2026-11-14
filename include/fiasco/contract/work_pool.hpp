#pragma once

/// @file
/// @brief Abstract offload interface for blocking work.
/// @internal

#include <functional>
#include <utility>

namespace fiasco::detail {

/// Offload interface for blocking or CPU-heavy work. The implementation lives
/// in the runtime layer (asio::thread_pool); the application only sees this.
struct work_pool {
    /// Submit a blocking task and receive its completion callback.
    std::move_only_function<void(std::move_only_function<void()>)> submit;
};

}  // namespace fiasco::detail
