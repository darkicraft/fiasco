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
| HTTP/1.1 (GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS) | ✅ |
| Path parameters bound by name with type conversion | ✅ |
| Query parameters bound by name with type conversion | ✅ |
| JSON request/response models (reflection, no macros) | ✅ |
| Multiple body parameters, embedded by name | ✅ |
| FastAPI-compatible error responses (`{"detail": ...}`, 422 validation) | ✅ |
| Type-erased JSON wrapper | ✅ |
| std::optional, vector, map, tuple serialization | ✅ |
| Sub-router mounting with path prefixes | ✅ |
| Multi-threaded Asio-based async I/O | ✅ |
| Keep-alive connections | ✅ |
| SIGINT/SIGTERM graceful shutdown | ✅ |
| C++20 modules (experimental, opt-in) | ✅ |
| Better error handling (custom exception handlers) | 🚧 Planned |
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
        return "Hello, World!";
    });

    app.get("/json", []() {
        return fiasco::json{
            {"message", "Hello, World!"}
        };
    });
}
```

## Path Parameters

Path parameters are matched to handler arguments **by name** (via C++26
reflection), so argument order doesn't matter.

```cpp
app.post("/users/{user_id}/deposit/{amount}", [](double amount, int user_id) {
    // user_id = 42, amount = 100.50 — order-independent
    return fiasco::json{{"status", "ok"}};
});
```

Supported types: `int`, `long`, `long long`, `float`, `double`, `bool`, `std::string`.

## Query Parameters

Scalar arguments whose name is not a path parameter are read from the query
string, also by name.

```cpp
app.get("/search", [](std::string q, int page) {
    // GET /search?q=cats&page=2
    return fiasco::json{{"q", q}, {"page", page}};
});
```

Missing or wrongly-typed parameters produce FastAPI's `422` response, e.g.
`{"detail":[{"type":"missing","loc":["query","q"],"msg":"Field required",...}]}`.

## Request & Response Models

Just define a struct and return it — fiasco (de)serialises it member-wise using
C++26 reflection (`std::meta`). No macros, no boilerplate.

```cpp
struct UserResponse {
    std::string username;
    int age;
};

app.get("/users/{id}", [](int id) {
    return UserResponse{"John Doe", 21};
});
```

Request body models work the same way — mixed freely with path parameters.

```cpp
struct CreateUserRequest {
    std::string username;
    int age;
};

app.post("/users/{department}", [](int department_id, CreateUserRequest req) {
    // department_id from path, req from JSON body
    return fiasco::json{{"created", true}};
});
```

When a handler takes more than one body model, their values are read from a
single JSON object, embedded under each parameter's name.

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
    [[= field{.gt = 0, .le = 100}]]   int percent;
    [[= field{.multiple_of = 3}]]     int boxes;
    [[= field{.min_length = 2}]]      std::string code;
    [[= field{.min_length = 1}]]      std::vector<int> tags;
    [[= field{.alias = "user-name"}]] std::string username;
};
```

Supported now: `gt`, `ge`, `lt`, `le`, `multiple_of`, `min_length`, `max_length`,
`alias`. Violations produce the same 422 `{"detail": [...]}` payloads as
FastAPI/Pydantic (including `ctx`). Misuse (e.g. `.gt` on a string, a fractional
bound on an `int`, or two `field` annotations on one member) is a **compile-time
error**.

## Response Types

Every handler returns a `fiasco::response`. If you return a plain value (string, int, or a model struct), it's wrapped automatically. For full control, use the factory methods:

```cpp
app.get("/text", []() {
    return fiasco::response::text("plain text");
});

app.get("/html", []() {
    return fiasco::response::html("<h1>Hello</h1>");
});

app.get("/json", []() {
    return fiasco::response::json(R"({"key":"value"})");
});

app.get("/redirect", []() {
    return fiasco::response::redirect("/new-location");
});

app.get("/error", []() {
    return fiasco::response::error("something went wrong", 400);
});

app.get("/empty", []() {
    return fiasco::response::empty(204);
});
```

Status codes and headers can be set directly on the response struct after construction.

## Sub-routers

Mount groups of routes under a prefix.

```cpp
fiasco::router api("/v1");
api.get("/status", []() { return "ok"; });

fiasco::server app;
app.include_router(std::move(api));
// GET /v1/status now works
```

## Threading

By default, Fiasco spawns `std::thread::hardware_concurrency()` threads (minimum 2). Pass the desired count to the constructor:

```cpp
fiasco::server app(4);  // 4 worker threads
app.run(8080);
```

## Signal Handling

The server catches `SIGINT` and `SIGTERM` internally via Asio's `signal_set`. Pressing Ctrl+C shuts down all worker threads gracefully and returns from `run()`.

## Dependencies

All fetched automatically at configure time:

- [nlohmann/json](https://github.com/nlohmann/json) v3.12 — JSON core
- [llhttp](https://github.com/nodejs/llhttp) v9.4 — HTTP/1.1 parser
- [Asio](https://github.com/chriskohlhoff/asio) 1.38.2 — async I/O
- [Catch2](https://github.com/catchorg/Catch2) v3.16 (tests only)

## License

MIT — see [LICENSE](LICENSE).
