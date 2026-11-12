#pragma once

#include <iostream>
#include <string>
#include <utility>

#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/meta/annotations.hpp"
#include "fiasco/internal/meta/reflect.hpp"
#include "fiasco/internal/router_options.hpp"
#include "fiasco/internal/routing/descriptor.hpp"
#include "fiasco/internal/routing/function_traits.hpp"
#include "fiasco/internal/routing/pathing.hpp"
#include "fiasco/internal/routing/route_table.hpp"

namespace fiasco::detail {

class router {
  public:
    router() = default;
    explicit router(std::string prefix) { m_options.prefix = std::move(prefix); }
    explicit router(fiasco::router_options options)
        : m_options(std::move(options)) {}

    [[nodiscard]] const fiasco::router_options& options() const { return m_options; }

    void print_routes() const {
        m_routes.for_each_route(
            [](http_method, const std::string& path) { std::cout << path << "\n"; });
        std::cout << "\n\n";
    }

    template <typename F>
    void get(const std::string& path, F&& f) {
        add(http_method::get, path, std::forward<F>(f));
    }

    template <typename F>
    void post(const std::string& path, F&& f) {
        add(http_method::post, path, std::forward<F>(f));
    }

    template <typename F>
    void put(const std::string& path, F&& f) {
        add(http_method::put, path, std::forward<F>(f));
    }

    template <typename F>
    void del(const std::string& path, F&& f) {
        add(http_method::del, path, std::forward<F>(f));
    }

    template <typename F>
    void patch(const std::string& path, F&& f) {
        add(http_method::patch, path, std::forward<F>(f));
    }

    void include_router(router sub, const std::string& prefix = "") {
        std::string full_prefix = join_prefix(prefix, sub.m_options.prefix);
        m_routes.merge_with_prefix(std::move(sub.m_routes), full_prefix, m_options.tags);
    }

    template <std::meta::info Ns>
    static consteval std::size_t annotated_route_count() {
        std::size_t count = 0;
        for (auto member :
             std::meta::members_of(Ns, std::meta::access_context::current())) {
            if (!std::meta::is_function(member)) {
                continue;
            }
            if (!std::meta::annotations_of_with_type(member, ^^fiasco::route_annotation).empty()) {
                ++count;
            }
        }
        return count;
    }

    template <std::meta::info Ns>
    static consteval bool has_duplicate_routes() {
        std::vector<std::pair<http_method, std::string_view>> routes;
        for (auto member :
             std::meta::members_of(Ns, std::meta::access_context::current())) {
            if (!std::meta::is_function(member)) {
                continue;
            }
            const auto annotations =
                std::meta::annotations_of_with_type(member, ^^fiasco::route_annotation);
            if (!annotations.empty()) {
                const auto annotation =
                    std::meta::extract<fiasco::route_annotation>(annotations[0]);
                routes.emplace_back(annotation.method, annotation.path.view());
            }
        }
        for (std::size_t i = 0; i < routes.size(); ++i) {
            for (std::size_t j = i + 1; j < routes.size(); ++j) {
                if (routes[i].first == routes[j].first &&
                    routes[i].second == routes[j].second) {
                    return true;
                }
            }
        }
        return false;
    }

    template <std::meta::info Ns>
    void register_api() {
        static_assert(annotated_route_count<Ns>() > 0,
                      "register_api: namespace contains no functions annotated with a route");
        static_assert(!has_duplicate_routes<Ns>(),
                      "register_api: duplicate method and path in namespace");
        template for (constexpr auto member :
                      std::define_static_array(
                          std::meta::members_of(Ns, std::meta::access_context::current()))) {
            if constexpr (std::meta::is_function(member)) {
                constexpr auto annotations = std::define_static_array(
                    std::meta::annotations_of_with_type(member, ^^fiasco::route_annotation));
                if constexpr (!annotations.empty()) {
                    constexpr auto annotation =
                        std::meta::extract<fiasco::route_annotation>(annotations[0]);
                    add(annotation.method, std::string(annotation.path.view()),
                        meta::reflected_callable<member>{}, annotation.options);
                }
            }
        }
    }

    template <typename F>
    void for_each_descriptor(F&& f) const {
        m_routes.for_each_descriptor(std::forward<F>(f));
    }

    match_result match(http_method method, std::string_view path) {
        return m_routes.match(method, path);
    }

    bool any_method_matches(std::string_view path) const {
        return m_routes.any_method_matches(path);
    }

  private:
    route_table m_routes;
    fiasco::router_options m_options;

    template <typename F>
    void add(http_method method,
             const std::string& pattern,
             F&& f,
             const fiasco::route_options& options = {}) {
        auto descriptor = describe<std::decay_t<F>>(method, pattern, options);
        descriptor.tags.insert(descriptor.tags.end(), m_options.tags.begin(),
                               m_options.tags.end());
        descriptor.deprecated = descriptor.deprecated || m_options.deprecated;
        descriptor.include_in_schema =
            descriptor.include_in_schema && m_options.include_in_schema;

        if constexpr (returns_async_handler<F>()) {
            m_routes.add(method, pattern, make_async_handler(std::forward<F>(f)),
                         std::move(descriptor));
        } else {
            m_routes.add(method, pattern, make_handler(std::forward<F>(f)),
                         std::move(descriptor));
        }
    }

    friend class server;
    friend class app;
};

}  // namespace fiasco::detail