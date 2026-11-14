#pragma once

/// @file
/// @brief Route storage and request matching.
/// @internal

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "fiasco/http/detail/request.hpp"
#include "fiasco/routing/descriptor.hpp"
#include "fiasco/routing/function_traits.hpp"
#include "fiasco/routing/pathing.hpp"

namespace fiasco::detail {

inline constexpr size_t num_http_methods = 8;

inline size_t method_index(http_method m) {
    return static_cast<size_t>(m);
}

struct string_hash {
    using is_transparent = void;
    size_t operator()(std::string_view s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
    size_t operator()(const std::string& s) const noexcept { return std::hash<std::string>{}(s); }
};

/// Stored handler and descriptor for one route.
struct route_entry {
    handler_fn handler;
    async_handler_fn async_handler;
    route_descriptor descriptor;
};

/// Result of matching a method and path against the route table.
struct match_result {
    bool matched = false;
    bool is_async = false;
    // Non-owning pointers to the matched handler, owned by the route_table.
    // Handlers are move-only, so the table cannot hand out a copy; callers
    // invoke via `(*match.async_handler)(req)` or `(*match.handler)(req)`.
    // match() is non-const because move_only_function::operator() is non-const.
    handler_fn* handler = nullptr;
    async_handler_fn* async_handler = nullptr;
    // Non-owning pointer to the matched route's registration metadata.
    const route_descriptor* descriptor = nullptr;
    std::vector<std::pair<std::string_view, std::string_view>> path_params;
};

/// Owns registered routes and matches requests against them.
class route_table {
  public:
    /// Add a synchronous route.
    void add(http_method method,
             const std::string& pattern,
             handler_fn handler,
             route_descriptor descriptor = {});
    /// Add an asynchronous route.
    void add(http_method method,
             const std::string& pattern,
             async_handler_fn handler,
             route_descriptor descriptor = {});
    /// Match a request method and path.
    match_result match(http_method method, std::string_view path);
    /// True when any method matches the path.
    bool any_method_matches(std::string_view path) const;
    /// Methods that match the path, used for `Allow` headers.
    [[nodiscard]] std::vector<http_method> allowed_methods(std::string_view path) const;
    /// Merge routes from another table under a prefix.
    void merge_with_prefix(route_table&& other,
                           std::string_view prefix,
                           std::span<const std::string> extra_tags = {});

    template <typename F>
    void for_each_route(F&& f) const {
        for (size_t mi = 0; mi < num_http_methods; ++mi) {
            for (const auto& [path, handler] : m_static[mi]) {
                f(static_cast<http_method>(mi), path);
            }
            for (const auto& entry : m_param[mi]) {
                f(static_cast<http_method>(mi), entry.pattern);
            }
        }
    }

    template <typename F>
    void for_each_descriptor(F&& f) const {
        for (size_t mi = 0; mi < num_http_methods; ++mi) {
            for (const auto& [path, entry] : m_static[mi]) {
                f(entry.descriptor);
            }
            for (const auto& entry : m_param[mi]) {
                f(entry.callable.descriptor);
            }
        }
    }

  private:
    struct param_route_entry {
        std::string pattern;
        std::vector<std::string> segments;
        std::vector<std::string> names;
        std::vector<bool> is_param_seg;
        std::vector<path_converter> converters;
        route_entry callable;
    };

    std::array<std::unordered_map<std::string, route_entry, string_hash, std::equal_to<>>,
               num_http_methods>
        m_static;
    std::array<std::vector<param_route_entry>, num_http_methods> m_param;

    static bool match_param_route(
        const param_route_entry& entry,
        std::string_view path,
        const std::vector<std::string_view>& request_segments,
        std::vector<std::pair<std::string_view, std::string_view>>* path_params);

    void add_entry(http_method method, const std::string& pattern, route_entry entry);
};

}  // namespace fiasco::detail
