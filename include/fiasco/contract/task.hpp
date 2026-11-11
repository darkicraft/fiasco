#pragma once

#include <asio/awaitable.hpp>

namespace fiasco::detail {

// Temporary alias until fiasco::task lands. The contract already owns the
// name so callers do not depend on Asio directly.
template <typename T = void>
using task = asio::awaitable<T>;

}  // namespace fiasco::detail
