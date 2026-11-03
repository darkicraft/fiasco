// Per-request dependency caching, and how to bypass it per occurrence.
//
//   cmake --build build --target dependency_caching
//   FIASCO_PORT=8080 ./build/examples/dependency_caching
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
#include <cstdint>
#include <cstdlib>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

std::atomic<int> g_calls{0};

int next_value() {
    return g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
}

int cached_value() {
    return next_value();
}

}  // namespace

int main() {
    fiasco::server app{
        fiasco::server_config{.network = {.port = example_port()}},
    };

    app.get("/reset", []() {
        g_calls.store(0, std::memory_order_relaxed);
        return fiasco::json{{"calls", 0}};
    });

    app.get("/cached",
            [](int first [[= fiasco::depends{^^cached_value}]],
               int second [[= fiasco::depends{^^cached_value}]]) {
                return fiasco::json{
                    {"first", first},
                    {"second", second},
                    {"calls", g_calls.load(std::memory_order_relaxed)},
                };
            });

    app.get("/uncached",
            [](int first [[= fiasco::depends{^^next_value, {.use_cache = false}}]],
               int second [[= fiasco::depends{^^next_value, {.use_cache = false}}]]) {
                return fiasco::json{
                    {"first", first},
                    {"second", second},
                    {"calls", g_calls.load(std::memory_order_relaxed)},
                };
            });

    app.run();
}
