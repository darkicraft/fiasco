// Worker threads and concurrent request handling.
//
//   cmake --build build --target threading
//   FIASCO_PORT=8080 ./build/examples/threading
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
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

}  // namespace

int main() {
    fiasco::app app;

    app.get("/sleep/{ms}", [](int ms) {
        ms = std::clamp(ms, 0, 5000);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return fiasco::json{{"slept_ms", ms}};
    });

    app.get("/cpu/{limit}", [](int limit) {
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
        return fiasco::json{{"limit", limit}, {"primes_below_or_equal", count}};
    });

    app.get("/whoami", []() {
        const auto hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
        return fiasco::json{{"thread_hash", std::to_string(hash)}};
    });

    app.run(fiasco::server_config{
        .network = {.port = example_port(), .workers = 4},
    });
}
