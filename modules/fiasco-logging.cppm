module;

#include "fiasco/internal/logging/logger.hpp"

export module fiasco.logging;

export namespace fiasco {
    using logger = detail::logger;
}
