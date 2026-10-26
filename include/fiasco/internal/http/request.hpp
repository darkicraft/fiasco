#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fiasco::detail {

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

http_method string_to_method(const std::string& m);
const char* method_to_string(http_method m);

struct request {
    http_method method = http_method::unknown;
    std::string url;                // owns the URL buffer
    std::string_view path;          // points into url
    std::string_view query_string;  // points into url
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    // Named path params (views point into url for values, route table for keys)
    std::unordered_map<std::string_view, std::string_view> path_params;

    // Positional path param values in URL order (views point into url)
    std::vector<std::string_view> ordered_path_params;

    // Decoded query parameters (first value wins), parsed from query_string.
    std::unordered_map<std::string, std::string> query_params;

    std::string client_address;

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

    [[nodiscard]] bool has_header(std::string_view key) const {
        return find_header(key) != nullptr;
    }

    [[nodiscard]] std::string header(const std::string& key) const {
        const auto* value = find_header(key);
        return value ? *value : "";
    }

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
