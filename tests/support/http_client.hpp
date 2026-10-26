#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <asio.hpp>

namespace fiasco_test {

class http_connection {
  public:
    explicit http_connection(std::uint16_t port)
        : m_socket(m_ioc) {
        m_socket.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port));
    }

    http_connection(const http_connection&) = delete;
    http_connection& operator=(const http_connection&) = delete;

    void send(std::string_view raw) {
        asio::write(m_socket, asio::buffer(raw.data(), raw.size()));
    }

    std::string read_response() {
        while (true) {
            const auto header_end = m_buffer.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                std::size_t content_length = 0;
                const auto headers = m_buffer.substr(0, header_end);
                if (const auto cl = headers.find("Content-Length: "); cl != std::string::npos) {
                    const auto start = cl + 16;
                    const auto end = headers.find("\r\n", start);
                    content_length = std::stoul(headers.substr(start, end - start));
                }

                const auto total = header_end + 4 + content_length;
                if (m_buffer.size() >= total) {
                    auto response = m_buffer.substr(0, total);
                    m_buffer.erase(0, total);
                    return response;
                }
            }

            std::array<char, 4096> buf;
            asio::error_code ec;
            const auto n = m_socket.read_some(asio::buffer(buf), ec);
            if (ec) {
                auto response = std::move(m_buffer);
                m_buffer.clear();
                return response;
            }
            m_buffer.append(buf.data(), n);
        }
    }

    bool wait_for_eof() {
        std::array<char, 256> buf;
        asio::error_code ec;
        while (!ec) {
            m_socket.read_some(asio::buffer(buf), ec);
        }
        return ec == asio::error::eof || ec == asio::error::connection_reset;
    }

  private:
    asio::io_context m_ioc;
    asio::ip::tcp::socket m_socket;
    std::string m_buffer;
};

inline std::string send_request(std::uint16_t port, std::string_view raw) {
    http_connection connection(port);
    connection.send(raw);
    return connection.read_response();
}

inline int extract_status(const std::string& response) {
    const auto first = response.find(' ');
    const auto second = response.find(' ', first + 1);
    return std::stoi(response.substr(first + 1, second - first - 1));
}

inline std::string extract_body(const std::string& response) {
    const auto header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return {};
    }
    return response.substr(header_end + 4);
}

}  // namespace fiasco_test
