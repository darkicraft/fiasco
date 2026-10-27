# Fiasco

An expressive C++26 web framework that brings FastAPI-like ergonomics to C++.

> **Status**: Alpha: the core API is functional, but expect breaking changes as features mature.

## Motivation

Existing C++ web frameworks are either syntactically painful (Drogon's verbosity), outdated, poorly documented, or not attractive enough to adopt. Fiasco aims to prove that C++ can be just as enjoyable as Python or Rust for building HTTP services, with the performance you expect from native code.

## Requirements

- **CMake** ≥ 3.30
- **GCC ≥ 16** (C++26)

> Only GCC is supported for now: fiasco relies on C++26 reflection (`std::meta`),
> which currently requires GCC 16's `-freflection`.

## Build

```shell
git clone https://github.com/darkicraft/fiasco
cd fiasco
cmake -S . -B build

# Optional: skip tests
# cmake -S . -B build -DFIASCO_BUILD_TESTS=OFF

# Optional: skip examples
# cmake -S . -B build -DFIASCO_BUILD_EXAMPLES=OFF

# Optional: enable C++20 module support (experimental)
# cmake -S . -B build -DFIASCO_BUILD_MODULES=ON

cmake --build build

# Run the test suite
ctest --test-dir build

# Optional: install
cmake --install build
```

## Features

| Feature | Status |
|---------|--------|
| HTTP/1.1 (GET, POST, PUT, PATCH, DELETE) | ✅ |
| Path parameters bound **by name** with type conversion | ✅ |
| Query parameters bound **by name** with type conversion | ✅ |
| JSON request/response models (reflection, no macros) | ✅ |
| Multiple body parameters, auto-embedded by name | ✅ |
| Nested models, `vector`/`map`/`pair`/`tuple`/`optional` fields | ✅ |
| Exact Pydantic required-vs-nullable rules | ✅ |
| `[[= field{...}]]` constraints (`gt`/`ge`/`lt`/`le`/`multiple_of`, `min_length`/`max_length`, `alias`) | ✅ |
| FastAPI-compatible `422` payloads (`{"detail": ...}`, `type`/`loc`/`msg`/`input`/`ctx`) | ✅ |
| Type-erased JSON wrapper | ✅ |
| Generic arithmetic parsing, including fixed-width `std::int*_t` / `std::uint*_t` | ✅ |
| Declarative per-server configuration (`server_config`) | ✅ |
| Per-server logging with configurable sinks | ✅ |
| Sub-router mounting with path prefixes | ✅ |
| Multi-threaded Asio-based async I/O | ✅ |
| Keep-alive, `Connection: close`, and malformed-request handling | ✅ |
| `SIGINT`/`SIGTERM` graceful shutdown | ✅ |
| C++20 modules (experimental, opt-in) | ✅ |
| Explicit parameter sources (`[[= query{}]]`, `[[= path{}]]`, `[[= body{}]]`, `[[= header{}]]`, `[[= cookie{}]]`) | ✅ |
| Synchronous dependency injection with per-request caching (`[[= depends{^^fn}]]`) | ✅ |
| `http_error` and custom exception handlers | ✅ |
| Coroutine/async handlers | 🚧 Planned |
| Automatic API docs generation (OpenAPI / Swagger) | 🚧 Planned |
| Static file serving | 🚧 Planned |
| HTTPS / TLS | 🚧 Planned |
| WebSocket | 🚧 Planned |

## Quick Start

```cpp
#include <fiasco/fiasco.hpp>

int main() {
    fiasco::server app;

    app.get("/hello", []() {
        return "Hello, World!";                 // text/plain
    });

    app.get("/json", []() {
        return fiasco::json{{"message", "Hello, World!"}};  // application/json
    });

    app.run();
}
```

## How requests are bound

Handler arguments are bound **by name** using C++26 reflection, so the order of
the parameters does not matter. A scalar parameter is resolved as a **path**
parameter if the route captured its name, otherwise as a **query** parameter. A
non-scalar JSON parameter is the **request body**.

```cpp
// POST /users/{user_id}?notify=true   body: {"name": "Ada"}
app.post("/users/{user_id}",
         [](bool notify, int user_id, CreateUser body) {
             // user_id <- path, notify <- query, body <- JSON body
             return body;
         });
```

## Path Parameters

Bound by name; argument order doesn't matter.

```cpp
app.post("/users/{user_id}/deposit/{amount}", [](double amount, int user_id) {
    // user_id = 42, amount = 100.50 — order-independent
    return fiasco::json{{"status", "ok"}};
});
```

Supported types: any integral or floating-point type, plus `bool` and
`std::string`. That includes the fixed-width `std::int*_t` / `std::uint*_t`
types. Platform-width types such as `long` use their native width. A value that
cannot be converted produces a `422` `*_parsing` error
(`loc: ["path", "<name>"]`). Empty path parameter segments are rejected.

## Query Parameters

Scalar arguments whose name is **not** captured by the path are read from the
query string, also by name.

```cpp
app.get("/search", [](std::string q, int page) {
    // GET /search?q=cats&page=2
    return fiasco::json{{"q", q}, {"page", page}};
});
```

Query names and values are percent-decoded, `+` is treated as a space, and the
first value wins for duplicate keys. A missing required parameter is a `422`
`missing` error;
`{"detail":[{"type":"missing","loc":["query","q"],"msg":"Field required",...}]}`.

## Explicit Parameter Sources

Inference is convenient, but a parameter can declare exactly where it comes
from. Markers use the same constraint syntax as model fields, and `alias`
changes the wire name:

```cpp
using fiasco::body;
using fiasco::cookie;
using fiasco::header;
using fiasco::path;
using fiasco::query;

app.get("/items/{id}",
        [](int id [[= path{.ge = 1}]],
           std::string q [[= query{.min_length = 2, .alias = "search"}]],
           std::string token [[= header{.alias = "X-Token"}]],
           std::string session [[= cookie{.alias = "session_id"}]]) {
            // GET /items/7?search=cats
            // Header: x-token: abc
            // Cookie: session_id=xyz
            return fiasco::json{{"id", id}, {"q", q},
                                {"token", token}, {"session", session}};
        });
```

Supported sources:

- `path` and `query` bind scalar parameters, exactly like inference but without
  guessing.
- `body` binds a scalar or a model from the JSON body. Multiple body parameters
  are embedded under their `alias` (or parameter name), as usual.
- `header` looks up a request header. Header names are case-insensitive.
- `cookie` reads a cookie from the request's `Cookie` header.

Scalar sources accept `std::optional<T>`; an absent value becomes
`std::nullopt` instead of a `missing` error.

`form` and `file` markers exist for the future multipart API, but currently
fail at compile time rather than silently misbehaving.

## Dependencies

A parameter can be supplied by another function with `depends{^^fn}`. The
dependency is resolved before the handler runs, and its parameters use the same
binding rules as handler parameters:

```cpp
struct DbSession {
    explicit DbSession(const fiasco::request&);
    ~DbSession();                       // cleanup after the handler
    DbSession(DbSession&&) = default;
    DbSession& operator=(DbSession&&) = default;
};

DbSession get_db(const fiasco::request& req) {
    return DbSession{req};
}

struct User {
    std::string name;
};

User get_current_user(
    DbSession& db [[= fiasco::depends{^^get_db}]],
    std::string token [[= fiasco::header{.alias = "Authorization"}]]) {
    return User{token};
}

app.get("/me",
        [](User user [[= fiasco::depends{^^get_current_user}]]) {
            return user;
        });
```

Dependencies are cached per request. If both the handler and another
dependency request `get_db`, it runs once and the same value is reused.

Provider results are cached by value, so take resources by reference:

```cpp
[](DbSession& db [[= depends{^^get_db}]]) { ... }
```

RAII is the replacement for FastAPI's `yield` dependencies: the value lives
until the handler returns, then its destructor performs cleanup. Dependencies
are synchronous for now; async providers, body parameters inside dependencies,
and runtime captured state are deferred.

## Request Bodies

Any aggregate `struct` is a model automatically. fiasco (de)serialises it
member-wise via reflection — no macros, no boilerplate.

```cpp
struct CreateUser {
    std::string username;
    int         age;
};

app.post("/users", [](CreateUser body) {
    return CreateUser{body.username, body.age + 1};   // a model response
});
```

When a handler takes **more than one** body model, each is embedded under its
parameter name (exactly like FastAPI):

```cpp
struct Payment { std::string card; int cents; };

app.post("/orders/{order_id}", [](std::string order_id, CreateUser buyer, Payment pay) {
    // order_id <- path
    // body: {"buyer": {...}, "pay": {...}}
    return fiasco::json{{"order_id", order_id}, {"paid", pay.cents}};
});
```

### Supported field types

- Primitive types (`int`, `double`, `bool`, `std::string`, …)
- Nested aggregate structs (also serialised by reflection)
- `std::vector<T>` and other sequence containers
- `std::map<std::string, T>` and other map containers
- `std::optional<T>` (a JSON `null` becomes `std::nullopt`)
- `std::pair<T, U>`, `std::tuple<…>`

Any aggregate `struct` is a model automatically; JSON keys are the member
identifiers, and unnamed members (e.g. anonymous unions) are skipped.

### Required vs. nullable (exactly like Pydantic)

The two are independent:

- a member is **required** iff it has **no default member initializer**;
- `std::optional<T>` controls **nullability only** (whether `null` is accepted).

```cpp
struct M {
    int a;                               // required, not nullable
    int b = 0;                           // optional (default 0)
    std::optional<int> c;                // required, nullable
    std::optional<int> d = std::nullopt; // optional, nullable
};
```

Absent + required ⇒ `missing`; absent + default ⇒ the default is used (and not
validated); `null` on a non-nullable field is a type error.

### Field constraints

Attach Pydantic-style constraints with an annotation:

```cpp
using fiasco::field;

struct Item {
    int percent [[= field{.gt = 0, .le = 100}]];
    int boxes [[= field{.multiple_of = 3}]];
    std::string code [[= field{.min_length = 2}]];
    std::vector<int> tags [[= field{.min_length = 1}]];
    std::string username [[= field{.alias = "user-name"}]];
};
```

Supported now: `gt`, `ge`, `lt`, `le`, `multiple_of`, `min_length`, `max_length`,
`alias`. `alias` changes the JSON key used for both reading and writing.

Misuse (e.g. `.gt` on a string, a fractional bound on an `int`, or two `field`
annotations on one member) is a **compile-time error** with a clear message.

## Validation Errors

Every failure produces FastAPI's `422` envelope. Errors are **collected**, not
failed fast, and nested locations are reported with their full path:

```json
{
  "detail": [
    {"type": "greater_than", "loc": ["body", "age"],
     "msg": "Input should be greater than 0", "input": -5, "ctx": {"gt": 0}},
    {"type": "string_too_short", "loc": ["body", "home", "street"],
     "msg": "String should have at least 2 characters", "input": "", "ctx": {"min_length": 2}},
    {"type": "missing", "loc": ["body", "home", "zip-code"],
     "msg": "Field required", "input": {"street": "", "number": 1}}
  ]
}
```

Notes:

- `loc` starts with `"body"`, `"path"`, `"query"`, `"header"`, or `"cookie"`
  (as in FastAPI).
- `input` is the **raw** value as received (the string from the URL for path/query,
  the JSON value for a body).
- `ctx` carries the constraint parameters and is omitted when Pydantic has none.

## Errors and Exception Handlers

Throw `fiasco::http_error` from a handler or dependency to return an HTTP error
directly:

```cpp
throw fiasco::http_error(
    401,
    "Not authenticated",
    {{"WWW-Authenticate", "Bearer"}});
```

This produces `{"detail": "Not authenticated"}` with status `401` and the
supplied headers.

Domain exceptions can be translated with a custom handler:

```cpp
struct Conflict : std::runtime_error {
    using std::runtime_error::runtime_error;
};

app.add_exception_handler<Conflict>(
    [](const Conflict& error) {
        return fiasco::response::error(error.what(), 409);
    });
```

Handlers match the exact dynamic exception type. `request_validation_error`
still maps to `422`, `json::exception` to `422`, and unhandled exceptions to
`500`.

## Response Types

Every handler returns a `fiasco::response`. Returning a plain value (string, int,
or a model struct) wraps it automatically. For full control, use the factories:

```cpp
app.get("/text", []() { return fiasco::response::text("plain text"); });
app.get("/html", []() { return fiasco::response::html("<h1>Hello</h1>"); });
app.get("/json", []() { return fiasco::response::json(R"({"key":"value"})"); });
app.get("/redirect", []() { return fiasco::response::redirect("/new-location"); });
app.get("/error", []() { return fiasco::response::error("something went wrong", 400); });
app.get("/empty", []() { return fiasco::response::empty(204); });
```

- `std::string` / `const char*` → `text/plain`
- a numeric primitive → its text form
- a model struct or `fiasco::json` → `application/json`
- a `void` handler → `204 No Content`

Status codes and headers can be set directly on the response struct after
construction.

## Sub-routers

Mount groups of routes under a prefix.

```cpp
fiasco::router api("/v1");
api.get("/status", []() { return "ok"; });

fiasco::server app;
app.include_router(std::move(api), "/api");
// GET /api/v1/status now works
```

`app.print_routes()` prints every registered route (useful at start-up).

## Configuration

Server construction is declarative. `server_config` is entirely optional; with
no config, Fiasco uses `0.0.0.0:8080`, automatic worker count, `info` level,
human logging, access logging enabled, and stdout as the sink.

```cpp
fiasco::server app{fiasco::server_config{
    .network = {
        .host = "0.0.0.0",
        .port = 8080,
        .workers = 4,  // 0 = hardware concurrency, minimum 2
    },
    .logging = {
        .enabled = true,
        .access = true,
        .level = fiasco::log_level::info,
        .format = fiasco::log_format::human,
        .colors = true,
        // .sink omitted => stdout
    },
}};

app.run();
```

`server_config` is move-only because the optional sink is move-only. A temporary
config can be passed directly; a named config must be moved:

```cpp
fiasco::server_config config{/* ... */};
fiasco::server app{std::move(config)};
```

Lifecycle helpers:

```cpp
auto actual_port = app.port();  // useful when the config uses port 0
app.wait_until_started();       // when run() is executing on another thread
app.flush_logs();
app.stop();
```

## Logging

Each server owns its own log registry, queue, and consumer thread. Named loggers
are created on demand and share that server's thread:

```cpp
fiasco::server app;
auto& log = app.get_logger("app");
log.info("starting up");
log.warn("careful", {{"key", "value"}});
```

Levels: `debug`, `info`, `warn`, `error`. Access logging (`fiasco.access`) is on
by default.

The logging config controls the level, formatter, colors, access logs, and the
sink. A sink is a move-only callable:

```cpp
using log_sink = std::move_only_function<void(std::string_view)>;
```

Use the provided stream helper or provide your own callback:

```cpp
auto sink = fiasco::make_ostream_log_sink(std::cerr);
config.logging.sink = std::move(sink);
```

```cpp
config.logging.sink = [&](std::string_view line) {
    external_logger.info("{}", line);
};
```

Omitting the sink uses stdout. Set `.logging.enabled = false` to disable all
logging, or `.logging.access = false` to keep application logs but suppress
per-request access lines.

The example executable also accepts `--no-log`, `--log`, `--no-access-log`,
`--access-log`, and `FIASCO_LOG=off`.

## Threading

Workers are configured through `server_config`:

```cpp
fiasco::server app{fiasco::server_config{
    .network = {.workers = 4},
}};
app.run();
```

`workers = 0` uses `std::thread::hardware_concurrency()`, with a minimum of 2.

## Signal Handling

The server catches `SIGINT` and `SIGTERM` internally via Asio's `signal_set`.
Pressing Ctrl+C shuts down all worker threads gracefully and returns from `run()`.

## Dependencies

All fetched automatically at configure time:

- [nlohmann/json](https://github.com/nlohmann/json) post-v3.12.0 commit — JSON core
- [llhttp](https://github.com/nodejs/llhttp) v9.4 — HTTP/1.1 parser
- [Asio](https://github.com/chriskohlhoff/asio) 1.38.2 — async I/O
- [Catch2](https://github.com/catchorg/Catch2) v3.16 (tests only)

## License

MIT — see [LICENSE](LICENSE).
