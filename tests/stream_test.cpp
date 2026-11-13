#include <catch2/catch_test_macros.hpp>

#include <asio/buffer.hpp>
#include <asio/co_spawn.hpp>
#include <asio/deferred.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/post.hpp>
#include <asio/read.hpp>
#include <asio/steady_timer.hpp>
#include <asio/system_executor.hpp>
#include <asio/write.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "fiasco/app/test_client.hpp"
#include "fiasco/fiasco.hpp"
#include "fiasco/stream.hpp"
#include "support/test_routes.hpp"
#include "support/test_server.hpp"

namespace {

fiasco::stream<std::string> text_stream() {
    co_yield "one";
    co_yield std::string("two");
    co_return;
}

fiasco::stream<std::string> failing_stream() {
    co_yield "before";
    throw std::runtime_error("stream failed");
}

fiasco::stream<std::unique_ptr<int>> pointer_stream() {
    co_yield std::make_unique<int>(7);
    co_return;
}

fiasco::stream<int> executor_stream() {
    auto executor = co_await asio::this_coro::executor;
    co_await asio::post(executor, asio::deferred);
    co_yield 1;
    co_return;
}

template <typename Token>
auto async_error(Token&& token) {
    return asio::async_initiate<Token, void(asio::error_code)>(
        [](auto&& handler) {
            asio::post(asio::system_executor{},
                       [handler = std::forward<decltype(handler)>(handler)]() mutable {
                           std::move(handler)(asio::error::operation_aborted);
                       });
        },
        token);
}

template <typename Token>
auto async_value(int value, Token&& token) {
    return asio::async_initiate<Token, void(asio::error_code, int)>(
        [value](auto&& handler) {
            asio::post(asio::system_executor{},
                       [handler = std::forward<decltype(handler)>(handler), value]() mutable {
                           std::move(handler)(asio::error_code{}, value);
                       });
        },
        token);
}

fiasco::stream<int> awaited_failure_stream() {
    co_await async_error(asio::deferred);
    co_yield 1;
}

fiasco::stream<std::string> response_stream() {
    co_yield "first";
    co_yield "second";
    co_return;
}

std::string raw_http_request(std::uint16_t port, std::string_view request) {
    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    socket.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port));
    asio::write(socket, asio::buffer(request.data(), request.size()));

    std::string response;
    std::array<char, 4096> buffer{};
    asio::error_code error;
    while (!error) {
        const auto count = socket.read_some(asio::buffer(buffer), error);
        response.append(buffer.data(), count);
    }
    return response;
}

std::string decode_chunked_body(std::string_view body) {
    std::string decoded;
    std::size_t pos = 0;
    while (pos < body.size()) {
        const auto line_end = body.find("\r\n", pos);
        REQUIRE(line_end != std::string_view::npos);
        const auto size = std::stoul(std::string(body.substr(pos, line_end - pos)), nullptr, 16);
        pos = line_end + 2;
        if (size == 0) {
            break;
        }
        REQUIRE(pos + size <= body.size());
        decoded.append(body.substr(pos, size));
        pos += size + 2;
    }
    return decoded;
}

fiasco::stream<int> delayed_stream(asio::steady_timer& timer) {
    timer.expires_after(std::chrono::milliseconds(1));
    co_await timer.async_wait(asio::deferred);
    co_yield 2;

    const int value = co_await async_value(42, asio::deferred);
    co_yield value;
    co_return;
}

}  // namespace

TEST_CASE("stream yields chunks and then completes", "[stream]") {
    std::vector<std::string> chunks;
    bool completed = false;

    asio::io_context io;
    asio::co_spawn(
        io,
        [&]() -> asio::awaitable<void> {
            auto stream = text_stream();
            while (auto chunk = co_await stream.next()) {
                chunks.push_back(std::move(*chunk));
            }
            completed = !(co_await stream.next()).has_value();
        }(),
        asio::detached);
    io.run();

    REQUIRE(chunks == std::vector<std::string>{"one", "two"});
    REQUIRE(completed);
}

TEST_CASE("stream propagates producer exceptions after yielded chunks", "[stream]") {
    std::vector<std::string> chunks;
    std::string error;

    asio::io_context io;
    asio::co_spawn(
        io,
        [&]() -> asio::awaitable<void> {
            auto stream = failing_stream();
            try {
                while (auto chunk = co_await stream.next()) {
                    chunks.push_back(std::move(*chunk));
                }
            } catch (const std::runtime_error& ex) {
                error = ex.what();
            }
        }(),
        asio::detached);
    io.run();

    REQUIRE(chunks == std::vector<std::string>{"before"});
    REQUIRE(error == "stream failed");
}

TEST_CASE("stream propagates awaited operation errors", "[stream]") {
    bool caught = false;

    asio::io_context io;
    asio::co_spawn(
        io,
        [&]() -> asio::awaitable<void> {
            auto stream = awaited_failure_stream();
            try {
                (void)co_await stream.next();
            } catch (const asio::system_error& ex) {
                caught = ex.code() == asio::error::operation_aborted;
            }
        }(),
        asio::detached);
    io.run();

    REQUIRE(caught);
}

TEST_CASE("stream supports move-only chunks", "[stream]") {
    int value = 0;

    asio::io_context io;
    asio::co_spawn(
        io,
        [&]() -> asio::awaitable<void> {
            auto stream = pointer_stream();
            auto chunk = co_await stream.next();
            if (chunk && *chunk) {
                value = **chunk;
            }
        }(),
        asio::detached);
    io.run();

    REQUIRE(value == 7);
}

TEST_CASE("stream awaits deferred operations and exposes its executor", "[stream]") {
    std::vector<int> values;

    asio::io_context io;
    asio::steady_timer timer(io);
    asio::co_spawn(
        io,
        [&]() -> asio::awaitable<void> {
            auto stream = delayed_stream(timer);
            while (auto chunk = co_await stream.next()) {
                values.push_back(*chunk);
            }

            auto executor = executor_stream();
            while (auto chunk = co_await executor.next()) {
                values.push_back(*chunk);
            }
        }(),
        asio::detached);
    io.run();

    REQUIRE(values == std::vector<int>{2, 42, 1});
}

TEST_CASE("response::stream is buffered by the test client", "[stream][response]") {
    fiasco::app app;
    fiasco_test::get(app, "/events", []() -> fiasco::response {
        return fiasco::response::stream("text/event-stream", response_stream());
    });

    fiasco::test_client client{app};
    fiasco::request req;
    req.method = fiasco::detail::http_method::get;
    req.url = "/events";
    req.reanchor_url_views();

    auto res = client.handle(std::move(req));
    REQUIRE(res.status_code == 200);
    REQUIRE(res.headers.at("Content-Type") == "text/event-stream");
    REQUIRE(res.body == "firstsecond");
    REQUIRE_FALSE(res.headers.contains("Content-Length"));
}

TEST_CASE("server streams chunked response bodies", "[stream][integration]") {
    fiasco_test::test_server server;
    fiasco_test::get(server.app(), "/events", []() -> fiasco::response {
        return fiasco::response::stream("text/event-stream", response_stream());
    });
    server.start();

    const auto raw = raw_http_request(server.port(),
                                      "GET /events HTTP/1.1\r\n"
                                      "Host: localhost\r\n"
                                      "Connection: close\r\n"
                                      "\r\n");

    REQUIRE(raw.starts_with("HTTP/1.1 200 OK\r\n"));
    REQUIRE(raw.find("Content-Type: text/event-stream\r\n") != std::string::npos);
    REQUIRE(raw.find("Transfer-Encoding: chunked\r\n") != std::string::npos);
    REQUIRE(raw.find("Content-Length:") == std::string::npos);

    const auto header_end = raw.find("\r\n\r\n");
    REQUIRE(header_end != std::string::npos);
    REQUIRE(decode_chunked_body(std::string_view(raw).substr(header_end + 4)) == "firstsecond");
}

TEST_CASE("stream is movable", "[stream]") {
    std::vector<std::string> chunks;

    asio::io_context io;
    asio::co_spawn(
        io,
        [&]() -> asio::awaitable<void> {
            auto original = text_stream();
            auto moved = std::move(original);
            while (auto chunk = co_await moved.next()) {
                chunks.push_back(std::move(*chunk));
            }
        }(),
        asio::detached);
    io.run();

    REQUIRE(chunks == std::vector<std::string>{"one", "two"});
}
