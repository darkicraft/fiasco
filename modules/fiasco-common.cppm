module;

#include "fiasco/types/json.hpp"

export module fiasco.common;

export namespace fiasco {
    using json = detail::json;
}
