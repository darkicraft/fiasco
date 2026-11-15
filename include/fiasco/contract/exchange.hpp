#pragma once

/// @file
/// @brief Transport-facing request/response exchange boundary.
/// @internal

#include "fiasco/contract/request.hpp"
#include "fiasco/contract/result.hpp"
#include "fiasco/contract/sink.hpp"

namespace fiasco::detail {

/// The transport-facing exchange: the request, the body source and the
/// response sink. Kept for streaming and test-client paths.
struct exchange {
    /// Parsed request for this exchange.
    request& req;
    /// Request body source.
    body_source& body;
    /// Response sink owned by the transport.
    response_sink& res;
};

/// Transport-facing application entry points.
///
/// `handle` is the synchronous dispatch seam used for buffered responses.
/// `prepare` finalizes file responses, and `write` owns the generic sink path
/// used by async, streamed and file responses.
struct application {
    std::move_only_function<request_result(request&)> handle;
    std::move_only_function<void(const request&, response&)> prepare;
    std::move_only_function<task<void>(response_sink&, response&)> write;
};

}  // namespace fiasco::detail
