#include "fiasco/http/tcp_server.hpp"

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
#include <vector>

#ifdef _WIN32
#else
#include <sys/socket.h>
#endif

#include "fiasco/internal/http/parser.hpp"

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

bool response_wants_close(const response& res) {
    const auto it = res.headers.find("Connection");
    return it != res.headers.end() && ascii_lower(it->second) == "close";
}

struct exchange_state {
    response out;
    response_sink sink;
    body_source body;
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

        const bool request_close = request_wants_close(*req);

        auto state = std::make_shared<exchange_state>();
        response* out = &state->out;
        state->sink = response_sink{
            .status = [out](int status) { out->status_code = status; },
            .header =
                [out](std::string_view key, std::string_view value) {
                    out->headers[std::string(key)] = std::string(value);
                },
            .write =
                [out](std::span<const std::byte> bytes) -> task<void> {
                    out->body.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                    co_return;
                },
            .end = []() -> task<void> { co_return; },
        };

        std::string_view body_view = req->body;
        state->body = body_source{
            .read =
                [body_view, pos = std::size_t{0}]() mutable
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

        auto self = shared_from_this();
        asio::co_spawn(
            strand,
            [self, req, state, request_close]() -> task<void> {
                exchange ex{*req, state->body, state->sink};
                try {
                    co_await self->handler(ex);
                } catch (...) {
                    state->out = response::error("Internal Server Error", 500);
                }
                self->finish(std::move(state->out), request_close);
                co_return;
            },
            asio::detached);
    }

    void finish(response res, bool request_close) {
        const bool close = request_close || response_wants_close(res);
        if (close) {
            res.headers["Connection"] = "close";
        }

        auto self = shared_from_this();
        auto raw = std::make_shared<std::string>(res.serialize());
        asio::async_write(
            socket,
            asio::buffer(*raw),
            asio::bind_executor(strand, [self, raw, close](asio::error_code ec, std::size_t) {
                if (!ec && !close) {
                    self->parser.reset();  // reset parser state for next request
                    self->read();          // keep reading on same connection
                }
            }));
    }
};

struct tcp_server::impl {
    application_handler m_handler;
    std::vector<std::unique_ptr<asio::io_context>> m_contexts;
    std::vector<std::unique_ptr<tcp::acceptor>> m_acceptors;
    std::vector<std::thread> m_threads;
    std::unique_ptr<asio::signal_set> m_signals;
    unsigned int m_workers;

    impl(uint16_t port, const std::string& host, application_handler handler,
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
        acceptor.async_accept(
            context, [this, index](asio::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<session>(std::move(socket),
                                              m_contexts[index]->get_executor(), m_handler)
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
