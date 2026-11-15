#include "fiasco/http/detail/tcp_server.hpp"

#include <asio.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#else
#include <sys/socket.h>
#endif

#include "fiasco/http/detail/parser.hpp"
#include "fiasco/http/detail/request.hpp"
#include "fiasco/http/detail/response.hpp"

namespace fiasco::detail {

using asio::ip::tcp;

namespace {

bool ascii_iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

struct exchange_state {
    int status = 200;
    http_method method = http_method::unknown;
    std::vector<std::pair<std::string, std::string>> headers;
    response_sink sink;
    bool request_close = false;
    bool headers_sent = false;
    bool chunked = false;
    bool close = false;
};

std::unique_ptr<tcp::acceptor> make_acceptor(asio::io_context& context,
                                             const std::string& host,
                                             std::uint16_t port,
                                             bool reuse_port) {
    auto acceptor = std::make_unique<tcp::acceptor>(context);
    acceptor->open(tcp::v4());
    acceptor->set_option(asio::socket_base::reuse_address(true));
#if !defined(_WIN32) && defined(SO_REUSEPORT)
    if (reuse_port) {
        const int one = 1;
        ::setsockopt(acceptor->native_handle(), SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
    }
#else
    (void)reuse_port;
#endif
    acceptor->bind(tcp::endpoint(asio::ip::make_address(host), port));
    acceptor->listen();
    return acceptor;
}

}  // namespace

struct session : std::enable_shared_from_this<session> {
    asio::strand<asio::io_context::executor_type> strand;
    tcp::socket socket;
    llhttp_parser parser;
    std::array<char, 8192> buf;
    application& handler;
    std::string peer_address = "unknown";
    body_source* active_stream = nullptr;
    asio::cancellation_signal peer_watch_cancel;
    std::uint64_t peer_watch_generation = 0;
    bool watching_peer = false;
    bool peer_closed = false;

    session(tcp::socket sock, asio::io_context::executor_type ex, application& h)
        : strand(asio::make_strand(ex)),
          socket(std::move(sock)),
          handler(h) {
        // Responses are small and written with keep-alive, so Nagle only adds
        // latency (and delayed ACKs can stall a response by tens of ms).
        asio::error_code ignored;
        socket.set_option(tcp::no_delay(true), ignored);
        // The peer address is stable for the connection; resolve it once rather
        // than issuing a getpeername syscall for every request.
        const auto endpoint = socket.remote_endpoint(ignored);
        if (!ignored) {
            peer_address = endpoint.address().to_string() + ':' + std::to_string(endpoint.port());
        }
    }

    void start() {
        auto self = shared_from_this();
        asio::co_spawn(
            strand,
            [self]() -> task<void> { co_await self->run(); },
            asio::detached);
    }

    task<void> run() {
        try {
            for (;;) {
                if (!parser.is_complete()) {
                    const auto n = co_await socket.async_read_some(asio::buffer(buf),
                                                                   asio::use_awaitable);
                    if (n == 0) {
                        break;
                    }
                    if (!parser.feed(buf.data(), n)) {
                        break;
                    }
                    if (!parser.is_complete()) {
                        continue;
                    }
                }

                request& req = parser.current();
                req.client_address = peer_address;

                exchange_state state;
                state.method = req.method;
                state.request_close = !req.keep_alive;
                stop_peer_watch();
                peer_closed = false;
                active_stream = nullptr;

                request_result result;
                response res;
                bool failed = false;
                try {
                    result = handler.handle(req);
                } catch (...) {
                    failed = true;
                }

                if (!failed) {
                    if (auto* ready = std::get_if<response>(&result)) {
                        res = std::move(*ready);
                    } else {
                        try {
                            auto async_res = co_await std::get<async>(std::move(result));
                            res = std::move(async_res).take();
                        } catch (...) {
                            failed = true;
                        }
                    }
                }

                if (!failed) {
                    try {
                        handler.prepare(req, res);
                        if (res.stream_source) {
                            state.sink = make_sink(state);
                            co_await handler.write(state.sink, res);
                        } else {
                            co_await send_response(state, res);
                        }
                    } catch (...) {
                        failed = true;
                    }
                }

                if (failed) {
                    if (state.headers_sent || peer_closed) {
                        close_socket();
                        break;
                    }
                    try {
                        co_await write_error_response(state);
                    } catch (...) {
                        close_socket();
                        break;
                    }
                }

                if (state.close || state.request_close || peer_closed) {
                    break;
                }
                parser.reset();
            }
        } catch (...) {
        }
        close_socket();
    }

    response_sink make_sink(exchange_state& state) {
        return response_sink{
            .status = [&state](int status) { state.status = status; },
            .header =
                [&state](std::string_view key, std::string_view value) {
                    state.headers.emplace_back(std::string(key), std::string(value));
                },
            .write =
                [this, &state](std::span<const std::byte> bytes) -> task<void> {
                    co_await write_chunk(state, bytes);
                },
            .end = [this, &state]() -> task<void> { co_await end_response(state); },
            .begin_stream =
                [this](body_source* source) {
                    active_stream = source;
                    start_peer_watch();
                },
            .end_stream =
                [this]() {
                    active_stream = nullptr;
                    stop_peer_watch();
                },
        };
    }

    void start_peer_watch() {
        if (watching_peer || peer_closed || !socket.is_open()) {
            return;
        }

        watching_peer = true;
        auto self = shared_from_this();
        const auto generation = ++peer_watch_generation;
        socket.async_wait(
            tcp::socket::wait_read,
            asio::bind_cancellation_slot(
                peer_watch_cancel.slot(),
                asio::bind_executor(strand, [self, generation](asio::error_code error) {
                    self->on_peer_readable(generation, error);
                })));
    }

    void stop_peer_watch() {
        if (!watching_peer) {
            return;
        }
        watching_peer = false;
        ++peer_watch_generation;
        peer_watch_cancel.emit(asio::cancellation_type::all);
    }

    void on_peer_readable(std::uint64_t generation, asio::error_code error) {
        if (generation != peer_watch_generation || !watching_peer) {
            return;
        }
        watching_peer = false;

        if (error == asio::error::operation_aborted) {
            return;
        }
        if (error) {
            peer_disconnected();
            return;
        }

        std::array<char, 1> byte{};
        const auto count = socket.receive(asio::buffer(byte), tcp::socket::message_peek, error);
        if (error == asio::error::would_block || error == asio::error::try_again) {
            return;
        }
        if (error || count == 0) {
            peer_disconnected();
        }
        // A readable byte that is not EOF is likely a pipelined request. Do not
        // consume it; stop watching instead of entering a readiness loop.
    }

    void peer_disconnected() {
        peer_closed = true;
        watching_peer = false;
        ++peer_watch_generation;
        if (active_stream && active_stream->cancel) {
            active_stream->cancel();
        }
        close_socket();
    }

    static auto chunk_size(std::size_t size) -> std::string {
        static constexpr char digits[] = "0123456789abcdef";
        std::string out;
        do {
            out.push_back(digits[size & 0x0f]);
            size >>= 4;
        } while (size != 0);
        std::reverse(out.begin(), out.end());
        return out;
    }

    task<void> write_raw(std::string_view bytes) {
        co_await asio::async_write(
            socket, asio::buffer(bytes.data(), bytes.size()), asio::use_awaitable);
    }

    task<void> send_response(exchange_state& state, response& res) {
        stop_peer_watch();
        if (peer_closed) {
            close_socket();
            co_return;
        }

        const bool status_bodyless = res.status_code == 204 || res.status_code == 304 ||
                                     (res.status_code >= 100 && res.status_code < 200);
        const bool no_body = status_bodyless || state.method == http_method::head;

        std::size_t capacity = 96;
        for (const auto& [key, value] : res.headers) {
            capacity += key.size() + value.size() + 4;
        }
        for (const auto& cookie : res.set_cookie_headers) {
            capacity += cookie.size() + 14;
        }

        bool has_length = false;
        bool has_connection = false;
        bool has_transfer_encoding = false;

        std::string head;
        head.reserve(capacity);
        head += "HTTP/1.1 ";
        head += std::to_string(res.status_code);
        head += ' ';
        head += response::reason_phrase(res.status_code);
        head += "\r\n";

        for (const auto& [key, value] : res.headers) {
            head += key;
            head += ": ";
            head += value;
            head += "\r\n";

            if (ascii_iequals(key, "Content-Length")) {
                has_length = true;
            } else if (ascii_iequals(key, "Connection")) {
                has_connection = true;
                state.close = state.close || ascii_iequals(value, "close");
            } else if (ascii_iequals(key, "Transfer-Encoding")) {
                has_transfer_encoding = true;
            }
        }
        for (const auto& cookie : res.set_cookie_headers) {
            head += "Set-Cookie: ";
            head += cookie;
            head += "\r\n";
        }

        if (!has_length && !has_transfer_encoding && !status_bodyless) {
            head += "Content-Length: ";
            head += std::to_string(res.body.size());
            head += "\r\n";
        }

        state.close = state.close || state.request_close;
        if (!has_connection) {
            head += state.close ? "Connection: close\r\n" : "Connection: keep-alive\r\n";
        }
        head += "\r\n";

        state.headers_sent = true;
        std::array<asio::const_buffer, 2> buffers{
            asio::buffer(head.data(), head.size()),
            no_body ? asio::const_buffer{} : asio::buffer(res.body)};
        co_await asio::async_write(socket, buffers, asio::use_awaitable);
    }

    static std::string build_headers(exchange_state& state) {
        std::string head;
        head += "HTTP/1.1 ";
        head += std::to_string(state.status);
        head += ' ';
        head += response::reason_phrase(state.status);
        head += "\r\n";

        bool has_length = false;
        bool has_connection = false;
        bool has_transfer_encoding = false;
        for (const auto& [key, value] : state.headers) {
            head += key;
            head += ": ";
            head += value;
            head += "\r\n";

            if (ascii_iequals(key, "Content-Length")) {
                has_length = true;
            } else if (ascii_iequals(key, "Connection")) {
                has_connection = true;
                state.close = state.close || ascii_iequals(value, "close");
            } else if (ascii_iequals(key, "Transfer-Encoding")) {
                has_transfer_encoding = true;
                state.chunked = ascii_iequals(value, "chunked");
            }
        }

        if (!has_length) {
            if (!has_transfer_encoding) {
                head += "Transfer-Encoding: chunked\r\n";
                state.chunked = true;
            }
        }

        state.close = state.close || state.request_close;
        if (!has_connection) {
            head += state.close ? "Connection: close\r\n" : "Connection: keep-alive\r\n";
        }
        head += "\r\n";
        return head;
    }

    task<void> write_chunk(exchange_state& state, std::span<const std::byte> bytes) {
        if (bytes.empty()) {
            co_return;
        }

        if (state.headers_sent) {
            if (state.chunked) {
                std::string frame;
                frame.reserve(bytes.size() + 24);
                frame += chunk_size(bytes.size());
                frame += "\r\n";
                frame.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                frame += "\r\n";
                co_await write_raw(frame);
            } else {
                co_await asio::async_write(
                    socket, asio::buffer(bytes.data(), bytes.size()), asio::use_awaitable);
            }
            co_return;
        }

        // Coalesce the response head with the first body chunk into a single
        // write. Two small writes per response interact badly with delayed ACKs
        // and cost an extra syscall on the hot path.
        std::string out = build_headers(state);
        if (state.chunked) {
            out += chunk_size(bytes.size());
            out += "\r\n";
            out.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            out += "\r\n";
        } else {
            out.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
        state.headers_sent = true;
        co_await write_raw(out);
    }

    task<void> end_response(exchange_state& state) {
        stop_peer_watch();
        if (peer_closed) {
            close_socket();
            co_return;
        }
        if (!state.headers_sent) {
            std::string out = build_headers(state);
            if (state.chunked) {
                out += "0\r\n\r\n";
            }
            state.headers_sent = true;
            co_await write_raw(out);
        } else if (state.chunked) {
            co_await write_raw("0\r\n\r\n");
        }
    }

    task<void> write_error_response(exchange_state& state) {
        stop_peer_watch();
        if (state.headers_sent || peer_closed) {
            close_socket();
            co_return;
        }

        const auto error = response::error("Internal Server Error", 500);
        state.status = error.status_code;
        state.headers.clear();
        for (const auto& [key, value] : error.headers) {
            state.headers.emplace_back(key, value);
        }
        try {
            co_await write_chunk(
                state, std::as_bytes(std::span<const char>(error.body.data(), error.body.size())));
            co_await end_response(state);
        } catch (...) {
            close_socket();
        }
    }

    void close_socket() {
        asio::error_code ignored;
        socket.shutdown(tcp::socket::shutdown_both, ignored);
        socket.close(ignored);
    }
};

struct tcp_server::impl {
    application m_handler;
    std::vector<std::unique_ptr<asio::io_context>> m_contexts;
    std::vector<std::unique_ptr<tcp::acceptor>> m_acceptors;
    std::vector<std::thread> m_threads;
    std::unique_ptr<asio::signal_set> m_signals;
    unsigned int m_workers;

    impl(uint16_t port,
         const std::string& host,
         application handler,
         unsigned int num_threads)
        : m_handler(std::move(handler)),
          m_workers(num_threads == 0 ? std::max(std::thread::hardware_concurrency(), 2u)
                                     : num_threads) {
#ifdef _WIN32
        // Windows: one shared io_context; IOCP balances across the worker threads.
        m_contexts.push_back(std::make_unique<asio::io_context>());
        m_acceptors.push_back(make_acceptor(*m_contexts.front(), host, port, false));
#else
        // Linux/BSD: one io_context per core, one acceptor per core, SO_REUSEPORT.
        m_contexts.reserve(m_workers);
        m_acceptors.reserve(m_workers);
        std::uint16_t actual_port = port;
        for (unsigned int i = 0; i < m_workers; ++i) {
            m_contexts.push_back(std::make_unique<asio::io_context>(1));
            m_acceptors.push_back(make_acceptor(*m_contexts.back(), host, actual_port, true));
            if (i == 0) {
                actual_port = m_acceptors.front()->local_endpoint().port();
            }
        }
#endif
        m_signals = std::make_unique<asio::signal_set>(*m_contexts.front(), SIGINT, SIGTERM);
        m_signals->async_wait([this](asio::error_code ec, int) {
            if (!ec) {
                stop();
            }
        });
    }

    void accept_at(std::size_t index) {
        auto& context = *m_contexts[index];
        auto& acceptor = *m_acceptors[index];
        acceptor.async_accept(context, [this, index](asio::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<session>(
                    std::move(socket), m_contexts[index]->get_executor(), m_handler)
                    ->start();
            }
            if (m_acceptors[index]->is_open()) {
                accept_at(index);
            }
        });
    }

    void run() {
        if (m_contexts.size() == 1) {
            accept_at(0);
            for (unsigned int i = 0; i < m_workers; ++i) {
                m_threads.emplace_back([this] { m_contexts.front()->run(); });
            }
        } else {
            for (std::size_t i = 0; i < m_contexts.size(); ++i) {
                accept_at(i);
            }
            for (std::size_t i = 0; i < m_contexts.size(); ++i) {
                m_threads.emplace_back([this, i] { m_contexts[i]->run(); });
            }
        }
        for (auto& thread : m_threads) {
            thread.join();
        }
    }

    void stop() {
        for (auto& context : m_contexts) {
            context->stop();
        }
    }

    [[nodiscard]] std::uint16_t port() const {
        return m_acceptors.empty() ? 0 : m_acceptors.front()->local_endpoint().port();
    }
};

tcp_server::tcp_server(uint16_t port,
                       const std::string& host,
                       application handler,
                       unsigned int num_threads)
    : p_impl(std::make_unique<impl>(port, host, std::move(handler), num_threads)) {}

uint16_t tcp_server::port() const {
    return p_impl->port();
}

void tcp_server::run() {
    p_impl->run();
}

void tcp_server::stop() noexcept {
    p_impl->stop();
}

tcp_server::~tcp_server() = default;

}  // namespace fiasco::detail
