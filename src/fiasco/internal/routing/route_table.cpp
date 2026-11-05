#include "fiasco/internal/routing/route_table.hpp"

#include "fiasco/internal/routing/pathing.hpp"

namespace fiasco::detail {

void route_table::add(http_method method, const std::string& pattern, handler_fn handler) {
    add_entry(method, pattern, route_entry{.handler = std::move(handler)});
}

void route_table::add(http_method method, const std::string& pattern, async_handler_fn handler) {
    add_entry(method, pattern, route_entry{.async_handler = std::move(handler)});
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
        m_static[method_index(method)][pattern] = std::move(entry);
        return;
    }

    param_route_entry param_entry;
    param_entry.pattern = pattern;
    param_entry.segments.reserve(segs.size());
    param_entry.names.reserve(segs.size());
    param_entry.is_param_seg.reserve(segs.size());

    for (auto seg : segs) {
        if (is_param_segment(seg)) {
            param_entry.is_param_seg.push_back(true);
            param_entry.names.emplace_back(param_name(seg));
        } else {
            param_entry.is_param_seg.push_back(false);
            param_entry.names.emplace_back();
        }
        param_entry.segments.emplace_back(seg);
    }

    param_entry.callable = std::move(entry);
    m_param[method_index(method)].push_back(std::move(param_entry));
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
        return result;
    }

    auto& param_routes = m_param[method_index(method)];
    if (param_routes.empty()) {
        return {false};
    }

    auto req_segs = split_path(path);

    for (auto& entry : param_routes) {
        if (entry.segments.size() != req_segs.size()) {
            continue;
        }

        bool ok = true;
        for (size_t i = 0; i < entry.segments.size(); ++i) {
            if (entry.is_param_seg[i]) {
                if (req_segs[i].empty()) {
                    ok = false;
                    break;
                }
            } else if (entry.segments[i] != req_segs[i]) {
                ok = false;
                break;
            }
        }

        if (!ok) {
            continue;
        }

        match_result result;
        result.matched = true;
        result.is_async = static_cast<bool>(entry.callable.async_handler);
        result.handler = &entry.callable.handler;
        result.async_handler = &entry.callable.async_handler;
        result.path_params.reserve(entry.segments.size());
        for (size_t i = 0; i < entry.segments.size(); ++i) {
            if (entry.is_param_seg[i]) {
                result.path_params.emplace_back(std::string_view{entry.names[i]}, req_segs[i]);
            }
        }
        return result;
    }

    return {false};
}

bool route_table::any_method_matches(std::string_view path) const {
    for (size_t mi = 0; mi < num_http_methods; ++mi) {
        if (m_static[mi].count(path)) {
            return true;
        }
    }

    auto req_segs = split_path(path);
    for (size_t mi = 0; mi < num_http_methods; ++mi) {
        for (const auto& entry : m_param[mi]) {
            if (entry.segments.size() != req_segs.size()) {
                continue;
            }
            bool ok = true;
            for (size_t i = 0; i < entry.segments.size(); ++i) {
                if (entry.is_param_seg[i]) {
                    if (req_segs[i].empty()) {
                        ok = false;
                        break;
                    }
                } else if (entry.segments[i] != req_segs[i]) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                return true;
            }
        }
    }
    return false;
}

void route_table::merge_with_prefix(route_table&& other, std::string_view prefix) {
    for (size_t mi = 0; mi < num_http_methods; ++mi) {
        for (auto& [path, handler] : other.m_static[mi]) {
            std::string full = join_prefix(prefix, path);
            m_static[mi][std::move(full)] = std::move(handler);
        }
        other.m_static[mi].clear();

        for (auto& entry : other.m_param[mi]) {
            std::string full_pattern = join_prefix(prefix, entry.pattern);
            add_entry(static_cast<http_method>(mi), full_pattern, std::move(entry.callable));
        }
        other.m_param[mi].clear();
    }
}

}  // namespace fiasco::detail