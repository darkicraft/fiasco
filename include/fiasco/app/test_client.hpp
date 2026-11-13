#pragma once

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "fiasco/app/app.hpp"
#include "fiasco/contract/exchange.hpp"

namespace fiasco::detail {

/// In-process test client: calls the application exchange directly. No socket,
/// no port, no thread. The buffering sink captures the response; async handlers
/// are driven on a local `io_context`.
class test_client {
  public:
    explicit test_client(app& app)
        : m_app(app) {}

    response handle(request req) {
        response out;
        response_sink sink{
            .status = [&out](int status) { out.status_code = status; },
            .header =
                [&out](std::string_view key, std::string_view value) {
                    if (key == "Set-Cookie") {
                        out.set_cookie_headers.emplace_back(value);
                    } else {
                        out.headers[std::string(key)] = std::string(value);
                    }
                },
            .write = [&out](std::span<const std::byte> bytes) -> task<void> {
                out.body.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                co_return;
            },
            .end = []() -> task<void> { co_return; },
        };

        std::string_view body_view = req.body;
        body_source body{
            .read = [body_view, pos = std::size_t{0}]() mutable
                -> task<std::optional<std::span<const std::byte>>> {
                if (pos >= body_view.size()) {
                    co_return std::nullopt;
                }
                auto chunk =
                    std::as_bytes(std::span(body_view.data() + pos, body_view.size() - pos));
                pos = body_view.size();
                co_return chunk;
            },
        };

        exchange ex{req, body, sink};
        asio::io_context io;
        asio::co_spawn(
            io,
            [this, &ex]() -> task<void> {
                co_await m_app.handle(ex);
                co_return;
            },
            asio::detached);
        io.run();
        return out;
    }

  private:
    app& m_app;
};

}  // namespace fiasco::detail
