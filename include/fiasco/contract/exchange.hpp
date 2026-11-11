#pragma once

#include <functional>

#include "fiasco/contract/request.hpp"
#include "fiasco/contract/sink.hpp"
#include "fiasco/contract/task.hpp"

namespace fiasco::detail {

/// The transport-facing exchange: the request, the body source and the
/// response sink. `application_handler` is the type-erased seam.
struct exchange {
    request& req;
    body_source& body;
    response_sink& res;
};

using application_handler = std::move_only_function<task<void>(exchange&)>;

}  // namespace fiasco::detail
