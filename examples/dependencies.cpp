// A small end-to-end dependency-injection example.
//
//   1. POST /init with {"username":"alice","password":"wonderland"}
//      -> returns {"username":"alice","token":"token-1","admin":true}
//
//   2. GET /me with Authorization: Bearer token-1
//      -> returns the current user
//
//   3. GET /admin with Authorization: Bearer token-1
//      -> succeeds because the first registered user is an admin
//
//   4. POST /transfer with Authorization: Bearer token-1
//      body: {"to":"bob","amount":10}
//      -> validates the recipient and amount through a body dependency
//
// Dependencies are synchronous, cached per request, and cleaned up with RAII.

#include <fiasco/fiasco.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

struct account {
    std::string username;
    std::string password;
    std::string token;
    bool admin = false;
};

struct auth_state {
    std::mutex mutex;
    std::unordered_map<std::string, account> accounts;
    std::unordered_map<std::string, std::string> tokens;
};

auth_state& auth() {
    static auth_state state;
    return state;
}

std::string next_token() {
    static std::atomic<std::uint64_t> counter{1};
    return "token-" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

struct session {
    explicit session(const fiasco::request& request)
        : client(request.client_address) {}

    std::string client;
};

session get_session(const fiasco::request& request) {
    return session{request};
}

struct user {
    std::string username;
    bool admin = false;
};

struct credentials {
    std::string username [[= fiasco::field{.min_length = 1}]];
    std::string password [[= fiasco::field{.min_length = 1}]];
};

struct transfer_request {
    std::string to;
    int amount = 0;
};

user get_current_user(
    session& request_session [[= fiasco::depends{^^get_session}]],
    std::optional<std::string> authorization
    [[= fiasco::header{.alias = "Authorization"}]]) {
    (void)request_session;

    if (!authorization) {
        throw fiasco::http_error(401,
                                 "Not authenticated",
                                 {{"WWW-Authenticate", "Bearer"}});
    }

    std::string token = *authorization;
    constexpr std::string_view prefix = "Bearer ";
    if (token.starts_with(prefix)) {
        token.erase(0, prefix.size());
    }

    std::lock_guard lock(auth().mutex);
    const auto token_it = auth().tokens.find(token);
    if (token_it == auth().tokens.end()) {
        throw fiasco::http_error(401, "Invalid token");
    }
    const auto account_it = auth().accounts.find(token_it->second);
    if (account_it == auth().accounts.end()) {
        throw fiasco::http_error(401, "Invalid token");
    }

    return user{account_it->second.username, account_it->second.admin};
}

struct admin {
    user value;
};

admin require_admin(const user& current [[= fiasco::depends{^^get_current_user}]]) {
    if (!current.admin) {
        throw fiasco::http_error(403, "Admin access required");
    }
    return admin{current};
}

struct insufficient_funds : std::runtime_error {
    using std::runtime_error::runtime_error;
};

transfer_request validate_transfer(transfer_request request) {
    if (request.amount <= 0) {
        throw insufficient_funds("amount must be greater than zero");
    }

    std::lock_guard lock(auth().mutex);
    if (!auth().accounts.contains(request.to)) {
        throw fiasco::http_error(404, "recipient not found");
    }
    return request;
}

}  // namespace

int main() {
    fiasco::server app;

    app.add_exception_handler([](const insufficient_funds& error) {
        return fiasco::response::error(error.what(), 400);
    });

    // Register a user. The first user is an administrator.
    app.post("/init", [](const credentials& input) {
        std::lock_guard lock(auth().mutex);

        if (auth().accounts.contains(input.username)) {
            throw fiasco::http_error(409, "user already exists");
        }

        const bool admin = auth().accounts.empty();
        const std::string token = next_token();
        auth().accounts.emplace(
            input.username,
            account{input.username, input.password, token, admin});
        auth().tokens.emplace(token, input.username);

        return fiasco::json{
            {"username", input.username},
            {"token", token},
            {"admin", admin},
        };
    });

    // get_current_user is resolved here, and its session dependency is cached.
    app.get("/me",
            [](const user& current [[= fiasco::depends{^^get_current_user}]]) {
                return fiasco::json{
                    {"username", current.username},
                    {"admin", current.admin},
                };
            });

    // require_admin depends on get_current_user. Both handler parameters share
    // the same cached user value.
    app.get("/admin",
            [](const user& current [[= fiasco::depends{^^get_current_user}]],
               const admin& access [[= fiasco::depends{^^require_admin}]]) {
                return fiasco::json{
                    {"current_user", current.username},
                    {"admin_user", access.value.username},
                };
            });

    // The transfer body is owned by a dependency. Because it is the only body
    // parameter in the graph, the whole request body is passed to it.
    app.post("/transfer",
             [](const transfer_request& transfer
                    [[= fiasco::depends{^^validate_transfer}]],
                const user& current [[= fiasco::depends{^^get_current_user}]]) {
                 return fiasco::json{
                     {"from", current.username},
                     {"to", transfer.to},
                     {"amount", transfer.amount},
                 };
             });

    app.get("/health", []() -> std::string { return "ok"; });

    app.run();
}
