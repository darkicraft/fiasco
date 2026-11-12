#pragma once

// Static file mounts. This is intentionally a small buffered implementation:
// path validation, MIME lookup, index.html support, GET/HEAD, and a clean 404.

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "fiasco/contract/request.hpp"
#include "fiasco/contract/response.hpp"

namespace fiasco::detail {

struct static_options {
    bool html = false;
};

struct static_mount {
    std::string prefix;
    std::filesystem::path directory;
    static_options options;
};

inline std::string normalize_static_prefix(std::string prefix) {
    if (prefix.empty()) {
        return "/";
    }
    if (prefix.front() != '/') {
        prefix.insert(prefix.begin(), '/');
    }
    while (prefix.size() > 1 && prefix.back() == '/') {
        prefix.pop_back();
    }
    return prefix;
}

inline bool static_path_matches(std::string_view prefix, std::string_view path) {
    if (prefix == "/") {
        return true;
    }
    if (path == prefix) {
        return true;
    }
    return path.size() > prefix.size() && path.starts_with(prefix) &&
           path[prefix.size()] == '/';
}

inline int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

inline std::string decode_static_path(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '%' && i + 2 < path.size()) {
            const int hi = hex_value(path[i + 1]);
            const int lo = hex_value(path[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>(hi * 16 + lo));
                i += 2;
                continue;
            }
        }
        out.push_back(path[i]);
    }
    return out;
}

inline std::optional<std::filesystem::path> static_relative_path(std::string_view rest) {
    const std::string decoded = decode_static_path(rest);
    std::filesystem::path relative;

    std::size_t start = 0;
    while (start <= decoded.size()) {
        const std::size_t slash = decoded.find('/', start);
        const std::string_view segment =
            (slash == std::string::npos)
                ? std::string_view(decoded).substr(start)
                : std::string_view(decoded).substr(start, slash - start);

        if (!segment.empty() && segment != ".") {
            if (segment == ".." || segment.find('\\') != std::string_view::npos) {
                return std::nullopt;
            }
            relative /= std::filesystem::path(segment);
        }

        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }

    return relative;
}

inline std::string static_content_type(std::string extension) {
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

inline response static_not_found() {
    return response::text("Not Found", 404);
}

inline response static_method_not_allowed() {
    response res = response::text("Method Not Allowed", 405);
    res.headers["Allow"] = "GET, HEAD";
    return res;
}

inline std::optional<response> serve_static(const static_mount& mount,
                                            const request& req) {
    if (!static_path_matches(mount.prefix, req.path)) {
        return std::nullopt;
    }
    if (req.method != http_method::get && req.method != http_method::head) {
        return static_method_not_allowed();
    }

    std::string_view rest = req.path;
    if (mount.prefix != "/") {
        rest = rest.substr(mount.prefix.size());
    }
    auto relative = static_relative_path(rest);
    if (!relative.has_value()) {
        return static_not_found();
    }

    std::error_code ec;
    const auto root = std::filesystem::weakly_canonical(mount.directory, ec);
    if (ec) {
        return static_not_found();
    }

    auto target = std::filesystem::weakly_canonical(root / *relative, ec);
    if (ec) {
        return static_not_found();
    }

    const auto from_root = std::filesystem::relative(target, root, ec);
    if (ec) {
        return static_not_found();
    }
    for (const auto& part : from_root) {
        if (part == "..") {
            return static_not_found();
        }
    }

    if (std::filesystem::is_directory(target, ec)) {
        if (!mount.options.html) {
            return static_not_found();
        }
        target /= "index.html";
        target = std::filesystem::weakly_canonical(target, ec);
        if (ec) {
            return static_not_found();
        }
    }

    if (!std::filesystem::is_regular_file(target, ec)) {
        return static_not_found();
    }

    const auto size = std::filesystem::file_size(target, ec);
    if (ec) {
        return static_not_found();
    }

    response res;
    res.status_code = 200;
    res.headers["Content-Type"] = static_content_type(target.extension().string());

    if (req.method == http_method::head) {
        res.headers["Content-Length"] = std::to_string(size);
        return res;
    }

    std::ifstream file(target, std::ios::binary);
    if (!file) {
        return static_not_found();
    }
    res.body.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return res;
}

}  // namespace fiasco::detail
