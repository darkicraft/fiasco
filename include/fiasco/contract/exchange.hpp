#pragma once

/// @file
/// @brief Transport-facing request/response exchange boundary.
/// @internal

#include "fiasco/contract/request.hpp"
#include "fiasco/contract/sink.hpp"

namespace fiasco::detail {

/// The transport-facing exchange: the request, the body source and the
/// response sink. `application_handler` is the type-erased seam.
struct exchange {
    /// Parsed request for this exchange.
    request& req;
    /// Request body source.
    body_source& body;
    /// Response sink owned by the transport.
    response_sink& res;
};

/// Type-erased handler driven by a transport.
using application_handler = std::move_only_function<task<void>(exchange&)>;

}  // namespace fiasco::detail
