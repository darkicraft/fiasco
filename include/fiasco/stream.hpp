#pragma once

// Pull-based async generator used by streaming responses.
//
//   auto events() -> fiasco::stream<std::string> {
//       co_yield "data: one\n\n";
//       co_await timer.async_wait(asio::deferred);
//       co_yield "data: two\n\n";
//       co_return;
//   }
//
// The transport pulls one chunk at a time. The promise uses return_void, so
// `co_return;` ends the stream and `co_return value;` is intentionally invalid.
//
// Await expressions inside a stream use the ordinary Asio deferred form:
//
//   co_await socket.async_read_some(buffer, asio::deferred);

#include <asio/any_io_executor.hpp>
#include <asio/async_result.hpp>
#include <asio/awaitable.hpp>
#include <asio/deferred.hpp>
#include <asio/error_code.hpp>
#include <asio/post.hpp>
#include <asio/system_error.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>

#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <system_error>
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

template <typename T>
class stream {
  public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    stream(const stream&) = delete;
    stream& operator=(const stream&) = delete;

    stream(stream&& other) noexcept
        : m_handle(std::exchange(other.m_handle, {})) {}

    stream& operator=(stream&& other) noexcept {
        if (this != &other) {
            if (m_handle) {
                m_handle.destroy();
            }
            m_handle = std::exchange(other.m_handle, {});
        }
        return *this;
    }

    ~stream() {
        if (m_handle) {
            m_handle.destroy();
        }
    }

    /// Pull the next chunk. Returns nullopt when the producer finishes.
    [[nodiscard]] auto next() -> asio::awaitable<std::optional<T>> {
        if (!m_handle || m_handle.done()) {
            if (m_handle && m_handle.promise().error) {
                std::rethrow_exception(m_handle.promise().error);
            }
            co_return std::nullopt;
        }

        auto& promise = m_handle.promise();
        promise.executor = co_await asio::this_coro::executor;

        auto token = asio::use_awaitable_t<>{};
        auto value = co_await asio::async_initiate<asio::use_awaitable_t<>, void(std::optional<T>)>(
            pull_initiation{this}, token);

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

        struct yield_awaiter {
            promise_type* promise;

            bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<promise_type>) noexcept {
                promise->deliver(std::move(promise->current));
                promise->current.reset();
            }

            void await_resume() const noexcept {}
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
            std::coroutine_handle<promise_type> handle;
            std::optional<std::tuple<Args...>>* result;

            void operator()(Args... args) {
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

            Op op;
            std::optional<result_type> result;

            bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<promise_type> handle) {
                std::move(op)(typename operation_traits<signature>::handler{handle, &result});
            }

            auto await_resume() {
                return stream_detail::interpret_result(std::move(result).value());
            }
        };

        auto get_return_object() -> stream { return stream{handle_type::from_promise(*this)}; }

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
            return operation_awaiter<std::decay_t<Op>>{std::forward<Op>(op)};
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
        stream* self;

        template <typename Handler>
        void operator()(Handler&& handler) {
            auto& promise = self->m_handle.promise();
            promise.pending = std::forward<Handler>(handler);
            self->m_handle.resume();
        }
    };

    explicit stream(handle_type handle)
        : m_handle(handle) {}

    handle_type m_handle{};
};

}  // namespace fiasco
