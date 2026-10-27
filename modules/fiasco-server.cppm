module;

#include "fiasco/internal/server.hpp"

export module fiasco.server;
export import fiasco.common;

export namespace fiasco {
    using http_error = detail::http_error;
    using request = detail::request;
    using server = detail::server;
}
