// =============================================================================
// fiasco feature tour
//
// Exercises, on purpose, in an over-engineered way:
//   - path params by name, order-independent, every supported scalar type
//   - query params by name (int / bool / string)
//   - multiple body models embedded by name  ({"req": {...}, "tuning": {...}})
//   - single body model (batch)
//   - required vs nullable vs defaulted members (Pydantic semantics)
//   - field constraints: gt ge lt le multiple_of min_length max_length alias
//   - optional / vector / map / pair / tuple / nested structs (both directions)
//   - fiasco::json type-erased wrapper (incl. recursive building)
//   - fiasco::response factories + status/header mutation
//   - sub-router mounting (/v1, /tools)
//   - multi-threaded server (sleep / cpu / whoami endpoints)
//
// Designated initializers in `field{...}` follow the declaration order:
//   gt, ge, lt, le, multiple_of, min_length, max_length,
//   pattern, alias, title, description
// =============================================================================
#include <fiasco/fiasco.hpp>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

using fiasco::field;

// =============================================================================
// REQUEST MODELS
// =============================================================================

struct region {
    int x [[= field{.ge = 0, .lt = 1000}]];  // required, constrained
    int y [[= field{.ge = 0, .lt = 1000}]];
    std::optional<std::string> note = std::nullopt;  // optional + nullable
};

struct filter_cfg {
    std::optional<int> min_degree = std::nullopt;       // exclude graph nodes below this
    std::optional<double> matrix_scale = std::nullopt;  // scale all matrix values
    std::vector<std::string> tags = {};                 // echoed back, sorted
};

struct analyze_request {
    std::string label [[= field{.min_length = 1, .max_length = 64}]];  // required
    std::string username [[= field{.alias = "user-name"}]] = "anon";   // JSON key: "user-name"
    std::vector<int> weights [[= field{.max_length = 16}]] = {};
    std::map<std::string, double> offsets = {};
    std::pair<int, int> window = {0, 4};
    std::tuple<int, std::string, bool> meta = {0, "", false};
    std::vector<region> regions = {};
    std::optional<region> focus;  // NO default => required, but nullable
    filter_cfg filters = {};
};

struct tuning_cfg {
    int percent [[= field{.gt = 0, .le = 100}]];  // required
    int boxes [[= field{.multiple_of = 3}]] = 3;
    double damping [[= field{.ge = 0.0, .lt = 10.0}]] = 0.5;
    std::string code [[= field{.min_length = 2, .max_length = 8}]] = "ab";
    int matrix_n [[= field{.ge = 1, .le = 8}]] = 4;  // matrix is N x N
};

struct item_model {
    std::string sku [[= field{.min_length = 2, .max_length = 20}]];
    double price [[= field{.gt = 0.0, .alias = "unit-price"}]];
    int quantity [[= field{.ge = 0, .le = 10000}]] = 1;
    int pack_size [[= field{.multiple_of = 6}]] = 6;
    std::vector<std::string> tags [[= field{.min_length = 1}]];  // required, non-empty
    std::optional<double> discount = std::nullopt;
    std::optional<std::string> notes;  // required + nullable
};

struct item_patch {
    std::optional<std::string> sku = std::nullopt;
    std::optional<double> price = std::nullopt;
    std::optional<int> quantity = std::nullopt;
    std::vector<std::string> add_tags = {};
};

struct batch_request {
    std::vector<item_model> items;
    std::map<std::string, std::vector<int>> groups = {};
};

// =============================================================================
// RESPONSE MODELS
// =============================================================================

struct vec3 {
    double x, y, z;
};

struct weighted_row {
    int index;
    std::vector<double> raw;
    std::vector<double> weighted;
    double raw_sum;
    double weighted_sum;
    double dot_with_unit;
};

struct mode_info {
    int index;
    double value;
    int count;
};

struct statistics {
    double min;
    double max;
    double mean;
    double median;
    double variance;
    double std_dev;
    mode_info mode;  // true mode (most frequent value at 1e-3 resolution)
    double weighted_mean;
};

struct prime_info {
    long long value;
    bool is_prime;
    std::optional<long long> next_prime;
    std::vector<long long> factors;
    int factor_count;
};

struct graph_node {
    int id;
    std::string label;
    std::vector<int> neighbors;
    int degree;
    bool is_leaf;
    std::optional<std::string> tag;
};

struct graph_stats {
    int node_count;
    int edge_count;
    int max_degree;
    int min_degree;
    double mean_degree;
    std::vector<int> leaf_ids;
    std::vector<int> hub_ids;  // degree > mean
};

struct char_freq {
    std::string ch;
    int count;
};

struct label_analysis {
    std::string original;
    int byte_length;
    int codepoint_length;
    std::vector<char_freq> freq;
    bool is_palindrome;
    std::optional<std::string> longest_run;
};

struct path_echo {
    int seed;
    long long multiplier;
    float ratio;
    double ratio_times_damping;
    bool strict;
    std::string mode;
};

struct query_echo {
    int page;
    bool verbose;
    std::string q;
};

struct extras_block {
    std::string username;
    std::map<std::string, double> offsets;
    std::map<std::string, double> scaled_offsets;
    double offsets_sum;
    std::pair<int, int> window;
    int window_span;
    std::tuple<int, std::string, bool> meta;
    std::vector<region> regions;
    std::optional<int> closest_region_index;
    std::optional<region> focus;
    tuning_cfg tuning;  // request model reused as a response model
};

struct combined_info {
    bool overflow;
    long long value;
    bool is_prime;
    std::optional<std::vector<long long>> factors;  // null when too large to factor
};

struct report {
    path_echo path;
    query_echo query;
    std::string label;
    std::vector<std::string> sorted_tags;
    std::optional<double> matrix_scale;
    vec3 unit_vector;
    std::vector<weighted_row> matrix;
    statistics stats;
    std::vector<prime_info> primes;
    std::optional<std::string> secret;
    std::vector<graph_node> graph;
    graph_stats g_stats;
    label_analysis analysis;
    extras_block extras;
    combined_info combined;
    std::optional<std::vector<std::string>> debug;  // only when ?verbose=true
};

struct item_view {
    int item_id;
    bool dry_run;
    item_model item;  // request model reused as response (alias applies: "unit-price")
    double total_value;
    std::string sku_upper;
    std::vector<std::string> sorted_tags;
    std::optional<double> discounted_price;
};

struct batch_report {
    int item_count;
    double grand_total;
    std::map<std::string, int> group_sums;
    std::map<std::string, std::vector<std::string>> skus_by_tag;
    std::optional<item_model> priciest;
    std::vector<std::pair<std::string, double>> price_list;
};

// =============================================================================
// HELPERS
// =============================================================================

static bool is_prime(long long n) {
    if (n < 2) {
        return false;
    }
    for (long long i = 2; i * i <= n; ++i) {
        if (n % i == 0) {
            return false;
        }
    }
    return true;
}

static std::optional<long long> next_prime(long long n) {
    for (long long i = n + 1; i < n + 1000; ++i) {
        if (is_prime(i)) {
            return i;
        }
    }
    return std::nullopt;
}

static std::vector<long long> factorize(long long n) {
    std::vector<long long> f;
    if (n < 2) {
        return f;
    }
    for (long long i = 2; i * i <= n; ++i) {
        while (n % i == 0) {
            f.push_back(i);
            n /= i;
        }
    }
    if (n > 1) {
        f.push_back(n);
    }
    return f;
}

static prime_info make_prime_info(long long v) {
    auto f = factorize(v);
    return {v, is_prime(v), next_prime(v), f, (int)f.size()};
}

static int utf8_codepoints(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) {
            ++n;
        }
    }
    return n;
}

static fiasco::json nest(int depth) {
    if (depth <= 0) {
        return fiasco::json{{"leaf", true}};
    }
    return fiasco::json{{"depth", depth}, {"child", nest(depth - 1)}};
}

// =============================================================================
// MAIN HANDLER LOGIC
// =============================================================================

static report analyze(bool strict, float ratio, long long multiplier, int seed,  // path (scrambled)
                      int page, bool verbose, std::string q,                      // query
                      analyze_request req, tuning_cfg tuning)                     // 2 body models
{
    report r{};
    const double scale = req.filters.matrix_scale.value_or(1.0);
    const int n = tuning.matrix_n;

    r.path = {seed, multiplier, ratio, ratio * tuning.damping, strict, strict ? "strict" : "lenient"};
    r.query = {page, verbose, q};
    r.label = req.label;
    r.matrix_scale = req.filters.matrix_scale;

    // ---- unit vector from seed ----
    double angle = seed * 0.1;
    vec3 uv{std::cos(angle), std::sin(angle), std::cos(angle) * std::sin(angle)};
    double mag = std::sqrt(uv.x * uv.x + uv.y * uv.y + uv.z * uv.z);
    uv.x /= mag;
    uv.y /= mag;
    uv.z /= mag;
    r.unit_vector = uv;

    // ---- N x N matrix ----
    auto w = req.weights;
    w.resize(n, 1);

    std::vector<double> flat_raw, flat_weighted;
    for (int i = 0; i < n; ++i) {
        std::vector<double> raw, wted;
        for (int j = 0; j < n; ++j) {
            double v = std::sin((double)seed * (i + 1.0) * (j + 1.0)) * scale;
            raw.push_back(v);
            wted.push_back(v * w[i] * (double)multiplier * tuning.damping);
        }
        double rsum = std::accumulate(raw.begin(), raw.end(), 0.0);
        double wsum = std::accumulate(wted.begin(), wted.end(), 0.0);
        double dot = raw[0] * uv.x + (n > 1 ? raw[1] * uv.y : 0.0) + (n > 2 ? raw[2] * uv.z : 0.0);
        r.matrix.push_back({i, raw, wted, rsum, wsum, dot});
        flat_raw.insert(flat_raw.end(), raw.begin(), raw.end());
        flat_weighted.insert(flat_weighted.end(), wted.begin(), wted.end());
    }

    // ---- stats ----
    {
        double fmin = *std::min_element(flat_raw.begin(), flat_raw.end());
        double fmax = *std::max_element(flat_raw.begin(), flat_raw.end());
        double fmean = std::accumulate(flat_raw.begin(), flat_raw.end(), 0.0) / flat_raw.size();
        double var = 0;
        for (double v : flat_raw) {
            var += (v - fmean) * (v - fmean);
        }
        var /= flat_raw.size();

        auto sorted = flat_raw;
        std::sort(sorted.begin(), sorted.end());
        size_t m = sorted.size();
        double median = m % 2 ? sorted[m / 2] : (sorted[m / 2 - 1] + sorted[m / 2]) / 2.0;

        std::map<long long, int> buckets, first_idx;
        for (size_t k = 0; k < flat_raw.size(); ++k) {
            long long key = std::llround(flat_raw[k] * 1000.0);
            if (!buckets.count(key)) {
                first_idx[key] = (int)k;
            }
            ++buckets[key];
        }
        long long best_key = buckets.begin()->first;
        int best_cnt = 0;
        for (auto& [key, cnt] : buckets) {
            if (cnt > best_cnt) {
                best_cnt = cnt;
                best_key = key;
            }
        }
        int mode_idx = first_idx[best_key];

        double wmean =
            std::accumulate(flat_weighted.begin(), flat_weighted.end(), 0.0) / flat_weighted.size();

        r.stats = {fmin, fmax, fmean, median, var, std::sqrt(var),
                   {mode_idx, flat_raw[mode_idx], best_cnt}, wmean};
    }

    // ---- primes around seed ----
    for (int off = -2; off <= 2; ++off) {
        r.primes.push_back(make_prime_info(std::llabs((long long)seed + off)));
    }
    if (is_prime(seed) && (!strict || multiplier % 2 != 0)) {
        r.secret = "seed " + std::to_string(seed) + " is prime. you found it.";
    }

    // ---- graph (size driven by ?page=) ----
    {
        const int node_count = 4 + std::clamp(page, 0, 6);
        const int min_deg = req.filters.min_degree.value_or(0);
        const auto& tags = req.filters.tags;

        std::vector<std::vector<int>> adj(node_count);
        for (int i = 0; i < node_count; ++i) {
            for (int j = 0; j < node_count; ++j) {
                if (i != j && ((long long)seed * (i + 1) * (j + 1)) % 3 == 0) {
                    adj[i].push_back(j);
                }
            }
        }

        // survivors by ORIGINAL degree, then drop edges to removed nodes
        std::vector<bool> keep(node_count);
        for (int i = 0; i < node_count; ++i) {
            keep[i] = (int)adj[i].size() >= min_deg;
        }
        std::vector<std::vector<int>> fadj(node_count);
        for (int i = 0; i < node_count; ++i) {
            if (!keep[i]) {
                continue;
            }
            for (int j : adj[i]) {
                if (keep[j]) {
                    fadj[i].push_back(j);
                }
            }
        }

        int kept = 0, deg_sum = 0, max_deg = 0, min_seen = INT_MAX;
        for (int i = 0; i < node_count; ++i) {
            if (!keep[i]) {
                continue;
            }
            ++kept;
            int d = (int)fadj[i].size();
            deg_sum += d;
            max_deg = std::max(max_deg, d);
            min_seen = std::min(min_seen, d);
        }
        double mean_deg = kept ? (double)deg_sum / kept : 0.0;

        std::vector<int> leaf_ids, hub_ids;
        for (int i = 0; i < node_count; ++i) {
            if (!keep[i]) {
                continue;
            }
            int d = (int)fadj[i].size();
            bool leaf = d == 1;
            if (leaf) {
                leaf_ids.push_back(i);
            }
            if (d > mean_deg) {
                hub_ids.push_back(i);
            }
            std::optional<std::string> tag;
            if ((size_t)i < tags.size()) {
                tag = tags[i];
            }
            r.graph.push_back({i, "node_" + std::to_string(i), fadj[i], d, leaf, tag});
        }
        r.g_stats = {kept, deg_sum / 2, max_deg, min_seen == INT_MAX ? 0 : min_seen, mean_deg,
                     leaf_ids, hub_ids};
    }

    // ---- tags ----
    r.sorted_tags = req.filters.tags;
    std::sort(r.sorted_tags.begin(), r.sorted_tags.end());

    // ---- label analysis (byte-wise freq, codepoint-aware length) ----
    {
        const std::string& lbl = req.label;
        std::map<char, int> freq_map;
        for (char c : lbl) {
            ++freq_map[c];
        }
        std::vector<char_freq> freq;
        for (auto& [c, cnt] : freq_map) {
            freq.push_back({std::string(1, c), cnt});
        }
        bool pal = lbl == std::string(lbl.rbegin(), lbl.rend());

        std::optional<std::string> longest;
        if (!lbl.empty()) {
            char cur = lbl[0], best_c = cur;
            int run = 1, best = 1;
            for (size_t i = 1; i < lbl.size(); ++i) {
                if (lbl[i] == cur) {
                    if (++run > best) {
                        best = run;
                        best_c = cur;
                    }
                } else {
                    cur = lbl[i];
                    run = 1;
                }
            }
            longest = std::string(best, best_c);
        }
        r.analysis = {lbl, (int)lbl.size(), utf8_codepoints(lbl), freq, pal, longest};
    }

    // ---- extras: map / pair / tuple / regions / focus ----
    {
        extras_block& e = r.extras;
        e.username = req.username;
        e.offsets = req.offsets;
        for (auto& [k, v] : req.offsets) {
            e.scaled_offsets[k] = v * scale;
            e.offsets_sum += v;
        }
        e.window = req.window;
        e.window_span = req.window.second - req.window.first;
        e.meta = req.meta;
        e.regions = req.regions;
        long long best_d = LLONG_MAX;
        for (size_t i = 0; i < req.regions.size(); ++i) {
            long long d = (long long)req.regions[i].x * req.regions[i].x +
                          (long long)req.regions[i].y * req.regions[i].y;
            if (d < best_d) {
                best_d = d;
                e.closest_region_index = (int)i;
            }
        }
        e.focus = req.focus;
        e.tuning = tuning;
    }

    // ---- combined (overflow-safe; skip factoring huge values) ----
    {
        long long prod = 0;
        bool ovf = __builtin_mul_overflow((long long)seed, multiplier, &prod);
        long long v = ovf ? 0 : std::llabs(prod);
        std::optional<std::vector<long long>> f;
        if (!ovf && v <= 1'000'000'000'000LL) {
            f = factorize(v);
        }
        r.combined = {ovf, v, is_prime(v), f};
    }

    if (verbose) {
        r.debug = std::vector<std::string>{
            "matrix_n=" + std::to_string(n),
            "weights_after_resize=" + std::to_string(w.size()),
            "graph_nodes=" + std::to_string(r.graph.size()),
            "boxes=" + std::to_string(tuning.boxes) + " code=" + tuning.code,
        };
    }
    return r;
}

// =============================================================================
// ROUTES
// =============================================================================

int main(int argc, char** argv) {
    fiasco::server_config config{
        .network = {.host = "0.0.0.0", .port = 8080, .workers = 4},
    };

    if (const char* env = std::getenv("FIASCO_LOG"); env && std::string_view{env} == "off") {
        config.logging.enabled = false;
    }

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--no-log") {
            config.logging.enabled = false;
        } else if (arg == "--log") {
            config.logging.enabled = true;
        } else if (arg == "--no-access-log") {
            config.logging.access = false;
        } else if (arg == "--access-log") {
            config.logging.access = true;
        }
    }

    fiasco::server app{std::move(config)};

    // ---- plain value wrapped automatically ----
    app.get("/ping", []() { return "pong"; });

    // ---- sub-router: /v1 ----
    fiasco::router v1("/v1");

    // heavy endpoint: 4 path params (all scalar kinds, scrambled order),
    // 3 query params, 2 embedded body models
    // v1.post("/analyze/{seed}/{multiplier}/{ratio}/{strict}",
    //         [](bool strict, float ratio, long long multiplier, int seed, int page, bool verbose,
    //            std::string q, analyze_request req, tuning_cfg tuning) -> report {
    //             return analyze(strict, ratio, multiplier, seed, page, verbose, q, req, tuning);
    //         });

    v1.post("/analyze/{seed}/{multiplier}/{ratio}/{strict}", analyze);

    // query-only, returns vector<model>
    v1.get("/primes", [](int start, int count) -> fiasco::response {
        if (count < 1 || count > 200) {
            return fiasco::response::error("count must be in 1..200", 400);
        }
        std::vector<prime_info> out;
        for (int i = 0; i < count; ++i) {
            out.push_back(make_prime_info(std::llabs((long long)start + i)));
        }
        // handler returns fiasco::response explicitly because it has two return shapes
        int found = 0;
        for (auto& p : out) {
            found += p.is_prime ? 1 : 0;
        }
        return fiasco::response::json("{\"count\":" + std::to_string(count) +
                                      ",\"start\":" + std::to_string(start) +
                                      ",\"primes_found\":" + std::to_string(found) + "}");
    });

    // path + query + body model (constraints w/ alias on both request and response)
    v1.put("/items/{item_id}", [](int item_id, bool dry_run, item_model item) -> item_view {
        std::string up = item.sku;
        std::transform(up.begin(), up.end(), up.begin(), [](unsigned char c) { return std::toupper(c); });
        auto tags = item.tags;
        std::sort(tags.begin(), tags.end());
        std::optional<double> disc;
        if (item.discount) {
            disc = item.price * (1.0 - *item.discount);
        }
        return {item_id, dry_run, item, item.price * item.quantity, up, tags, disc};
    });

    // everything optional: tests absent vs null vs present
    v1.patch("/items/{item_id}", [](int item_id, item_patch patch) {
        return fiasco::json{{"item_id", item_id},
                            {"sku_changed", patch.sku.has_value()},
                            {"price_changed", patch.price.has_value()},
                            {"quantity_changed", patch.quantity.has_value()},
                            {"tags_added", (int)patch.add_tags.size()}};
    });

    // single body model, map<string, vector<int>>, vector<pair>, optional<model>
    v1.post("/batch", [](batch_request batch) -> batch_report {
        batch_report out{};
        out.item_count = (int)batch.items.size();
        const item_model* top = nullptr;
        for (auto& it : batch.items) {
            out.grand_total += it.price * it.quantity;
            out.price_list.emplace_back(it.sku, it.price);
            for (auto& t : it.tags) {
                out.skus_by_tag[t].push_back(it.sku);
            }
            if (!top || it.price > top->price) {
                top = &it;
            }
        }
        if (top) {
            out.priciest = *top;
        }
        for (auto& [g, vals] : batch.groups) {
            out.group_sums[g] = std::accumulate(vals.begin(), vals.end(), 0);
        }
        return out;
    });

    // ---- sub-router: /tools (response factories, threading, json wrapper) ----
    fiasco::router tools("/tools");

    tools.get("/text", []() { return fiasco::response::text("plain text"); });
    tools.get("/html", []() { return fiasco::response::html("<h1>Hello</h1>"); });
    tools.get("/raw-json",
              []() { return fiasco::response::json(R"({"key":"value","n":[1,2,3]})"); });
    tools.get("/redirect", []() { return fiasco::response::redirect("/v1/primes?start=2&count=5"); });
    tools.get("/error/{code}", [](int code) {
        return fiasco::response::error("requested error " + std::to_string(code), code);
    });
    tools.get("/empty/{code}", [](int code) { return fiasco::response::empty(code); });
    tools.get("/headers", []() {
        auto r = fiasco::response::text("check the headers");
        // r.status = 203;
        r.headers["X-Stress-Test"] = "fiasco";
        return r;
    });

    // every supported path-param type at once
    tools.get("/types/{i}/{lg}/{ll}/{f}/{d}/{b}/{s}",
              [](std::string s, bool b, double d, float f, long long ll, long lg, int i) {
                  return fiasco::json{{"int", i},   {"long", lg}, {"long_long", ll}, {"float", f},
                                      {"double", d}, {"bool", b},  {"string", s}};
              });

    // recursive fiasco::json building
    tools.get("/nest/{depth}", [](int depth) { return nest(std::clamp(depth, 0, 64)); });

    // concurrency probes: fire several at once and watch thread ids / timing
    tools.get("/sleep/{ms}", [](int ms) {
        ms = std::clamp(ms, 0, 5000);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return fiasco::json{{"slept_ms", ms}};
    });
    tools.get("/cpu/{limit}", [](int limit) {
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
        long long count = std::count(sieve.begin(), sieve.end(), true);
        return fiasco::json{{"limit", limit}, {"primes_below_or_equal", count}};
    });
    tools.get("/whoami", []() {
        auto h = std::hash<std::thread::id>{}(std::this_thread::get_id());
        return fiasco::json{{"thread_hash", std::to_string(h)}};
    });

    app.include_router(std::move(v1));
    app.include_router(std::move(tools));
    app.run();
}

// =============================================================================
// SAMPLE REQUESTS
// =============================================================================
//
// Happy path (all features):
//   curl -X POST 'localhost:8080/v1/analyze/7/3/0.1/true?page=3&verbose=true&q=hi' \
//     -H 'content-type: application/json' -d '{
//       "req": {
//         "label": "aabbbcc",
//         "user-name": "ada",
//         "weights": [1, 2, 3],
//         "offsets": {"a": 1.5, "b": -2.0},
//         "window": [2, 9],
//         "meta": [7, "x", true],
//         "regions": [{"x": 5, "y": 5, "note": null}, {"x": 1, "y": 2}],
//         "focus": null,
//         "filters": {"min_degree": 1, "matrix_scale": 2.5, "tags": ["z", "a", "m"]}
//       },
//       "tuning": {"percent": 50, "boxes": 9, "damping": 1.5, "code": "abc", "matrix_n": 5}
//     }'
//
// Should 422:
//   - "focus" missing                      (required + nullable)
//   - "label": ""                          (min_length)
//   - "tuning.percent": 0 or 101           (gt / le)
//   - "tuning.boxes": 4                    (multiple_of 3)
//   - "tuning.matrix_n": 9                 (le 8)
//   - "regions": [{"x": -1, "y": 0}]       (ge on nested element)
//   - "weights" with 17 items              (max_length)
//   - "username" instead of "user-name"    (alias; falls back to default)
//   - ?page=abc, missing ?q, {strict}=maybe
//   - body with only "req" (missing "tuning" embedded model)
//
// Edge seeds: 0, -7, 2147483647, 7 (prime => secret)
// Overflow:   /v1/analyze/2147483647/9223372036854775807/1.0/false  => combined.overflow=true
//
// Items:
//   curl -X PUT 'localhost:8080/v1/items/1?dry_run=false' -H 'content-type: application/json' \
//     -d '{"sku":"ab-1","unit-price":9.99,"quantity":3,"pack_size":12,"tags":["x","a"],
//          "discount":0.1,"notes":null}'
//
// Concurrency (4 threads => 4 parallel sleeps finish in ~1s, not 4s):
//   for i in 1 2 3 4; do curl -s localhost:8080/tools/sleep/1000 & done; wait
//   for i in 1 2 3 4 5 6; do curl -s localhost:8080/tools/whoami; done
//   ab -n 2000 -c 64 'localhost:8080/tools/cpu/200000'
