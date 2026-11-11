#pragma once

#include <functional>
#include <variant>

#include "fiasco/contract/request.hpp"
#include "fiasco/contract/response.hpp"
#include "fiasco/internal/http/async.hpp"

namespace fiasco::detail {

// A request completes either synchronously or as a coroutine. Keeping both
// alternatives lets the transport preserve the synchronous fast path.
using request_result = std::variant<response, async>;

// Move-only, for the same reason as routing's handler_fn: handlers may capture
// move-only state. request_handler::operator() is non-const.
using request_handler = std::move_only_function<request_result(request)>;

}  // namespace fiasco::detail
