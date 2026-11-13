#pragma once

#include <cstdint>
#include <memory>

#include "fiasco/contract/exchange.hpp"
#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/http/response.hpp"

namespace fiasco::detail {

class tcp_server {
  public:
    tcp_server(uint16_t port,
               const std::string& host,
               application_handler handler,
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
