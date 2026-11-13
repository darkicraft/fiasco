#pragma once

// Public server runtime configuration. Kept separate from `app_options`:
// this describes the transport, not the API document.

#include <cstdint>
#include <string>

namespace fiasco {

struct network_config {
    std::string host = "0.0.0.0";
    std::uint16_t port = 8080;
    unsigned int workers = 0;  // 0 = hardware concurrency, at least 2
};

struct server_config {
    network_config network{};
};

}  // namespace fiasco
