#pragma once

/// @file
/// @brief Network and worker configuration for the server runtime.

// Public server runtime configuration. Kept separate from `app_options`:
// this describes the transport, not the API document.

#include <cstdint>
#include <string>

namespace fiasco {

/// Network listener and worker settings.
struct network_config {
    /// Address to bind. Use `127.0.0.1` for local-only access.
    std::string host = "0.0.0.0";
    /// TCP port to bind. `0` asks the operating system for an ephemeral port.
    std::uint16_t port = 8080;
    /// Worker count. `0` uses hardware concurrency with a minimum of two.
    unsigned int workers = 0;
};

/// Transport configuration passed to `fiasco::server`.
struct server_config {
    /// Network listener settings.
    network_config network{};
};

}  // namespace fiasco
