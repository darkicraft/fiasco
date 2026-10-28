// A dependency-graph stress test.
//
// The graph is intentionally large and redundant: many providers depend on the
// same shared nodes, and the handler asks for several roots. With per-request
// caching, every provider runs exactly once.
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
#include <cstdint>
#include <optional>
#include <string>

namespace {

inline constexpr int graph_nodes = 19;
inline constexpr int body_graph_nodes = 20;

std::atomic<int> g_calls{0};

void count_call() {
    g_calls.fetch_add(1, std::memory_order_relaxed);
}

int graph_calls() {
    return g_calls.load(std::memory_order_relaxed);
}

void reset_graph() {
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

request_id get_request_id(
    const fiasco::request& request,
    std::optional<std::string> trace [[= fiasco::header{.alias = "X-Trace"}]]) {
    count_call();
    return request_id{trace.value_or(request.client_address)};
}

tenant get_tenant(std::optional<std::string> value
                  [[= fiasco::header{.alias = "X-Tenant"}]]) {
    count_call();
    return tenant{value.value_or("public")};
}

config get_config() {
    count_call();
    return config{};
}

flags get_flags() {
    count_call();
    return flags{};
}

db_session get_db(const fiasco::request& request) {
    (void)request;
    count_call();
    return db_session{};
}

cache_client get_cache() {
    count_call();
    return cache_client{};
}

metrics get_metrics() {
    count_call();
    return metrics{};
}

int get_limit(std::optional<int> limit [[= fiasco::query{.ge = 1, .alias = "limit"}]]) {
    count_call();
    return limit.value_or(10);
}

auth get_auth(
    const fiasco::request& request,
    request_id& id [[= fiasco::depends{^^get_request_id}]],
    tenant& current_tenant [[= fiasco::depends{^^get_tenant}]],
    std::optional<std::string> authorization
    [[= fiasco::header{.alias = "Authorization"}]]) {
    (void)request;
    (void)id;
    (void)current_tenant;
    count_call();
    return auth{authorization.has_value()};
}

user get_user(
    auth& current_auth [[= fiasco::depends{^^get_auth}]],
    config& current_config [[= fiasco::depends{^^get_config}]],
    request_id& id [[= fiasco::depends{^^get_request_id}]]) {
    count_call();
    return user{current_auth.ok ? id.value : "anonymous-" + std::to_string(current_config.version)};
}

permissions get_permissions(
    user& current_user [[= fiasco::depends{^^get_user}]],
    flags& current_flags [[= fiasco::depends{^^get_flags}]]) {
    count_call();
    return permissions{.read = true, .write = current_user.name != "anonymous-1" && current_flags.audit};
}

rate_limit get_rate_limit(
    cache_client& cache [[= fiasco::depends{^^get_cache}]],
    flags& current_flags [[= fiasco::depends{^^get_flags}]],
    int limit [[= fiasco::depends{^^get_limit}]]) {
    (void)cache;
    count_call();
    return rate_limit{current_flags.beta ? limit : limit / 2};
}

trace get_trace(
    metrics& current_metrics [[= fiasco::depends{^^get_metrics}]],
    request_id& id [[= fiasco::depends{^^get_request_id}]]) {
    count_call();
    return trace{id.value, current_metrics.base};
}

quota get_quota(
    rate_limit& limit [[= fiasco::depends{^^get_rate_limit}]],
    config& current_config [[= fiasco::depends{^^get_config}]],
    trace& current_trace [[= fiasco::depends{^^get_trace}]]) {
    count_call();
    const int capped = limit.remaining < current_config.max_items ? limit.remaining
                                                                  : current_config.max_items;
    return quota{capped + current_trace.depth};
}

audit get_audit(
    db_session& db [[= fiasco::depends{^^get_db}]],
    user& current_user [[= fiasco::depends{^^get_user}]],
    trace& current_trace [[= fiasco::depends{^^get_trace}]]) {
    (void)db;
    count_call();
    return audit{static_cast<int>(current_user.name.size() + current_trace.id.size())};
}

feature get_feature(
    flags& current_flags [[= fiasco::depends{^^get_flags}]],
    config& current_config [[= fiasco::depends{^^get_config}]],
    permissions& current_permissions [[= fiasco::depends{^^get_permissions}]]) {
    count_call();
    return feature{current_flags.beta && current_permissions.read && current_config.version > 0};
}

authorization get_authorization(
    auth& current_auth [[= fiasco::depends{^^get_auth}]],
    permissions& current_permissions [[= fiasco::depends{^^get_permissions}]],
    quota& current_quota [[= fiasco::depends{^^get_quota}]],
    trace& current_trace [[= fiasco::depends{^^get_trace}]]) {
    count_call();
    return authorization{current_auth.ok && current_permissions.write && current_quota.limit > 0 &&
                         current_trace.depth > 0};
}

policy get_policy(
    feature& current_feature [[= fiasco::depends{^^get_feature}]],
    quota& current_quota [[= fiasco::depends{^^get_quota}]],
    config& current_config [[= fiasco::depends{^^get_config}]]) {
    count_call();
    return policy{current_config.version + (current_feature.enabled ? 1 : 0) +
                  (current_quota.limit > 0 ? 1 : 0)};
}

request_context get_request_context(
    authorization& current_authorization [[= fiasco::depends{^^get_authorization}]],
    policy& current_policy [[= fiasco::depends{^^get_policy}]],
    audit& current_audit [[= fiasco::depends{^^get_audit}]],
    trace& current_trace [[= fiasco::depends{^^get_trace}]]) {
    (void)current_authorization;
    count_call();
    return request_context{current_trace.id, current_audit.entries + current_policy.version};
}

payload get_payload(payload graph [[= fiasco::body{.alias = "graph"}]]) {
    count_call();
    return graph;
}

}  // namespace

int main() {
    fiasco::server app;

    app.get("/graph/reset", []() {
        reset_graph();
        return fiasco::json{{"calls", graph_calls()}};
    });

    app.get("/graph", [](const request_context& context
                         [[= fiasco::depends{^^get_request_context}]],
                         const authorization& authorization
                         [[= fiasco::depends{^^get_authorization}]],
                         const policy& policy [[= fiasco::depends{^^get_policy}]],
                         const audit& audit [[= fiasco::depends{^^get_audit}]],
                         const trace& trace [[= fiasco::depends{^^get_trace}]],
                         const user& user [[= fiasco::depends{^^get_user}]],
                         const request_id& id [[= fiasco::depends{^^get_request_id}]],
                         const tenant& tenant [[= fiasco::depends{^^get_tenant}]],
                         int limit [[= fiasco::depends{^^get_limit}]]) {
        return fiasco::json{
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
    });

    app.post("/graph/body",
             [](const request_context& context
                    [[= fiasco::depends{^^get_request_context}]],
                const payload& request [[= fiasco::body{.alias = "request"}]],
                const payload& graph [[= fiasco::depends{^^get_payload}]],
                int limit [[= fiasco::depends{^^get_limit}]]) {
                 return fiasco::json{
                     {"calls", graph_calls()},
                     {"expected", body_graph_nodes},
                     {"sum", request.seed + graph.seed},
                     {"limit", limit},
                     {"context", context.id},
                 };
             });

    app.run();
}
