#pragma once

/// @file
/// @brief Umbrella header for the public Fiasco API.

#include "fiasco/http/detail/async.hpp"
#include "fiasco/http/detail/response.hpp"
#include "fiasco/http/error.hpp"

#include "fiasco/app/app.hpp"
#include "fiasco/app/test_client.hpp"
#include "fiasco/http/server.hpp"
#include "fiasco/http/server_config.hpp"
#include "fiasco/middleware/cors.hpp"
#include "fiasco/middleware/middleware.hpp"
#include "fiasco/routing/router.hpp"
#include "fiasco/stream/generator.hpp"
#include "fiasco/stream/json_lines.hpp"
#include "fiasco/stream/sse.hpp"
#include "fiasco/types/json.hpp"

/// Public Fiasco API.
namespace fiasco {

/// Application object: routes, middleware, exception handlers and metadata.
using app = detail::app;

/// Untyped asynchronous handler.
using async = detail::async;

/// Asynchronous handler with an explicit response value type.
template <typename T>
using async_t = detail::async_t<T>;

/// Options for setting and deleting response cookies.
using cookie_options = detail::cookie_options;

/// Options for serving a file response.
using file_options = detail::file_options;

/// Exception that maps to an HTTP error response.
using http_error = detail::http_error;

/// Dynamic JSON value.
using json = detail::json;

/// The next handler passed to middleware.
using next = detail::next_handler;

/// HTTP request data.
using request = detail::request;

/// HTTP response data and response factories.
using response = detail::response;

/// A route collection that can be included in an app or another router.
using router = detail::router;

/// Options for static-file mounts.
using static_options = detail::static_options;

/// In-process test client for exercising an app without opening a socket.
using test_client = detail::test_client;

}  // namespace fiasco

namespace fiasco::detail {

inline void app::run() {
    fiasco::server srv(handler(), fiasco::server_config{});
    srv.run();
}

}  // namespace fiasco::detail
