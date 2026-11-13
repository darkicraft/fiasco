// Basic dependency injection with per-request caching and RAII cleanup.
//
//   cmake --build build --target dependencies
//   ./build/examples/dependencies
//
//   curl localhost:8080/me -H 'Authorization: Bearer ada'
//   curl localhost:8080/me
//
// get_current_user depends on get_db. /me also asks for get_db directly, but
// the provider runs once per request; both parameters share the cached value.
// db_session is move-only and lives until the handler returns.

#include <fiasco/fiasco.hpp>

#include <optional>
#include <string>
#include <utility>

using fiasco::depends;
using fiasco::get;
using fiasco::header;
using fiasco::json;

namespace {

struct db_session {
    explicit db_session(const fiasco::request& request)
        : client(request.client_address) {}

    db_session(db_session&&) noexcept = default;
    db_session& operator=(db_session&&) noexcept = default;
    db_session(const db_session&) = delete;
    db_session& operator=(const db_session&) = delete;
    ~db_session() = default;

    std::string client;
};

struct user {
    std::string name;
};

auto get_db(const fiasco::request& request) {
    return db_session{request};
}

auto get_current_user(
    db_session& db [[=depends{^^get_db}]],
    std::optional<std::string> authorization [[=header{.alias = "Authorization"}]]) {
    return user{authorization.value_or("anonymous@" + db.client)};
}

}  // namespace

namespace routes {

[[=get("/me")]]
auto me(db_session& db [[=depends{^^get_db}]],
                         user& current [[=depends{^^get_current_user}]]) -> json {
    return json{
        {"user", current.name},
        {"client", db.client},
    };
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
