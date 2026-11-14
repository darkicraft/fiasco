#pragma once

/// @file
/// @brief Application middleware chain and next-handler contract.
/// @internal

#include <asio/awaitable.hpp>

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "fiasco/http/detail/async.hpp"
#include "fiasco/http/detail/request.hpp"
#include "fiasco/http/detail/response.hpp"

namespace fiasco::detail {

class middleware_chain;

using endpoint_fn = std::move_only_function<asio::awaitable<response>(request&)>;

class next_handler {
  public:
    next_handler(middleware_chain& chain, std::size_t index, endpoint_fn& endpoint, bool& called)
        : m_chain(&chain),
          m_index(index),
          m_endpoint(&endpoint),
          m_called(&called) {}

    asio::awaitable<response> operator()(request& req) const;

  private:
    middleware_chain* m_chain;
    std::size_t m_index;
    endpoint_fn* m_endpoint;
    bool* m_called;
};

using middleware_fn = std::move_only_function<async(request&, const next_handler&)>;

class middleware_chain {
  public:
    void add(middleware_fn middleware) {
        m_middleware.insert(m_middleware.begin(), std::move(middleware));
    }

    [[nodiscard]] bool empty() const noexcept { return m_middleware.empty(); }

    asio::awaitable<response> run(request& req, endpoint_fn endpoint) {
        co_return co_await invoke(0, req, endpoint);
    }

  private:
    friend class next_handler;

    asio::awaitable<response> invoke(std::size_t index, request& req, endpoint_fn& endpoint) {
        if (index >= m_middleware.size()) {
            co_return co_await endpoint(req);
        }

        bool called = false;
        next_handler next{*this, index + 1, endpoint, called};
        auto result = co_await m_middleware[index](req, next);
        co_return std::move(result).take();
    }

    std::vector<middleware_fn> m_middleware;
};

inline asio::awaitable<response> next_handler::operator()(request& req) const {
    if (*m_called) {
        throw std::logic_error("middleware called next more than once");
    }
    *m_called = true;
    co_return co_await m_chain->invoke(m_index, req, *m_endpoint);
}

}  // namespace fiasco::detail

namespace fiasco::middleware {

using fn = detail::middleware_fn;

}  // namespace fiasco::middleware
