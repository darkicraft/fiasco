#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include <asio.hpp>

namespace fiasco_test {

inline std::string send_request(std::uint16_t port, std::string_view raw) {
    asio::io_context ioc;
    asio::ip::tcp::socket socket(ioc);
    socket.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port));

    asio::write(socket, asio::buffer(raw.data(), raw.size()));

    std::string response;
    std::array<char, 4096> buf;
    asio::error_code ec;

    while (response.find("\r\n\r\n") == std::string::npos) {
        const auto n = socket.read_some(asio::buffer(buf), ec);
        if (ec) {
            break;
        }
        response.append(buf.data(), n);
    }

    const auto header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return response;
    }

    const auto headers = response.substr(0, header_end);
    std::size_t content_length = 0;
    if (const auto cl = headers.find("Content-Length: "); cl != std::string::npos) {
        const auto start = cl + 16;
        const auto end = headers.find("\r\n", start);
        content_length = std::stoul(headers.substr(start, end - start));
    }

    while (response.size() < header_end + 4 + content_length) {
        const auto n = socket.read_some(asio::buffer(buf), ec);
        if (ec) {
            break;
        }
        response.append(buf.data(), n);
    }

    return response;
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
