// Basic dependency injection with per-request caching and RAII cleanup.
//
//   cmake --build build --target dependencies
//   FIASCO_PORT=8080 ./build/examples/dependencies
//
//   curl localhost:8080/me -H 'Authorization: Bearer ada'
//   curl localhost:8080/me
//
// get_current_user depends on get_db. /me also asks for get_db directly, but
// the provider runs once per request; both parameters share the cached value.
// db_session is move-only and lives until the handler returns.

#include <fiasco/fiasco.hpp>

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

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

db_session get_db(const fiasco::request& request) {
    return db_session{request};
}

user get_current_user(
    db_session& db [[= fiasco::depends{^^get_db}]],
    std::optional<std::string> authorization
    [[= fiasco::header{.alias = "Authorization"}]]) {
    return user{authorization.value_or("anonymous@" + db.client)};
}

}  // namespace

int main() {
    fiasco::server app{
        fiasco::server_config{.network = {.port = example_port()}},
    };

    app.get("/me",
            [](db_session& db [[= fiasco::depends{^^get_db}]],
               user& current [[= fiasco::depends{^^get_current_user}]]) {
                return fiasco::json{
                    {"user", current.name},
                    {"client", db.client},
                };
            });

    app.run();
}
