// Synchronous and asynchronous handlers.
//
//   cmake --build build --target async_handlers
//   ./build/examples/async/basic
//
//   curl localhost:8080/sync
//   curl localhost:8080/async
//   curl localhost:8080/async/typed
//   curl -i localhost:8080/async/custom
//
// Use `fiasco::async` for an untyped coroutine and `fiasco::async_t<T>` when
// the response model should be inferred for OpenAPI.

#include <fiasco/fiasco.hpp>

#include <string>

using fiasco::get;
using fiasco::response;

namespace {

struct user {
    std::string name;
    int age;
};

auto load_user() -> user {
    return {"ada", 36};
}

}  // namespace

namespace routes {

[[=get("/sync")]]
auto sync() -> user {
    return load_user();
}

[[=get("/async")]]
auto untyped() -> fiasco::async {
    co_return load_user();
}

[[=get("/async/typed")]]
auto typed() -> fiasco::async_t<user> {
    co_return load_user();
}

[[=get("/async/custom")]]
auto custom() -> fiasco::async {
    auto res = response::json(R"({"created":true})");
    res.status_code = 201;
    res.headers["X-Async"] = "yes";
    co_return res;
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
