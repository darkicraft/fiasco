#pragma once

#include "fiasco/internal/http/async.hpp"
#include "fiasco/internal/http/http_error.hpp"
#include "fiasco/internal/http/response.hpp"

#include "fiasco/internal/json.hpp"
#include "fiasco/internal/logging.hpp"
#include "fiasco/internal/middleware.hpp"
#include "fiasco/internal/router.hpp"
#include "fiasco/middleware/cors.hpp"
#include "fiasco/internal/server.hpp"
#include "fiasco/app/app.hpp"

namespace fiasco {

using app = detail::app;
using async = detail::async;
using http_error = detail::http_error;
using json = detail::json;
using next = detail::next_handler;
using request = detail::request;
using response = detail::response;
using router = detail::router;
using server = detail::server;
using logger = detail::logger;

}  // namespace fiasco