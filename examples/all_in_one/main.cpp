// All-in-one demo server. This is the binary used by demo/demo.sh.
//
//   cmake --build build --target all_in_one
//   ./build/examples/all_in_one/main
//
// It intentionally combines routing, typed parameters, reflected models,
// dependencies, validation, middleware, CORS, response helpers, cookies,
// static files, streamed files, SSE, JSON Lines, typed async handlers, custom
// exception handling, sub-routers and OpenAPI in one application.

#include <asio/deferred.hpp>
#include <asio/steady_timer.hpp>
#include <asio/this_coro.hpp>

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>

#include "fiasco/fiasco.hpp"

using fiasco::depends;
using fiasco::field;
using fiasco::get;
using fiasco::header;
using fiasco::json;
using fiasco::path;
using fiasco::post;
using fiasco::query;
using fiasco::response;

namespace demo {

struct item {
    long id = 0;
    std::string name;
    int quantity = 1;
};

struct item_input {
    std::string name;
    int quantity [[=field{.ge = 1, .le = 100}]] = 1;
};

struct demo_error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

int request_marker(const fiasco::request& request) {
    return static_cast<int>(request.path.size());
}

std::string current_user(
    std::optional<std::string> authorization [[=header{.alias = "Authorization"}]]) {
    if (!authorization) {
        return "anonymous";
    }
    if (authorization->starts_with("Bearer ")) {
        return authorization->substr(7);
    }
    return "unrecognized";
}

auto event_stream() -> fiasco::stream<fiasco::sse_event> {
    co_yield fiasco::sse_event{.data = "all-in-one", .event = "welcome"};

    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_after(std::chrono::milliseconds(50));
    co_await timer.async_wait(asio::deferred);

    co_yield fiasco::sse_event{.data = "1", .event = "tick"};
    co_yield fiasco::sse_event{.data = "2", .event = "tick"};
    co_return;
}

auto json_line_stream() -> fiasco::stream<item> {
    co_yield item{1, "one", 1};
    co_yield item{2, "two", 2};
    co_return;
}

[[=get("/")]]
auto index() -> std::string {
    return "Fiasco all-in-one demo. Try /items/42, /events, /static/, /docs.";
}

[[=get("/hello/{name}")]]
auto hello(std::string name) -> std::string {
    return "hello " + name;
}

[[=get("/items/{id}")]]
auto read_item(long id [[=path{.ge = 1}]]) -> item {
    return item{id, "demo-item", 3};
}

[[=post("/items")]]
auto create_item(item_input input) -> item {
    return item{42, input.name, input.quantity};
}

[[=get("/search")]]
auto search(std::optional<std::string> q [[=query{}]],
            int limit [[=query{.ge = 1, .le = 100}]] = 10) -> json {
    return json{
        {"query", q.value_or("*")},
        {"limit", limit},
    };
}

[[=get("/me")]]
auto me(int marker [[=depends{^^request_marker}]],
        std::string user [[=depends{^^current_user}]]) -> json {
    return json{
        {"user", user},
        {"marker", marker},
    };
}

[[=get("/async")]]
auto async_item() -> fiasco::async {
    co_return item{1, "async", 1};
}

[[=get("/async/typed")]]
auto typed_async_item() -> fiasco::async_t<item> {
    co_return item{2, "typed-async", 2};
}

[[=get("/error")]]
auto error_route() -> void {
    throw demo_error("demo exception");
}

[[=get("/response")]]
auto response_route() -> response {
    auto res = response::json(R"({"factory":"response"})");
    res.headers["X-Demo"] = "yes";
    return res;
}

[[=get("/cookie")]]
auto cookie_route() -> response {
    auto res = response::text("cookie set");
    res.set_cookie("session", "demo-session", {.httponly = true, .samesite = "lax"});
    return res;
}

[[=get("/download")]]
auto download() -> response {
    return response::file("examples/responses/download.txt", {.filename = "fiasco-demo.txt"});
}

[[=get("/events")]]
auto events() -> response {
    return fiasco::sse(event_stream());
}

[[=get("/lines")]]
auto lines() -> response {
    return fiasco::json_lines(json_line_stream());
}

}  // namespace demo

namespace admin {

[[=get("/health")]]
auto health() -> json {
    return json{{"status", "healthy"}};
}

}  // namespace admin

int main() {
    fiasco::app app{fiasco::app_options{
        .title = "Fiasco All-in-One",
        .version = "0.1.0",
        .description = "Every major Fiasco feature in one demo application.",
    }};

    app.middleware([](fiasco::request& request, const fiasco::next& next) -> fiasco::async {
        auto response = co_await next(request);
        response.headers["X-Demo"] = "all-in-one";
        co_return response;
    });

    app.middleware(fiasco::middleware::cors({
        .allow_origins = {"*"},
        .allow_methods = {"GET", "POST", "OPTIONS"},
        .allow_headers = {"Content-Type", "Authorization", "X-Demo"},
        .expose_headers = {"X-Demo"},
        .max_age = 600,
    }));

    app.add_exception_handler(
        [](const demo::demo_error& error) { return response::error(error.what(), 409); });

    app.register_api<^^demo>();

    fiasco::router admin_router("/admin");
    admin_router.register_api<^^admin>();
    app.include_router(std::move(admin_router));

    app.mount_static("/static", "examples/static/public", {.html = true});
    app.print_routes();
    app.run();
}
