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
    asio::io_context m_ioc;
    application_handler m_handler;
    tcp::acceptor m_acceptor;
    asio::executor_work_guard<asio::io_context::executor_type> m_work_guard;
    unsigned int m_threads;
    asio::signal_set m_signals;

    impl(uint16_t port, const std::string& host, application_handler handler,
         unsigned int num_threads)
        : m_handler(std::move(handler)),
          m_acceptor(m_ioc, tcp::endpoint(asio::ip::make_address(host), port)),
          m_work_guard(asio::make_work_guard(m_ioc)),
          m_threads(num_threads == 0 ? std::max(std::thread::hardware_concurrency(), 2u)
                                     : num_threads),
          m_signals(m_ioc, SIGINT, SIGTERM) {
        m_acceptor.set_option(tcp::no_delay(true));

        m_signals.async_wait([this](asio::error_code ec, int) {
            if (!ec) {
                m_work_guard.reset();
                m_ioc.stop();
            }
        });
    }
};

tcp_server::tcp_server(uint16_t port,
                       const std::string& host,
                       application_handler handler,
                       unsigned int num_threads)
    : p_impl(std::make_unique<impl>(port, host, std::move(handler), num_threads)) {}

uint16_t tcp_server::port() const {
    return p_impl->m_acceptor.local_endpoint().port();
}

void tcp_server::run() {
    accept();
    std::vector<std::thread> threads;
    threads.reserve(p_impl->m_threads);
    for (unsigned int i = 0; i < p_impl->m_threads; ++i) {
        threads.emplace_back([this] { p_impl->m_ioc.run(); });
    }
    for (auto& t : threads) {
        t.join();
    }
}

void tcp_server::stop() noexcept {
    // io_context::stop() is the documented, thread-safe way to interrupt run()
    // from another thread. We intentionally do NOT touch m_work_guard here:
    // resetting it from a foreign thread races with the io threads on Asio's
    // internal state (and Asio's fence-based synchronization is invisible to
    // TSan). stop() alone is sufficient to make run() return.
    p_impl->m_ioc.stop();
}

void tcp_server::accept() {
    p_impl->m_acceptor.async_accept(p_impl->m_ioc, [this](asio::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<session>(
                std::move(socket), p_impl->m_ioc.get_executor(), p_impl->m_handler)
                ->start();
        }
        if (p_impl->m_acceptor.is_open()) {
            accept();
        }
    });
}

tcp_server::~tcp_server() = default;

}  // namespace fiasco::detail
