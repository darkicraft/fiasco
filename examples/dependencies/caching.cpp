// Per-request dependency caching, and how to bypass it per occurrence.
//
//   cmake --build build --target dependency_caching
//   ./build/examples/dependency_caching
//
//   curl localhost:8080/reset
//   curl localhost:8080/cached      # calls=1, both values are the same
//   curl localhost:8080/reset
//   curl localhost:8080/uncached    # calls=2, the values differ
//
// use_cache=false is FastAPI's use_cache=False: the provider is invoked for
// every annotated occurrence, while the result still lives until handler return.

#include <fiasco/fiasco.hpp>

#include <atomic>

using fiasco::depends;
using fiasco::get;
using fiasco::json;

namespace {

std::atomic<int> g_calls{0};

auto next_value() -> int {
    return g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
}

auto cached_value() -> int {
    return next_value();
}

}  // namespace

namespace routes {

[[=get("/reset")]]
auto reset() -> json {
    g_calls.store(0, std::memory_order_relaxed);
    return json{{"calls", 0}};
}

[[=get("/cached")]]
auto cached(int first [[=depends{^^cached_value}]],
                                 int second [[=depends{^^cached_value}]]) -> json {
    return json{
        {"first", first},
        {"second", second},
        {"calls", g_calls.load(std::memory_order_relaxed)},
    };
}

[[=get("/uncached")]]
auto uncached(int first [[=depends{^^next_value, {.use_cache = false}}]],
                                     int second [[=depends{^^next_value, {.use_cache = false}}]])
    -> json {
    return json{
        {"first", first},
        {"second", second},
        {"calls", g_calls.load(std::memory_order_relaxed)},
    };
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
