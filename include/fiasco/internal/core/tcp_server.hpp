#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/http/response.hpp"

namespace fiasco::detail {

// Move-only, for the same reason as routing's handler_fn: handlers may capture
// move-only state. Consequence: request_handler::operator() is non-const, so
// sessions must hold and invoke it through a non-const reference.
using request_handler = std::move_only_function<response(request)>;

class tcp_server {
  public:
    tcp_server(uint16_t port,
               const std::string& host,
               request_handler handler,
               unsigned int num_threads = 0);

    void run();
    void stop() noexcept;
    [[nodiscard]] uint16_t port() const;

    ~tcp_server();

  private:
    void accept();

    struct impl;
    std::unique_ptr<impl> p_impl;
};

}  // namespace fiasco::detail