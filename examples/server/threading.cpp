// Worker threads and concurrent request handling.
//
//   cmake --build build --target threading
//   ./build/examples/threading
//
//   for i in 1 2 3 4; do curl -s localhost:8080/sleep/1000 & done; wait
//   for i in 1 2 3 4 5 6; do curl -s localhost:8080/whoami; done
//   curl localhost:8080/cpu/200000
//
// With four workers, the four sleeps finish in roughly one second, not four.
// whoami shows that requests are spread across different worker threads.

#include <fiasco/fiasco.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

using fiasco::get;
using fiasco::json;

namespace routes {

[[=get("/sleep/{ms}")]]
auto sleep(int ms) -> json {
    ms = std::clamp(ms, 0, 5000);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return json{{"slept_ms", ms}};
}

[[=get("/cpu/{limit}")]]
auto cpu(int limit) -> json {
    limit = std::clamp(limit, 2, 5'000'000);
    std::vector<bool> sieve(limit + 1, true);
    sieve[0] = sieve[1] = false;
    for (long long i = 2; i * i <= limit; ++i) {
        if (sieve[i]) {
            for (long long j = i * i; j <= limit; j += i) {
                sieve[j] = false;
            }
        }
    }
    const auto count = std::count(sieve.begin(), sieve.end(), true);
    return json{{"limit", limit}, {"primes_below_or_equal", count}};
}

[[=get("/whoami")]]
auto whoami() -> json {
    const auto hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return json{{"thread_hash", std::to_string(hash)}};
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
