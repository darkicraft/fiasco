#pragma once

/// @file
/// @brief Pull-based async generator used by streaming responses.

/// A pull-based async sequence for streamed responses.
///
/// Producers use `co_yield` for chunks, may await async work between chunks,
/// and finish with `co_return;`. The transport pulls one chunk at a time.
///
/// @code
/// auto events() -> fiasco::stream<std::string> {
///     co_yield "first";
///     co_await some_async_operation;
///     co_yield "second";
///     co_return;
/// }
/// @endcode
///
/// Destroying or cancelling a stream while a pull is in flight cancels the
/// awaited operation and resolves that pull with `std::nullopt`.

#include <asio/any_io_executor.hpp>
#include <asio/async_result.hpp>
#include <asio/awaitable.hpp>
#include <asio/cancellation_signal.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/post.hpp>
#include <asio/system_error.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>

#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

namespace fiasco {

namespace stream_detail {

inline void interpret_result(std::tuple<>&&) {}

template <typename... Args>
auto interpret_result(std::tuple<Args...>&& args) {
    return std::move(args);
}

template <typename... Args>
auto interpret_result(std::tuple<std::exception_ptr, Args...>&& args) {
    if (std::get<0>(args)) {
        std::rethrow_exception(std::get<0>(args));
    }

    return std::apply([](auto&&, auto&&... rest) { return std::make_tuple(std::move(rest)...); },
                      std::move(args));
}

template <typename... Args>
auto interpret_result(std::tuple<asio::error_code, Args...>&& args) {
    if (std::get<0>(args)) {
        throw asio::system_error(std::get<0>(args));
    }

    return std::apply([](auto&&, auto&&... rest) { return std::make_tuple(std::move(rest)...); },
                      std::move(args));
}

template <typename Arg>
auto interpret_result(std::tuple<Arg>&& args) {
    return std::get<0>(std::move(args));
}

template <typename Arg>
auto interpret_result(std::tuple<std::exception_ptr, Arg>&& args) {
    if (std::get<0>(args)) {
        std::rethrow_exception(std::get<0>(args));
    }
    return std::get<1>(std::move(args));
}

inline void interpret_result(std::tuple<asio::error_code>&& args) {
    if (std::get<0>(args)) {
        throw asio::system_error(std::get<0>(args));
    }
}

inline void interpret_result(std::tuple<std::exception_ptr>&& args) {
    if (std::get<0>(args)) {
        std::rethrow_exception(std::get<0>(args));
    }
}

template <typename Arg>
auto interpret_result(std::tuple<asio::error_code, Arg>&& args) {
    if (std::get<0>(args)) {
        throw asio::system_error(std::get<0>(args));
    }
    return std::get<1>(std::move(args));
}

}  // namespace stream_detail

/// A pull-based async sequence of chunks.
///
/// Producers are coroutines that use `co_yield` for chunks and `co_return;` to
/// finish. Consumers pull with `next()`. Destroying or cancelling a stream
/// cancels an in-flight pull and resolves it with `std::nullopt`.
template <typename T>
class stream {
  public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct state {
        explicit state(handle_type h)
            : handle(h) {}

        ~state();
        void cancel() noexcept;

        handle_type handle{};
        bool pull_active = false;
    };

    stream(const stream&) = delete;
    stream& operator=(const stream&) = delete;

    stream(stream&& other) noexcept
        : m_state(std::exchange(other.m_state, {})) {}

    stream& operator=(stream&& other) noexcept {
        if (this != &other) {
            reset();
            m_state = std::exchange(other.m_state, {});
        }
        return *this;
    }

    ~stream() { reset(); }

    /// Cancel the stream. The active or next pull resolves with `std::nullopt`.
    void cancel() noexcept { reset(); }

    /// Pull the next chunk.
    ///
    /// @return The next chunk, or `std::nullopt` when the producer finishes or
    ///         the stream is cancelled.
    /// @throws std::exception Propagates an exception from the producer.
    [[nodiscard]] auto next() -> asio::awaitable<std::optional<T>> {
        auto state = m_state;
        if (!state || !state->handle || state->handle.done()) {
            if (state && state->handle && state->handle.promise().error) {
                std::rethrow_exception(state->handle.promise().error);
            }
            co_return std::nullopt;
        }

        auto& promise = state->handle.promise();
        promise.executor = co_await asio::this_coro::executor;
        state->pull_active = true;

        std::optional<T> value;
        try {
            auto token = asio::use_awaitable_t<>{};
            value = co_await asio::async_initiate<asio::use_awaitable_t<>, void(std::optional<T>)>(
                pull_initiation{state}, token);
        } catch (...) {
            state->pull_active = false;
            throw;
        }
        state->pull_active = false;

        if (promise.cancelled) {
            co_return std::nullopt;
        }
        if (promise.error) {
            std::rethrow_exception(promise.error);
        }
        co_return value;
    }

    struct promise_type {
        std::optional<T> current;
        std::exception_ptr error;
        asio::any_io_executor executor;
        std::move_only_function<void(std::optional<T>)> pending;
        asio::cancellation_signal cancel_signal;
        bool cancelled = false;
        bool awaiting_async = false;

        struct yield_awaiter {
            promise_type* promise;

            bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<promise_type>) noexcept {
                if (!promise->cancelled) {
                    promise->deliver(std::move(promise->current));
                }
                promise->current.reset();
            }

            void await_resume() const {
                if (promise->cancelled) {
                    throw asio::system_error(asio::error::operation_aborted);
                }
            }
        };

        struct final_awaiter {
            promise_type* promise;

            bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<promise_type>) noexcept {
                promise->deliver(std::nullopt);
            }

            void await_resume() const noexcept {}
        };

        template <typename... Args>
        struct operation_handler {
            promise_type* promise;
            std::coroutine_handle<promise_type> handle;
            std::optional<std::tuple<Args...>>* result;

            using cancellation_slot_type = asio::cancellation_slot;

            auto get_cancellation_slot() const noexcept -> cancellation_slot_type {
                return promise->cancel_signal.slot();
            }

            void operator()(Args... args) {
                promise->awaiting_async = false;
                result->emplace(std::move(args)...);
                handle.resume();
            }
        };

        template <typename Signature>
        struct operation_traits;

        template <typename... Args>
        struct operation_traits<void(Args...)> {
            using result_type = std::tuple<Args...>;
            using handler = operation_handler<Args...>;
        };

        template <typename Op>
        struct operation_awaiter {
            using signature = asio::completion_signature_of_t<Op>;
            using result_type = typename operation_traits<signature>::result_type;

            promise_type* promise;
            Op op;
            std::optional<result_type> result;

            bool await_ready() const noexcept { return promise->cancelled; }

            void await_suspend(std::coroutine_handle<promise_type> handle) {
                promise->awaiting_async = true;
                std::move(op)(
                    typename operation_traits<signature>::handler{promise, handle, &result});
            }

            auto await_resume() {
                if (promise->cancelled) {
                    throw asio::system_error(asio::error::operation_aborted);
                }
                return stream_detail::interpret_result(std::move(result).value());
            }
        };

        auto get_return_object() -> stream {
            const auto handle = handle_type::from_promise(*this);
            return stream{std::make_shared<state>(handle)};
        }

        auto initial_suspend() noexcept -> std::suspend_always { return {}; }

        auto final_suspend() noexcept -> final_awaiter { return {this}; }

        void return_void() noexcept {}

        auto yield_value(T value) -> yield_awaiter {
            current.emplace(std::move(value));
            return {this};
        }

        void unhandled_exception() noexcept { error = std::current_exception(); }

        auto await_transform(asio::this_coro::executor_t) const noexcept {
            struct awaiter {
                asio::any_io_executor value;

                bool await_ready() const noexcept { return true; }
                void await_suspend(std::coroutine_handle<>) const noexcept {}
                auto await_resume() const noexcept -> asio::any_io_executor { return value; }
            };
            return awaiter{executor};
        }

        template <typename Op>
            requires asio::is_async_operation<std::decay_t<Op>>::value
        auto await_transform(Op&& op) {
            return operation_awaiter<std::decay_t<Op>>{this, std::forward<Op>(op)};
        }

        void deliver(std::optional<T> value) noexcept {
            auto handler = std::move(pending);

            if (!handler) {
                return;
            }

            if (executor) {
                asio::post(executor,
                           [handler = std::move(handler), value = std::move(value)]() mutable {
                               std::move(handler)(std::move(value));
                           });
            } else {
                std::move(handler)(std::move(value));
            }
        }
    };

  private:
    struct pull_initiation {
        std::shared_ptr<state> source;

        template <typename Handler>
        void operator()(Handler&& handler) {
            auto& promise = source->handle.promise();
            promise.pending = std::forward<Handler>(handler);
            source->handle.resume();
        }
    };

    void reset() noexcept {
        auto state = std::exchange(m_state, {});
        if (state) {
            state->cancel();
        }
    }

    explicit stream(std::shared_ptr<state> state)
        : m_state(std::move(state)) {}

    std::shared_ptr<state> m_state;
};

template <typename T>
stream<T>::state::~state() {
    if (!handle) {
        return;
    }

    auto& promise = handle.promise();
    if (!handle.done() && (promise.awaiting_async || pull_active)) {
        promise.cancelled = true;
        promise.cancel_signal.emit(asio::cancellation_type::all);
        // The coroutine still owns a suspended async operation. Its completion
        // path is the only safe place to destroy the frame.
        handle = {};
        return;
    }

    handle.destroy();
    handle = {};
}

template <typename T>
void stream<T>::state::cancel() noexcept {
    if (!handle) {
        return;
    }

    auto& promise = handle.promise();
    if (pull_active) {
        promise.cancelled = true;
        if (promise.awaiting_async) {
            promise.cancel_signal.emit(asio::cancellation_type::all);
            return;
        }
        if (promise.pending) {
            promise.deliver(std::nullopt);
        }
        return;
    }

    if (!handle.done() && promise.awaiting_async) {
        promise.cancelled = true;
        promise.cancel_signal.emit(asio::cancellation_type::all);
        return;
    }

    handle.destroy();
    handle = {};
}

}  // namespace fiasco
