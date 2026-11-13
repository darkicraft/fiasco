#include "fiasco/routing/route_table.hpp"

#include <stdexcept>
#include <utility>

#include "fiasco/routing/pathing.hpp"

namespace fiasco::detail {

namespace {

void append_tags(route_descriptor& descriptor, std::span<const std::string> tags) {
    descriptor.tags.insert(descriptor.tags.end(), tags.begin(), tags.end());
}

}  // namespace

void route_table::add(http_method method,
                      const std::string& pattern,
                      handler_fn handler,
                      route_descriptor descriptor) {
    add_entry(method,
              pattern,
              route_entry{.handler = std::move(handler), .descriptor = std::move(descriptor)});
}

void route_table::add(http_method method,
                      const std::string& pattern,
                      async_handler_fn handler,
                      route_descriptor descriptor) {
    add_entry(
        method,
        pattern,
        route_entry{.async_handler = std::move(handler), .descriptor = std::move(descriptor)});
}

void route_table::add_entry(http_method method, const std::string& pattern, route_entry entry) {
    auto segs = split_path(pattern);

    bool has_param = false;
    for (auto seg : segs) {
        if (is_param_segment(seg)) {
            has_param = true;
            break;
        }
    }

    if (!has_param) {
        auto& table = m_static[method_index(method)];
        if (table.contains(pattern)) {
            throw std::logic_error("duplicate route: " + std::string(method_to_string(method)) +
                                   " " + pattern);
        }
        table[pattern] = std::move(entry);
        return;
    }

    for (const auto& existing : m_param[method_index(method)]) {
        if (existing.pattern == pattern) {
            throw std::logic_error("duplicate route: " + std::string(method_to_string(method)) +
                                   " " + pattern);
        }
    }

    param_route_entry param_entry;
    param_entry.pattern = pattern;
    param_entry.segments.reserve(segs.size());
    param_entry.names.reserve(segs.size());
    param_entry.is_param_seg.reserve(segs.size());
    param_entry.converters.reserve(segs.size());

    for (std::size_t i = 0; i < segs.size(); ++i) {
        const auto seg = segs[i];
        if (is_param_segment(seg)) {
            const auto converter = param_converter(seg);
            if (converter == path_converter::unknown) {
                throw std::logic_error("unknown path converter in route: " + pattern);
            }
            if (converter == path_converter::path && i + 1 != segs.size()) {
                throw std::logic_error("path converter must be the final segment: " + pattern);
            }
            param_entry.is_param_seg.push_back(true);
            param_entry.names.emplace_back(param_name(seg));
            param_entry.converters.push_back(converter);
        } else {
            param_entry.is_param_seg.push_back(false);
            param_entry.names.emplace_back();
            param_entry.converters.push_back(path_converter::string);
        }
        param_entry.segments.emplace_back(seg);
    }

    param_entry.callable = std::move(entry);
    m_param[method_index(method)].push_back(std::move(param_entry));
}

bool route_table::match_param_route(
    const param_route_entry& entry,
    std::string_view path,
    const std::vector<std::string_view>& request_segments,
    std::vector<std::pair<std::string_view, std::string_view>>* path_params) {
    bool has_catch_all = false;
    std::size_t catch_all_index = 0;
    for (std::size_t i = 0; i < entry.segments.size(); ++i) {
        if (entry.is_param_seg[i] && entry.converters[i] == path_converter::path) {
            has_catch_all = true;
            catch_all_index = i;
            break;
        }
    }

    if (has_catch_all) {
        if (request_segments.size() <= catch_all_index) {
            return false;
        }
    } else if (entry.segments.size() != request_segments.size()) {
        return false;
    }

    for (std::size_t i = 0; i < entry.segments.size(); ++i) {
        if (entry.is_param_seg[i]) {
            if (entry.converters[i] == path_converter::path) {
                // Validated at registration: a catch-all is the final segment.
                const auto* start = request_segments[i].data();
                const auto* end = path.data() + path.size();
                const auto value = std::string_view(start, static_cast<std::size_t>(end - start));
                if (path_params != nullptr) {
                    path_params->emplace_back(entry.names[i], value);
                }
                return true;
            }

            const auto value = request_segments[i];
            if (value.empty()) {
                return false;
            }
            if (path_params != nullptr) {
                path_params->emplace_back(entry.names[i], value);
            }
        } else if (entry.segments[i] != request_segments[i]) {
            return false;
        }
    }

    return !has_catch_all;
}

match_result route_table::match(http_method method, std::string_view path) {
    auto& static_routes = m_static[method_index(method)];
    auto static_hit = static_routes.find(path);
    if (static_hit != static_routes.end()) {
        match_result result;
        result.matched = true;
        result.is_async = static_cast<bool>(static_hit->second.async_handler);
        result.handler = &static_hit->second.handler;
        result.async_handler = &static_hit->second.async_handler;
        result.descriptor = &static_hit->second.descriptor;
        return result;
    }

    auto& param_routes = m_param[method_index(method)];
    if (param_routes.empty()) {
        return {false};
    }

    auto req_segs = split_path(path);

    for (auto& entry : param_routes) {
        match_result result;
        if (!match_param_route(entry, path, req_segs, &result.path_params)) {
            continue;
        }

        result.matched = true;
        result.is_async = static_cast<bool>(entry.callable.async_handler);
        result.handler = &entry.callable.handler;
        result.async_handler = &entry.callable.async_handler;
        result.descriptor = &entry.callable.descriptor;
        return result;
    }

    return {false};
}

bool route_table::any_method_matches(std::string_view path) const {
    return !allowed_methods(path).empty();
}

std::vector<http_method> route_table::allowed_methods(std::string_view path) const {
    std::vector<http_method> methods;
    for (size_t mi = 0; mi < num_http_methods; ++mi) {
        if (m_static[mi].count(path)) {
            methods.push_back(static_cast<http_method>(mi));
        }
    }

    const auto req_segs = split_path(path);
    for (size_t mi = 0; mi < num_http_methods; ++mi) {
        for (const auto& entry : m_param[mi]) {
            if (match_param_route(entry, path, req_segs, nullptr)) {
                methods.push_back(static_cast<http_method>(mi));
                break;
            }
        }
    }
    return methods;
}

void route_table::merge_with_prefix(route_table&& other,
                                    std::string_view prefix,
                                    std::span<const std::string> extra_tags) {
    for (size_t mi = 0; mi < num_http_methods; ++mi) {
        for (auto& [path, handler] : other.m_static[mi]) {
            std::string full = join_prefix(prefix, path);
            handler.descriptor.path = join_prefix(prefix, handler.descriptor.path);
            append_tags(handler.descriptor, extra_tags);
            m_static[mi][std::move(full)] = std::move(handler);
        }
        other.m_static[mi].clear();

        for (auto& entry : other.m_param[mi]) {
            const std::string full_pattern = join_prefix(prefix, entry.pattern);
            entry.callable.descriptor.path = join_prefix(prefix, entry.callable.descriptor.path);
            append_tags(entry.callable.descriptor, extra_tags);
            add_entry(static_cast<http_method>(mi), full_pattern, std::move(entry.callable));
        }
        other.m_param[mi].clear();
    }
}

}  // namespace fiasco::detail
