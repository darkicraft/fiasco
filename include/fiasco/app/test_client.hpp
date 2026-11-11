#pragma once

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>

#include <utility>
#include <variant>

#include "fiasco/app/app.hpp"
#include "fiasco/contract/result.hpp"

namespace fiasco::detail {

/// In-process test client: calls `app::handle` directly. No socket, no port,
/// no thread. Async handlers are driven on a local `io_context` so the call
/// still returns a `response`.
class test_client {
  public:
    explicit test_client(app& app) : m_app(app) {}

    response handle(request req) {
        auto result = m_app.handle(std::move(req));
        if (auto* res = std::get_if<response>(&result)) {
            return *res;
        }

        asio::io_context io;
        response out;
        bool done = false;
        asio::co_spawn(
            io,
            [result = std::move(result), &out, &done]() mutable -> asio::awaitable<void> {
                auto res = co_await std::get<async>(std::move(result));
                out = std::move(res).take();
                done = true;
                co_return;
            },
            asio::detached);
        io.run();
        (void)done;
        return out;
    }

  private:
    app& m_app;
};

}  // namespace fiasco::detail
