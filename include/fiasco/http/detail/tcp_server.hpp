#pragma once

/// @file
/// @brief Internal TCP server implementation interface.
/// @internal

#include <cstdint>
#include <memory>
#include <string>

#include "fiasco/contract/exchange.hpp"

namespace fiasco::detail {

/// Multithreaded TCP transport for the HTTP application handler.
class tcp_server {
  public:
    /// Bind `host:port` and prepare worker threads. Port `0` is ephemeral.
    tcp_server(uint16_t port,
               const std::string& host,
               application_handler handler,
               unsigned int num_threads = 0);

    /// Run the accept loop and worker event loops until stop().
    void run();

    /// Stop all event loops.
    void stop() noexcept;

    /// Bound port, or `0` if the server is not running.
    [[nodiscard]] uint16_t port() const;

    ~tcp_server();

  private:
    void accept();

    struct impl;
    std::unique_ptr<impl> p_impl;
};

}  // namespace fiasco::detail
