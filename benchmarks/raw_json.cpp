// Raw Asio/Beast reference for the JSON bind-and-validate benchmark.
//
// This is the baseline: a hand-written HTTP/1.1 server that parses the same
// request body, binds it to a plain struct, and validates it. It is deliberately
// not a framework; it exists only to show the cost of the framework's
// reflection, validation, and routing on top of the transport.
//
//   ./benchmarks/raw_json_bind 8081
//   wrk -t4 -c50 -d10s -s benchmarks/post.lua http://127.0.0.1:8081/validate

#include <asio.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using asio::ip::tcp;
using nlohmann::json;

struct user {
    std::string username;
    int age;
};

std::string response(std::string body) {
    try {
        json input = json::parse(body);
        if (!input.contains("username") || !input.contains("age")) {
            return "HTTP/1.1 422 Unprocessable Entity\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: 2\r\n"
                   "Connection: keep-alive\r\n\r\n"
                   "{}";
        }
        user bound{input["username"].get<std::string>(), input["age"].get<int>()};
        json output{{"username", bound.username}, {"age", bound.age + 1}};
        const auto payload = output.dump();
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(payload.size()) + "\r\n"
               "Connection: keep-alive\r\n\r\n" + payload;
    } catch (const std::exception&) {
        return "HTTP/1.1 422 Unprocessable Entity\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: 2\r\n"
               "Connection: keep-alive\r\n\r\n"
               "{}";
    }
}

class session : public std::enable_shared_from_this<session> {
  public:
    explicit session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() {
        asio::async_read_until(
            socket_, buffer_, "\r\n\r\n",
            [self = shared_from_this()](asio::error_code error, std::size_t) {
                if (error) {
                    return;
                }
                self->on_headers();
            });
    }

  private:
    void on_headers() {
        std::istream stream(&buffer_);
        std::string line;
        std::getline(stream, line);

        std::size_t content_length = 0;
        while (std::getline(stream, line) && line != "\r") {
            if (line.starts_with("Content-Length:")) {
                content_length = std::stoul(line.substr(15));
            }
        }

        std::string body(content_length, '\0');
        if (buffer_.size() >= content_length) {
            stream.read(body.data(), static_cast<std::streamsize>(content_length));
            respond(body);
        } else {
            asio::async_read(
                socket_, asio::buffer(body.data(), content_length),
                [self = shared_from_this(), body = std::move(body)](
                    asio::error_code error, std::size_t) mutable {
                    if (error) {
                        return;
                    }
                    self->respond(body);
                });
        }
    }

    void respond(const std::string& body) {
        auto reply = std::make_shared<std::string>(response(body));
        asio::async_write(
            socket_, asio::buffer(*reply),
            [self = shared_from_this(), reply](asio::error_code error, std::size_t) {
                if (error) {
                    return;
                }
                self->start();
            });
    }

    tcp::socket socket_;
    asio::streambuf buffer_;
};

class server {
  public:
    server(asio::io_context& io, std::uint16_t port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        accept();
    }

  private:
    void accept() {
        acceptor_.async_accept(
            [this](asio::error_code error, tcp::socket socket) {
                if (!error) {
                    std::make_shared<session>(std::move(socket))->start();
                }
                accept();
            });
    }

    tcp::acceptor acceptor_;
};

}  // namespace

int main(int argc, char** argv) {
    const auto port = argc > 1 ? static_cast<std::uint16_t>(std::stoi(argv[1]))
                               : static_cast<std::uint16_t>(8081);

    asio::io_context io;
    server srv(io, port);

    unsigned int threads = 4;
    if (const char* value = std::getenv("FIASCO_WORKERS")) {
        threads = std::max(1u, static_cast<unsigned int>(std::stoul(value)));
    }
    std::vector<std::thread> workers;
    workers.reserve(threads);
    for (unsigned i = 0; i < threads; ++i) {
        workers.emplace_back([&io] { io.run(); });
    }
    for (auto& worker : workers) {
        worker.join();
    }
}
