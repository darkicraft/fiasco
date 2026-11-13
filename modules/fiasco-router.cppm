module;

#include "fiasco/routing/router.hpp"

export module fiasco.router;
export import fiasco.common;

export namespace fiasco {
    using router = detail::router;
}
