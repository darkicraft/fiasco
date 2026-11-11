// Nested models and standard collection fields, in both directions.
//
//   cmake --build build --target collections
//   FIASCO_PORT=8080 ./build/examples/collections
//
//   curl -X POST localhost:8080/collections \
//     -H 'Content-Type: application/json' \
//     -d '{
//       "values": [3, 1, 4, 1, 5],
//       "weights": {"a": 1.5, "b": 2.0},
//       "window": [2, 9],
//       "meta": [7, "x", true],
//       "regions": [{"x": 1, "y": 2}, {"x": 5, "y": 8, "note": "focus"}],
//       "focus": {"x": 5, "y": 8, "note": "hello"}
//     }'
//
// Supported here: vector, map, pair, tuple, optional, and nested structs.

#include <fiasco/fiasco.hpp>

#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

struct region {
    int x;
    int y;
    std::optional<std::string> note = std::nullopt;
};

struct collection_request {
    std::vector<int> values;
    std::map<std::string, double> weights;
    std::pair<int, int> window;
    std::tuple<int, std::string, bool> meta;
    std::vector<region> regions;
    std::optional<region> focus;
};

struct collection_response {
    int total;
    std::vector<std::string> weight_keys;
    std::pair<int, int> span;
    std::tuple<int, std::string, bool> meta;
    std::optional<region> first_region;
    std::optional<region> focus;
    std::map<std::string, int> region_counts;
};

}  // namespace

int main() {
    fiasco::app app;

    app.post("/collections", [](collection_request input) -> collection_response {
        int total = 0;
        for (int value : input.values) {
            total += value;
        }

        std::vector<std::string> keys;
        std::map<std::string, int> region_counts;
        for (const auto& [key, value] : input.weights) {
            (void)value;
            keys.push_back(key);
        }
        for (const auto& region : input.regions) {
            region_counts[region.note.value_or("unnamed")] += 1;
        }

        std::optional<region> first;
        if (!input.regions.empty()) {
            first = input.regions.front();
        }

        return {
            total,
            keys,
            input.window,
            input.meta,
            first,
            input.focus,
            region_counts,
        };
    });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
