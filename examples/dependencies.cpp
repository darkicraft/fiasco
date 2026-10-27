// A focused example of dependency injection, RAII resources, and HTTP errors.
//
//   curl -H 'Authorization: secret' localhost:8080/me
//   curl localhost:8080/me
//   curl localhost:8080/conflict

#include <fiasco/fiasco.hpp>

#include <optional>
#include <stdexcept>
#include <string>

struct session {
    explicit session(const fiasco::request&) {}
    ~session() = default;

    session(session&&) = default;
    session& operator=(session&&) = default;
    session(const session&) = delete;
    session& operator=(const session&) = delete;
};

namespace {

session get_session(const fiasco::request& request) {
    (void)request;
    return session{request};
}

struct user {
    std::string name;
};

user get_current_user(session& db [[= fiasco::depends{^^get_session}]],
                      std::optional<std::string> token
                      [[= fiasco::header{.alias = "Authorization"}]]) {
    (void)db;
    if (!token || *token != "secret") {
        throw fiasco::http_error(401,
                                 "Not authenticated",
                                 {{"WWW-Authenticate", "Bearer"}});
    }
    return user{*token};
}

struct conflict_error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

}  // namespace

int main() {
    fiasco::server app;

    app.add_exception_handler([](const conflict_error& error) {
        return fiasco::response::error(error.what(), 409);
    });

    app.get("/me", [](const user& current [[= fiasco::depends{^^get_current_user}]]) {
        return fiasco::json{{"name", current.name}};
    });

    app.get("/conflict", []() -> std::string {
        throw conflict_error("resource already exists");
    });

    app.get("/health", []() -> std::string { return "ok"; });

    app.run();
}
