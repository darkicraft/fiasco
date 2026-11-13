#include <catch2/catch_test_macros.hpp>

#include <asio/co_spawn.hpp>
#include <asio/deferred.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>
#include <asio/system_executor.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "fiasco/stream.hpp"

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
