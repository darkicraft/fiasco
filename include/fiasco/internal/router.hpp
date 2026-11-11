#pragma once

#include <iostream>
#include <string>
#include <utility>

#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/meta/annotations.hpp"
#include "fiasco/internal/meta/reflect.hpp"
#include "fiasco/internal/routing/function_traits.hpp"
#include "fiasco/internal/routing/pathing.hpp"
#include "fiasco/internal/routing/route_table.hpp"

namespace fiasco::detail {

class router {
  public:
    router() = default;
    explicit router(std::string prefix)
        : m_prefix(std::move(prefix)) {}

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
        std::string full_prefix = join_prefix(prefix, sub.m_prefix);
        m_routes.merge_with_prefix(std::move(sub.m_routes), full_prefix);
    }

    template <std::meta::info Ns>
    void register_api() {
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
                        meta::reflected_callable<member>{});
                }
            }
        }
    }

    match_result match(http_method method, std::string_view path) {
        return m_routes.match(method, path);
    }

    bool any_method_matches(std::string_view path) const {
        return m_routes.any_method_matches(path);
    }

  private:
    route_table m_routes;
    const std::string m_prefix = "";

    template <typename F>
    void add(http_method method, const std::string& pattern, F&& f) {
        if constexpr (returns_async_handler<F>()) {
            m_routes.add(method, pattern, make_async_handler(std::forward<F>(f)));
        } else {
            m_routes.add(method, pattern, make_handler(std::forward<F>(f)));
        }
    }

    friend class server;
    friend class app;
};

}  // namespace fiasco::detail