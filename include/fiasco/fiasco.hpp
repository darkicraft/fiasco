#pragma once

#include "fiasco/http/detail/async.hpp"
#include "fiasco/http/error.hpp"
#include "fiasco/http/detail/response.hpp"

#include "fiasco/app/app.hpp"
#include "fiasco/app/test_client.hpp"
#include "fiasco/types/json.hpp"
#include "fiasco/middleware/middleware.hpp"
#include "fiasco/routing/router.hpp"
#include "fiasco/middleware/cors.hpp"
#include "fiasco/http/server.hpp"
#include "fiasco/http/server_config.hpp"
#include "fiasco/stream/generator.hpp"

namespace fiasco {

using app = detail::app;
using async = detail::async;
template <typename T>
using async_t = detail::async_t<T>;
using cookie_options = detail::cookie_options;
using file_options = detail::file_options;
using http_error = detail::http_error;
using json = detail::json;
using next = detail::next_handler;
using request = detail::request;
using response = detail::response;
using router = detail::router;
using static_options = detail::static_options;
using test_client = detail::test_client;

}  // namespace fiasco

namespace fiasco::detail {

inline void app::run() {
    fiasco::server srv(handler(), fiasco::server_config{});
    srv.run();
}

}  // namespace fiasco::detail
