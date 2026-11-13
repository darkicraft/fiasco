// Advanced dependency-graph stress test.
//
// The graph is intentionally large and redundant: many providers depend on the
// same shared nodes, and the handler asks for several roots. With per-request
// caching, every provider runs exactly once. This is a manual diagnostic
// example, not the place to learn dependency injection.
//
//   curl localhost:8080/graph/reset
//   curl 'localhost:8080/graph?limit=25' -H 'X-Trace: abc' -H 'X-Tenant: acme'
//   # calls should equal graph_nodes
//
//   curl localhost:8080/graph/reset
//   curl -X POST localhost:8080/graph/body \
//     -H 'Content-Type: application/json' \
//     -d '{"request":{"seed":2},"graph":{"seed":3}}'
//   # calls should equal body_graph_nodes

#include <fiasco/fiasco.hpp>

#include <atomic>
#include <optional>
#include <string>

using fiasco::body;
using fiasco::depends;
using fiasco::get;
using fiasco::header;
using fiasco::json;
using fiasco::post;
using fiasco::query;

namespace {

inline constexpr int graph_nodes = 19;
inline constexpr int body_graph_nodes = 20;

std::atomic<int> g_calls{0};

auto count_call() -> void {
    g_calls.fetch_add(1, std::memory_order_relaxed);
}

auto graph_calls() -> int {
    return g_calls.load(std::memory_order_relaxed);
}

auto reset_graph() -> void {
    g_calls.store(0, std::memory_order_relaxed);
}

struct request_id {
    std::string value;
};

struct tenant {
    std::string value;
};

struct config {
    int max_items = 100;
    int version = 1;
};

struct flags {
    bool audit = true;
    bool beta = true;
};

struct db_session {
    db_session() = default;
    db_session(db_session&&) noexcept = default;
    db_session& operator=(db_session&&) noexcept = default;
    db_session(const db_session&) = delete;
    db_session& operator=(const db_session&) = delete;
    ~db_session() = default;
};

struct cache_client {
    std::string name = "cache";
};

struct metrics {
    int base = 1;
};

struct auth {
    bool ok = false;
};

struct user {
    std::string name;
};

struct permissions {
    bool read = false;
    bool write = false;
};

struct rate_limit {
    int remaining = 0;
};

struct trace {
    std::string id;
    int depth = 0;
};

struct quota {
    int limit = 0;
};

struct audit {
    int entries = 0;
};

struct feature {
    bool enabled = false;
};

struct authorization {
    bool allowed = false;
};

struct policy {
    int version = 0;
};

struct request_context {
    std::string id;
    int depth = 0;
};

struct payload {
    int seed = 1;
};

auto get_request_id(const fiasco::request& request,
                    std::optional<std::string> trace [[=header{.alias = "X-Trace"}]])
    -> request_id {
    count_call();
    return request_id{trace.value_or(request.client_address)};
}

auto get_tenant(std::optional<std::string> value [[=header{.alias = "X-Tenant"}]]) -> tenant {
    count_call();
    return tenant{value.value_or("public")};
}

auto get_config() -> config {
    count_call();
    return config{};
}

auto get_flags() -> flags {
    count_call();
    return flags{};
}

auto get_db(const fiasco::request& request) -> db_session {
    (void)request;
    count_call();
    return db_session{};
}

auto get_cache() -> cache_client {
    count_call();
    return cache_client{};
}

auto get_metrics() -> metrics {
    count_call();
    return metrics{};
}

auto get_limit(std::optional<int> limit [[=query{.ge = 1, .alias = "limit"}]]) -> int {
    count_call();
    return limit.value_or(10);
}

auto get_auth(const fiasco::request& request,
              request_id& id [[=depends{^^get_request_id}]],
              tenant& current_tenant [[=depends{^^get_tenant}]],
              std::optional<std::string> authorization [[=header{.alias = "Authorization"}]])
    -> auth {
    (void)request;
    (void)id;
    (void)current_tenant;
    count_call();
    return auth{authorization.has_value()};
}

auto get_user(auth& current_auth [[=depends{^^get_auth}]],
              config& current_config [[=depends{^^get_config}]],
              request_id& id [[=depends{^^get_request_id}]]) -> user {
    count_call();
    return user{current_auth.ok ? id.value : "anonymous-" + std::to_string(current_config.version)};
}

auto get_permissions(user& current_user [[=depends{^^get_user}]],
                     flags& current_flags [[=depends{^^get_flags}]]) -> permissions {
    count_call();
    return permissions{.read = true,
                       .write = current_user.name != "anonymous-1" && current_flags.audit};
}

auto get_rate_limit(cache_client& cache [[=depends{^^get_cache}]],
                    flags& current_flags [[=depends{^^get_flags}]],
                    int limit [[=depends{^^get_limit}]]) -> rate_limit {
    (void)cache;
    count_call();
    return rate_limit{current_flags.beta ? limit : limit / 2};
}

auto get_trace(metrics& current_metrics [[=depends{^^get_metrics}]],
               request_id& id [[=depends{^^get_request_id}]]) -> trace {
    count_call();
    return trace{id.value, current_metrics.base};
}

auto get_quota(rate_limit& limit [[=depends{^^get_rate_limit}]],
               config& current_config [[=depends{^^get_config}]],
               trace& current_trace [[=depends{^^get_trace}]]) -> quota {
    count_call();
    const int capped =
        limit.remaining < current_config.max_items ? limit.remaining : current_config.max_items;
    return quota{capped + current_trace.depth};
}

auto get_audit(db_session& db [[=depends{^^get_db}]],
               user& current_user [[=depends{^^get_user}]],
               trace& current_trace [[=depends{^^get_trace}]]) -> audit {
    (void)db;
    count_call();
    return audit{static_cast<int>(current_user.name.size() + current_trace.id.size())};
}

auto get_feature(flags& current_flags [[=depends{^^get_flags}]],
                 config& current_config [[=depends{^^get_config}]],
                 permissions& current_permissions [[=depends{^^get_permissions}]]) -> feature {
    count_call();
    return feature{current_flags.beta && current_permissions.read && current_config.version > 0};
}

auto get_authorization(auth& current_auth [[=depends{^^get_auth}]],
                       permissions& current_permissions [[=depends{^^get_permissions}]],
                       quota& current_quota [[=depends{^^get_quota}]],
                       trace& current_trace [[=depends{^^get_trace}]]) -> authorization {
    count_call();
    return authorization{current_auth.ok && current_permissions.write && current_quota.limit > 0 &&
                         current_trace.depth > 0};
}

auto get_policy(feature& current_feature [[=depends{^^get_feature}]],
                quota& current_quota [[=depends{^^get_quota}]],
                config& current_config [[=depends{^^get_config}]]) -> policy {
    count_call();
    return policy{current_config.version + (current_feature.enabled ? 1 : 0) +
                  (current_quota.limit > 0 ? 1 : 0)};
}

auto get_request_context(authorization& current_authorization [[=depends{^^get_authorization}]],
                         policy& current_policy [[=depends{^^get_policy}]],
                         audit& current_audit [[=depends{^^get_audit}]],
                         trace& current_trace [[=depends{^^get_trace}]]) -> request_context {
    (void)current_authorization;
    count_call();
    return request_context{current_trace.id, current_audit.entries + current_policy.version};
}

auto get_payload(payload graph [[=body{.alias = "graph"}]]) -> payload {
    count_call();
    return graph;
}

}  // namespace

namespace routes {

[[=get("/graph/reset")]]
auto graph_reset() -> json {
    reset_graph();
    return json{{"calls", graph_calls()}};
}

[[=get("/graph")]]
auto graph(const request_context& context [[=depends{^^get_request_context}]],
                               const authorization& authorization [[=depends{^^get_authorization}]],
                               const policy& policy [[=depends{^^get_policy}]],
                               const audit& audit [[=depends{^^get_audit}]],
                               const trace& trace [[=depends{^^get_trace}]],
                               const user& user [[=depends{^^get_user}]],
                               const request_id& id [[=depends{^^get_request_id}]],
                               const tenant& tenant [[=depends{^^get_tenant}]],
                               int limit [[=depends{^^get_limit}]]) -> json {
    return json{
        {"calls", graph_calls()},
        {"expected", graph_nodes},
        {"request_id", id.value},
        {"tenant", tenant.value},
        {"user", user.name},
        {"limit", limit},
        {"context", context.id},
        {"depth", context.depth},
        {"allowed", authorization.allowed},
        {"policy", policy.version},
        {"audit_entries", audit.entries},
        {"trace_depth", trace.depth},
    };
}

[[=post("/graph/body")]]
auto graph_body(
    const request_context& context [[=depends{^^get_request_context}]],
    const payload& request [[=body{.alias = "request"}]],
    const payload& graph [[=depends{^^get_payload}]],
    int limit [[=depends{^^get_limit}]]) -> json {
    return json{
        {"calls", graph_calls()},
        {"expected", body_graph_nodes},
        {"sum", request.seed + graph.seed},
        {"limit", limit},
        {"context", context.id},
    };
}

}  // namespace routes

auto main() -> int {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
