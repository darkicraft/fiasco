#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fiasco::detail {

/// Path segment converters. `{id}` captures one segment as a string;
/// `{rest:path}` captures the rest of the path, including slashes. FastAPI
/// documents only the `:path` converter; numeric types come from the handler
/// parameter, so Fiasco does not expose `:int`/`:float`.
enum class path_converter { string, path, unknown };

/// Splits a path into segments, preserving emptiness information so that
/// trailing/leading/double slashes remain meaningful.
///
/// "/users/42"   -> ["users", "42"]
/// "/users/42/"  -> ["users", "42", ""]   (trailing slash => trailing empty segment)
/// "/"           -> [""]
/// "//a"         -> ["", "a"]
inline std::vector<std::string_view> split_path(std::string_view path) {
    std::vector<std::string_view> segs;

    // Strip exactly one leading '/' if present
    if (!path.empty() && path.front() == '/') {
        path = path.substr(1);
    }

    if (path.empty()) {
        // "/" (after stripping the leading slash) -> single empty segment.
        segs.emplace_back("");
        return segs;
    }

    size_t start = 0;
    while (true) {
        size_t pos = path.find('/', start);
        if (pos == std::string_view::npos) {
            segs.push_back(path.substr(start));
            break;
        }
        segs.push_back(path.substr(start, pos - start));
        start = pos + 1;
    }

    return segs;
}

/// True if a segment is a "{name}" placeholder.
inline bool is_param_segment(std::string_view seg) {
    return seg.size() >= 3 && seg.front() == '{' && seg.back() == '}';
}

/// Extracts "name" from "{name}" or "{name:converter}". Caller must ensure
/// is_param_segment(seg).
inline std::string_view param_name(std::string_view seg) {
    auto inner = seg.substr(1, seg.size() - 2);
    const auto colon = inner.find(':');
    return colon == std::string_view::npos ? inner : inner.substr(0, colon);
}

/// Converter declared by a param segment, or `string` when there is none.
inline path_converter param_converter(std::string_view seg) {
    auto inner = seg.substr(1, seg.size() - 2);
    const auto colon = inner.find(':');
    if (colon == std::string_view::npos) {
        return path_converter::string;
    }
    if (inner.substr(colon + 1) == "path") {
        return path_converter::path;
    }
    return path_converter::unknown;
}

/// Route patterns use `{rest:path}`; OpenAPI paths use the parameter name
/// only (`{rest}`).
inline std::string openapi_path(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    for (std::size_t i = 0; i < path.size();) {
        if (path[i] == '{') {
            const auto end = path.find('}', i + 1);
            if (end != std::string_view::npos) {
                const auto segment = path.substr(i, end - i + 1);
                if (is_param_segment(segment)) {
                    out.push_back('{');
                    out.append(param_name(segment));
                    out.push_back('}');
                    i = end + 1;
                    continue;
                }
            }
        }
        out.push_back(path[i]);
        ++i;
    }
    return out;
}

/// Joins a prefix and a path with exactly one '/' between them
/// join_prefix("/api/", "/users")  -> "/api/users"
/// join_prefix("/api", "/users")   -> "/api/users"
/// join_prefix("", "/users")       -> "/users"
/// join_prefix("/api", "/users/") -> "/api/users/"
/// join_prefix("/api/", "/users/") -> "/api/users/"
inline std::string join_prefix(std::string_view prefix, std::string_view path) {
    if (prefix.empty()) {
        return std::string(path);
    }

    bool prefix_ends_slash = prefix.back() == '/';
    bool path_starts_slash = !path.empty() && path.front() == '/';

    std::string result;
    result.reserve(prefix.size() + path.size() + 1);
    result += prefix;

    if (prefix_ends_slash && path_starts_slash) {
        result.append(path.substr(1));
    } else if (!prefix_ends_slash && !path_starts_slash && !path.empty()) {
        result += '/';
        result += path;
    } else {
        result += path;
    }

    return result;
}

/// Percent-decode a URL component ('+' -> space).
inline std::string url_decode(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '+') {
            out.push_back(' ');
        } else if (c == '%' && i + 2 < s.size()) {
            const auto hex = [](char h) -> int {
                if (h >= '0' && h <= '9') {
                    return h - '0';
                }
                if (h >= 'a' && h <= 'f') {
                    return h - 'a' + 10;
                }
                if (h >= 'A' && h <= 'F') {
                    return h - 'A' + 10;
                }
                return -1;
            };
            const int hi = hex(s[i + 1]);
            const int lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>(hi * 16 + lo));
                i += 2;
                continue;
            }
            out.push_back(c);
        } else {
            out.push_back(c);
        }
    }
    return out;
}

/// Parse a query string into parameters (first occurrence wins), matching how
/// Starlette/FastAPI expose query params.
inline std::unordered_map<std::string, std::string> parse_query(std::string_view query) {
    std::unordered_map<std::string, std::string> params;
    std::size_t start = 0;
    while (start <= query.size()) {
        const std::size_t amp = query.find('&', start);
        const std::string_view pair = (amp == std::string_view::npos)
                                          ? query.substr(start)
                                          : query.substr(start, amp - start);
        if (!pair.empty()) {
            const std::size_t eq = pair.find('=');
            if (eq == std::string_view::npos) {
                params.try_emplace(url_decode(pair), std::string());
            } else {
                params.try_emplace(url_decode(pair.substr(0, eq)), url_decode(pair.substr(eq + 1)));
            }
        }
        if (amp == std::string_view::npos) {
            break;
        }
        start = amp + 1;
    }
    return params;
}

}  // namespace fiasco::detail
