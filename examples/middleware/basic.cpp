// Application middleware: ordering, request mutation, short-circuiting, and
// response mutation.
//
//   cmake --build build --target middleware
//   ./build/examples/middleware/basic
//
//   curl -i localhost:8080/hello
//   curl -i localhost:8080/whoami
//   curl -i localhost:8080/blocked
//
// Middleware is registered with app.middleware(). The last one added is the
// outermost: request_id wraps guard, so short-circuited responses still get an
// ID.

#include <fiasco/fiasco.hpp>

#include <atomic>
#include <string>

using fiasco::get;
using fiasco::json;

namespace routes {

[[=get("/hello")]]
auto hello() -> std::string {
    return "hello";
}

[[=get("/whoami")]]
auto whoami(const fiasco::request& req) -> json {
    return json{{"request_id", req.header("X-Request-ID")}};
}

}  // namespace routes

int main() {
    fiasco::app app;

    std::atomic<int> next_request_id{1};

    // Inner middleware: short-circuit one path.
    app.middleware([](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
        if (req.path == "/blocked") {
            co_return fiasco::response::error("blocked by middleware", 403);
        }
        co_return co_await next(req);
    });

    // Outer middleware: give every request an ID and echo it back.
    app.middleware(
        [&next_request_id](fiasco::request& req, const fiasco::next& next) -> fiasco::async {
            const std::string id =
                "req-" + std::to_string(next_request_id.fetch_add(1, std::memory_order_relaxed));
            req.headers["X-Request-ID"] = id;

            auto res = co_await next(req);
            res.headers["X-Request-ID"] = id;
            co_return res;
        });

    app.register_api<^^routes>();
    app.run();
}
