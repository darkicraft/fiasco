#pragma once

/// @file
/// @brief Result variants used between the application and transport.
/// @internal

#include <functional>
#include <variant>

#include "fiasco/contract/request.hpp"
#include "fiasco/contract/response.hpp"
#include "fiasco/http/detail/async.hpp"

namespace fiasco::detail {

// A request completes either synchronously or as a coroutine. Keeping both
// alternatives lets the transport preserve the synchronous fast path.
/// A request completes synchronously or asynchronously.
using request_result = std::variant<response, async>;

// Move-only, for the same reason as routing's handler_fn: handlers may capture
// move-only state. request_handler::operator() is non-const.
/// Type-erased synchronous/async request handler.
using request_handler = std::move_only_function<request_result(request)>;

}  // namespace fiasco::detail
