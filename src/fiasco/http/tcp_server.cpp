#include "fiasco/http/detail/tcp_server.hpp"

#include <asio.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
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

namespace fiasco::detail {

using asio::ip::tcp;

namespace {

std::string ascii_lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool request_wants_close(const request& req) {
    return ascii_lower(req.header("Connection")) == "close";
}

struct exchange_state {
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    response_sink sink;
    body_source body;
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
    application_handler& handler;

    session(tcp::socket sock, asio::io_context::executor_type ex, application_handler& h)
        : strand(asio::make_strand(ex)),
          socket(std::move(sock)),
          handler(h) {}

    void start() { read(); }

    void read() {
        auto self = shared_from_this();
        socket.async_read_some(
            asio::buffer(buf),
            asio::bind_executor(strand, [self](asio::error_code ec, std::size_t n) {
                if (ec) {
                    return;
                }
                const bool fed = self->parser.feed(self->buf.data(), n);
                if (self->parser.is_complete()) {
                    self->respond();
                } else if (fed) {
                    self->read();
                }
            }));
    }

    void respond() {
        auto req = std::make_shared<request>(parser.take_request());

        try {
            auto ep = socket.remote_endpoint();
            req->client_address = ep.address().to_string() + ':' + std::to_string(ep.port());
        } catch (...) {
            req->client_address = "unknown";
        }

        auto state = std::make_shared<exchange_state>();
        state->request_close = request_wants_close(*req);

        auto self = shared_from_this();
        state->sink = response_sink{
            .status = [state](int status) { state->status = status; },
            .header =
                [state](std::string_view key, std::string_view value) {
                    state->headers.emplace_back(std::string(key), std::string(value));
                },
            .write = [self, state](std::span<const std::byte> bytes) -> task<void> {
                co_await self->write_chunk(state, bytes);
            },
            .end = [self, state]() -> task<void> { co_await self->end_response(state); },
        };

        std::string_view body_view = req->body;
        state->body = body_source{
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

        asio::co_spawn(
            strand,
            [self, req, state]() -> task<void> {
                exchange ex{*req, state->body, state->sink};
                std::exception_ptr error;
                try {
                    co_await self->handler(ex);
                } catch (...) {
                    error = std::current_exception();
                }
                if (error) {
                    co_await self->write_error_response(state);
                }
                co_return;
            },
            asio::detached);
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

    task<void> send_headers(const std::shared_ptr<exchange_state>& state) {
        std::string head;
        head += "HTTP/1.1 ";
        head += std::to_string(state->status);
        head += ' ';
        head += response::reason_phrase(state->status);
        head += "\r\n";

        bool has_length = false;
        bool has_connection = false;
        bool has_transfer_encoding = false;
        for (const auto& [key, value] : state->headers) {
            head += key;
            head += ": ";
            head += value;
            head += "\r\n";

            const auto lower = ascii_lower(key);
            if (lower == "content-length") {
                has_length = true;
            } else if (lower == "connection") {
                has_connection = true;
                state->close = state->close || ascii_lower(value) == "close";
            } else if (lower == "transfer-encoding") {
                has_transfer_encoding = true;
                state->chunked = ascii_lower(value) == "chunked";
            }
        }

        if (!has_length) {
            if (!has_transfer_encoding) {
                head += "Transfer-Encoding: chunked\r\n";
                state->chunked = true;
            }
        }

        state->close = state->close || state->request_close;
        if (!has_connection) {
            head += state->close ? "Connection: close\r\n" : "Connection: keep-alive\r\n";
        }
        head += "\r\n";

        co_await write_raw(head);
        state->headers_sent = true;
    }

    task<void> write_chunk(const std::shared_ptr<exchange_state>& state,
                           std::span<const std::byte> bytes) {
        if (bytes.empty()) {
            co_return;
        }
        if (!state->headers_sent) {
            co_await send_headers(state);
        }

        if (state->chunked) {
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
    }

    task<void> end_response(const std::shared_ptr<exchange_state>& state) {
        if (!state->headers_sent) {
            co_await send_headers(state);
        }
        if (state->chunked) {
            co_await write_raw("0\r\n\r\n");
        }
        finish_connection(state->close);
    }

    task<void> write_error_response(const std::shared_ptr<exchange_state>& state) {
        if (state->headers_sent) {
            close_socket();
            co_return;
        }

        const auto error = response::error("Internal Server Error", 500);
        state->status = error.status_code;
        state->headers.clear();
        for (const auto& [key, value] : error.headers) {
            state->headers.emplace_back(key, value);
        }
        try {
            co_await write_chunk(
                state, std::as_bytes(std::span<const char>(error.body.data(), error.body.size())));
            co_await end_response(state);
        } catch (...) {
            close_socket();
        }
    }

    void finish_connection(bool close) {
        if (close) {
            close_socket();
            return;
        }
        parser.reset();
        read();
    }

    void close_socket() {
        asio::error_code ignored;
        socket.shutdown(tcp::socket::shutdown_both, ignored);
        socket.close(ignored);
    }
};

struct tcp_server::impl {
    application_handler m_handler;
    std::vector<std::unique_ptr<asio::io_context>> m_contexts;
    std::vector<std::unique_ptr<tcp::acceptor>> m_acceptors;
    std::vector<std::thread> m_threads;
    std::unique_ptr<asio::signal_set> m_signals;
    unsigned int m_workers;

    impl(uint16_t port,
         const std::string& host,
         application_handler handler,
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
                       application_handler handler,
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
