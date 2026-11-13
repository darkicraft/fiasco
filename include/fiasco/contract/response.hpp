#pragma once

#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace fiasco::detail {

struct cookie_options {
    std::optional<std::int64_t> max_age;
    std::optional<std::chrono::system_clock::time_point> expires;
    std::string path = "/";
    std::optional<std::string> domain;
    bool secure = false;
    bool httponly = false;
    std::string samesite = "lax";
};

struct file_options {
    std::optional<std::string> filename;
    std::optional<std::string> media_type;
    std::string content_disposition_type = "attachment";
};

namespace response_detail {

inline std::string media_type_for_path(std::string_view path) {
    const auto dot = path.rfind('.');
    std::string extension =
        (dot == std::string_view::npos) ? std::string{} : std::string(path.substr(dot));
    for (char& c : extension) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (extension == ".html" || extension == ".htm") {
        return "text/html; charset=utf-8";
    }
    if (extension == ".css") {
        return "text/css; charset=utf-8";
    }
    if (extension == ".js" || extension == ".mjs") {
        return "application/javascript";
    }
    if (extension == ".json") {
        return "application/json";
    }
    if (extension == ".txt" || extension == ".md") {
        return "text/plain; charset=utf-8";
    }
    if (extension == ".svg") {
        return "image/svg+xml";
    }
    if (extension == ".png") {
        return "image/png";
    }
    if (extension == ".jpg" || extension == ".jpeg") {
        return "image/jpeg";
    }
    if (extension == ".gif") {
        return "image/gif";
    }
    if (extension == ".ico") {
        return "image/x-icon";
    }
    if (extension == ".wasm") {
        return "application/wasm";
    }
    if (extension == ".woff") {
        return "font/woff";
    }
    if (extension == ".woff2") {
        return "font/woff2";
    }
    return "application/octet-stream";
}

inline std::string content_disposition(const file_options& options) {
    if (!options.filename.has_value()) {
        return {};
    }

    std::string escaped;
    escaped.reserve(options.filename->size());
    for (char c : *options.filename) {
        if (c == '"' || c == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }

    return options.content_disposition_type + "; filename=\"" + escaped + "\"";
}

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

inline std::string http_date(std::chrono::system_clock::time_point point) {
    using namespace std::chrono;

    const auto whole_seconds = floor<std::chrono::seconds>(point);
    const auto whole_days = floor<std::chrono::days>(whole_seconds);
    const year_month_day date{whole_days};
    const weekday day_of_week{whole_days};
    const hh_mm_ss time{whole_seconds - whole_days};

    static constexpr const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static constexpr const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    return std::format("{}, {:02} {} {:04} {:02}:{:02}:{:02} GMT",
                       weekdays[day_of_week.c_encoding()],
                       static_cast<unsigned>(date.day()),
                       months[static_cast<unsigned>(date.month()) - 1],
                       static_cast<int>(date.year()),
                       static_cast<int>(time.hours().count()),
                       static_cast<int>(time.minutes().count()),
                       static_cast<int>(time.seconds().count()));
}

inline bool cookie_name_char(char c) {
    const auto uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc) != 0) {
        return true;
    }
    constexpr std::string_view extra = "!#$%&'*+-.^_`|~";
    return extra.find(c) != std::string_view::npos;
}

inline void validate_cookie_text(std::string_view value, const char* what) {
    for (char c : value) {
        const auto uc = static_cast<unsigned char>(c);
        if (c == '\r' || c == '\n' || uc < 0x20 || uc == 0x7f) {
            throw std::invalid_argument(std::string("invalid ") + what +
                                        ": control characters are not allowed");
        }
    }
}

inline void append_cookie_value(std::string& out, std::string_view value) {
    validate_cookie_text(value, "cookie value");

    bool quote = value.empty();
    for (char c : value) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc <= 0x20 || uc >= 0x7f || c == '"' || c == '\\' || c == ',' || c == ';') {
            quote = true;
            break;
        }
    }

    if (!quote) {
        out.append(value);
        return;
    }

    out.push_back('"');
    for (char c : value) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
}

inline std::string make_cookie_header(std::string_view name,
                                      std::string_view value,
                                      const cookie_options& options) {
    validate_cookie_text(name, "cookie name");
    if (name.empty()) {
        throw std::invalid_argument("cookie name must not be empty");
    }
    for (char c : name) {
        if (!cookie_name_char(c)) {
            throw std::invalid_argument("invalid cookie name");
        }
    }

    std::string header;
    header.reserve(name.size() + value.size() + 96);
    header.append(name);
    header.push_back('=');
    append_cookie_value(header, value);

    if (!options.path.empty()) {
        validate_cookie_text(options.path, "cookie path");
        header += "; Path=";
        header += options.path;
    }
    if (options.domain.has_value() && !options.domain->empty()) {
        validate_cookie_text(*options.domain, "cookie domain");
        header += "; Domain=";
        header += *options.domain;
    }
    if (options.max_age.has_value()) {
        header += "; Max-Age=";
        header += std::to_string(*options.max_age);
    }
    if (options.expires.has_value()) {
        header += "; Expires=";
        header += http_date(*options.expires);
    }
    if (options.secure) {
        header += "; Secure";
    }
    if (options.httponly) {
        header += "; HttpOnly";
    }
    if (!options.samesite.empty()) {
        std::string lowered = options.samesite;
        for (char& c : lowered) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (lowered != "lax" && lowered != "strict" && lowered != "none") {
            throw std::invalid_argument("invalid SameSite value");
        }
        header += "; SameSite=";
        header += options.samesite;
    }

    return header;
}

}  // namespace response_detail

struct response {
    int status_code = 200;
    std::unordered_map<std::string, std::string> headers;
    std::vector<std::string> set_cookie_headers;
    std::string body;

    response& set_cookie(std::string_view name,
                         std::string_view value,
                         const cookie_options& options = {}) {
        set_cookie_headers.push_back(response_detail::make_cookie_header(name, value, options));
        return *this;
    }

    response& delete_cookie(std::string_view name, cookie_options options = {}) {
        options.max_age = 0;
        options.expires = std::chrono::system_clock::now();
        return set_cookie(name, "", options);
    }

    static response empty(int status = 204) {
        response r;
        r.status_code = status;
        return r;
    }

    static response file(const std::filesystem::path& path, const file_options& options = {}) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            throw std::runtime_error("file does not exist: " + path.string());
        }

        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("cannot open file: " + path.string());
        }

        response r;
        r.headers["Content-Type"] = options.media_type.has_value()
                                        ? *options.media_type
                                        : response_detail::media_type_for_path(path.string());
        const auto disposition = response_detail::content_disposition(options);
        if (!disposition.empty()) {
            r.headers["Content-Disposition"] = disposition;
        }
        r.body.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
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
        for (const auto& cookie : set_cookie_headers) {
            out += "Set-Cookie: ";
            out += cookie;
            out += "\r\n";
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
