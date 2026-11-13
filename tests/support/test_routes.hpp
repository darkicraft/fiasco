#pragma once

// Test-only route registration. The public API is annotation-only; these
// overloads keep lambda-heavy tests concise without reopening the runtime API.

#include <string>
#include <utility>

#include "fiasco/app/app.hpp"
#include "fiasco/internal/router.hpp"

namespace fiasco::detail {

struct test_route_access {
    template <typename F>
    static void add(app& target, http_method method, std::string path, F&& handler) {
        target.add_route_for_test(method, std::move(path), std::forward<F>(handler));
    }

    template <typename F>
    static void add(router& target, http_method method, std::string path, F&& handler) {
        target.add(method, std::move(path), std::forward<F>(handler));
    }
};

}  // namespace fiasco::detail

namespace fiasco_test {

template <typename F>
void get(fiasco::detail::app& app, std::string path, F&& handler) {
    fiasco::detail::test_route_access::add(
        app, fiasco::detail::http_method::get, std::move(path), std::forward<F>(handler));
}

template <typename F>
void post(fiasco::detail::app& app, std::string path, F&& handler) {
    fiasco::detail::test_route_access::add(
        app, fiasco::detail::http_method::post, std::move(path), std::forward<F>(handler));
}

template <typename F>
void put(fiasco::detail::app& app, std::string path, F&& handler) {
    fiasco::detail::test_route_access::add(
        app, fiasco::detail::http_method::put, std::move(path), std::forward<F>(handler));
}

template <typename F>
void patch(fiasco::detail::app& app, std::string path, F&& handler) {
    fiasco::detail::test_route_access::add(
        app, fiasco::detail::http_method::patch, std::move(path), std::forward<F>(handler));
}

template <typename F>
void del(fiasco::detail::app& app, std::string path, F&& handler) {
    fiasco::detail::test_route_access::add(
        app, fiasco::detail::http_method::del, std::move(path), std::forward<F>(handler));
}

template <typename F>
void get(fiasco::detail::router& router, std::string path, F&& handler) {
    fiasco::detail::test_route_access::add(
        router, fiasco::detail::http_method::get, std::move(path), std::forward<F>(handler));
}

template <typename F>
void post(fiasco::detail::router& router, std::string path, F&& handler) {
    fiasco::detail::test_route_access::add(
        router, fiasco::detail::http_method::post, std::move(path), std::forward<F>(handler));
}

template <typename F>
void put(fiasco::detail::router& router, std::string path, F&& handler) {
    fiasco::detail::test_route_access::add(
        router, fiasco::detail::http_method::put, std::move(path), std::forward<F>(handler));
}

template <typename F>
void patch(fiasco::detail::router& router, std::string path, F&& handler) {
    fiasco::detail::test_route_access::add(
        router, fiasco::detail::http_method::patch, std::move(path), std::forward<F>(handler));
}

template <typename F>
void del(fiasco::detail::router& router, std::string path, F&& handler) {
    fiasco::detail::test_route_access::add(
        router, fiasco::detail::http_method::del, std::move(path), std::forward<F>(handler));
}

}  // namespace fiasco_test
