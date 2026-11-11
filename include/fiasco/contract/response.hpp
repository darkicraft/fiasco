#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace fiasco::detail {

namespace response_detail {

inline std::string json_escape(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                constexpr char hex[] = "0123456789abcdef";
                out += "\\u00";
                out += hex[(static_cast<unsigned char>(c) >> 4) & 0x0f];
                out += hex[static_cast<unsigned char>(c) & 0x0f];
            } else {
                out += c;
            }
        }
    }
    return out;
}

}  // namespace response_detail

struct response {
    int status_code = 200;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    static response empty(int status = 204) {
        response r;
        r.status_code = status;
        return r;
    }

    static response text(const std::string& body, int status = 200) {
        response r;
        r.status_code = status;
        r.body = body;
        r.body += '\n';
        r.headers["Content-Type"] = "text/plain";
        return r;
    }

    static response json(const std::string& json_body, int status = 200) {
        response r;
        r.status_code = status;
        r.body = json_body;
        r.body += '\n';
        r.headers["Content-Type"] = "application/json";
        return r;
    }

    static response html(const std::string& body, int status = 200) {
        response r;
        r.status_code = status;
        r.body = body;
        r.body += '\n';
        r.headers["Content-Type"] = "text/html";
        return r;
    }

    static response redirect(const std::string& location, int status = 302) {
        response r;
        r.status_code = status;
        r.headers["Location"] = location;
        return r;
    }

    static response error(const std::string& message, int status) {
        response r;
        r.status_code = status;
        r.body = "{\"detail\":\"" + response_detail::json_escape(message) + "\"}\n";
        r.headers["Content-Type"] = "application/json";
        return r;
    }

    [[nodiscard]] static const char* reason_phrase(int code) {
        switch (code) {
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 204:
            return "No Content";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 422:
            return "Unprocessable Entity";
        case 500:
            return "Internal Server Error";
        default:
            return "Unknown";
        }
    }

    [[nodiscard]] std::string serialize() const {
        // pre-size to avoid reallocations: status line ~20, headers ~60 each, body
        std::string out;
        out.reserve(128 + headers.size() * 64 + body.size());

        // status line
        out += "HTTP/1.1 ";
        out += std::to_string(status_code);
        out += ' ';
        out += reason_phrase(status_code);
        out += "\r\n";

        bool has_content_length = false;
        bool has_connection = false;
        for (const auto& [key, val] : headers) {
            out += key;
            out += ": ";
            out += val;
            out += "\r\n";
            if (key == "Content-Length") {
                has_content_length = true;
            }
            if (key == "Connection") {
                has_connection = true;
            }
        }

        if (!has_content_length) {
            out += "Content-Length: ";
            out += std::to_string(body.size());
            out += "\r\n";
        }
        if (!has_connection) {
            out += "Connection: keep-alive\r\n";
        }

        out += "\r\n";
        out += body;
        return out;
    }
};

}  // namespace fiasco::detail
