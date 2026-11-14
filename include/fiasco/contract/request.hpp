#pragma once

/// @file
/// @brief HTTP request data and header helpers.

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fiasco::detail {

/// HTTP request method.
enum class http_method {
    get,
    post,
    put,
    del,  // "delete" is a C++ keyword
    patch,
    head,
    options,
    unknown
};

inline http_method string_to_method(const std::string& m) {
    if (m == "GET") {
        return http_method::get;
    }
    if (m == "POST") {
        return http_method::post;
    }
    if (m == "PUT") {
        return http_method::put;
    }
    if (m == "DELETE") {
        return http_method::del;
    }
    if (m == "PATCH") {
        return http_method::patch;
    }
    if (m == "HEAD") {
        return http_method::head;
    }
    if (m == "OPTIONS") {
        return http_method::options;
    }
    return http_method::unknown;
}

inline const char* method_to_string(http_method m) {
    switch (m) {
    case http_method::get:
        return "GET";
    case http_method::post:
        return "POST";
    case http_method::put:
        return "PUT";
    case http_method::del:
        return "DELETE";
    case http_method::patch:
        return "PATCH";
    case http_method::head:
        return "HEAD";
    case http_method::options:
        return "OPTIONS";
    default:
        return "UNKNOWN";
    }
}

/// An incoming HTTP request after parsing and parameter binding.
///
/// `path` and `query_string` are views into `url`; they stay valid as long as
/// the request is not moved. Call `reanchor_url_views()` after moving a request
/// before reading them.
struct request {
    /// Request method.
    http_method method = http_method::unknown;
    /// Complete request target, including the query string.
    std::string url;
    /// Path portion of `url`.
    std::string_view path;
    /// Query string portion of `url`, without the leading `?`.
    std::string_view query_string;
    /// HTTP headers with case-insensitive lookup through `header()`.
    std::unordered_map<std::string, std::string> headers;
    /// Request body.
    std::string body;

    /// Named path parameters populated by the router.
    std::unordered_map<std::string_view, std::string_view> path_params;

    /// Path parameter values in URL order.
    std::vector<std::string_view> ordered_path_params;

    /// Decoded query parameters. The first value wins for duplicate keys.
    std::unordered_map<std::string, std::string> query_params;

    /// Remote client address, when the transport can determine it.
    std::string client_address;

    /// Re-anchor path and query views after the request object has been moved.
    /// Callers must do this before reading path/query_string; path_params are
    /// populated later by routing and are not touched here.
    void reanchor_url_views() {
        if (url.empty()) {
            path = {};
            query_string = {};
            return;
        }
        const auto qpos = url.find('?');
        if (qpos == std::string::npos) {
            path = std::string_view(url);
            query_string = {};
        } else {
            path = std::string_view(url.data(), qpos);
            query_string = std::string_view(url.data() + qpos + 1, url.size() - qpos - 1);
        }
    }

    /// Find a header by case-insensitive name. Returns null when absent.
    [[nodiscard]] const std::string* find_header(std::string_view key) const {
        if (auto it = headers.find(std::string(key)); it != headers.end()) {
            return &it->second;
        }
        for (const auto& [name, value] : headers) {
            if (header_name_equal(name, key)) {
                return &value;
            }
        }
        return nullptr;
    }

    /// Return true when a header with a case-insensitive name exists.
    [[nodiscard]] bool has_header(std::string_view key) const {
        return find_header(key) != nullptr;
    }

    /// Return a header value, or an empty string when it is absent.
    [[nodiscard]] std::string header(const std::string& key) const {
        const auto* value = find_header(key);
        return value ? *value : "";
    }

    /// Return the request `Content-Type`, or an empty string when absent.
    [[nodiscard]] std::string content_type() const { return header("Content-Type"); }

  private:
    static bool header_name_equal(std::string_view a, std::string_view b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i) {
            const auto lower = [](char c) {
                return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
            };
            if (lower(a[i]) != lower(b[i])) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace fiasco::detail
