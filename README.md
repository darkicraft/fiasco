# Fiasco

An expressive C++26 web framework that brings FastAPI-like ergonomics to C++.

> **Status**: Alpha. The core API works, but expect breaking changes as the
> surface matures.

## Requirements

- CMake ≥ 3.30
- GCC ≥ 16 (C++26 reflection via `-freflection`)

Only GCC is supported for now.

## Build

```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$(nproc)"
ctest --test-dir build --output-on-failure
```

Useful options:

```shell
-DCMAKE_BUILD_TYPE=Debug
-DFIASCO_BUILD_TESTS=OFF
-DFIASCO_BUILD_EXAMPLES=OFF
-DFIASCO_BUILD_BENCHMARKS=ON
-DFIASCO_WITH_IO_URING=ON   # Linux, requires liburing
```

Examples are built by default:

```shell
./build/examples/hello
```

## Installation and consumption

Fiasco is a CMake project with header-only public layers and compiled support
targets. The simplest integration is a subdirectory or FetchContent:

```cmake
add_subdirectory(third_party/fiasco)
target_link_libraries(my_app PRIVATE fiasco)
```

`cmake --install build` currently installs the public headers. A generated
CMake package config is not provided yet, so `find_package(Fiasco)` is not
available.

## Basic usage

```cpp
#include <fiasco/fiasco.hpp>

#include <string>

using fiasco::get;
using fiasco::json;
using fiasco::path;
using fiasco::post;

namespace api {

struct create_item_request {
    std::string name;
};

[[=get("/items/{id}", {.summary = "Read an item"})]]
auto read_item(long id [[=path{.ge = 1}]]) -> std::string {
    return "item " + std::to_string(id);
}

[[=post("/items")]]
auto create_item(create_item_request input) -> json {
    return json{{"name", input.name}};
}

}  // namespace api

int main() {
    fiasco::app app;
    app.register_api<^^api>();
    app.run();  // 0.0.0.0:8080, automatic worker count
}
```

Handlers can return strings, numbers, models, `fiasco::json`, or
`fiasco::response`. Plain values are wrapped automatically.

## Configuration and lifecycle

`app.run()` uses `0.0.0.0:8080` and automatic worker count. Runtime config
belongs to `fiasco::server`:

```cpp
fiasco::app app;
app.register_api<^^api>();

fiasco::server server(app.handler(), {
    .network = {
        .host = "0.0.0.0",
        .port = 8080,
        .workers = 4,  // 0 = hardware concurrency, minimum 2
    },
});

server.run();
```

`fiasco::server` also exposes `port()`, `wait_until_started()`, and `stop()`
for dynamic ports and programmatic lifecycle control.

## OpenAPI and docs

Every app serves an OpenAPI 3.1 document by default:

- `GET /openapi.json`
- `GET /docs` — Swagger UI
- `GET /redoc` — ReDoc

## Feature overview

| Area | Available |
|---|---|
| Routing | Annotation routes, sub-routers, path/query/header/cookie/body binding, `:path` catch-all, 405 `Allow`, trailing-slash redirects |
| Models | Reflected structs, nested models, `vector`/`map`/`pair`/`tuple`/`optional`, field constraints, FastAPI-style 422 errors |
| Dependencies | `[[=depends{^^fn}]]`, request/header/path/query/body injection, per-request caching |
| Middleware | Application middleware and built-in CORS |
| OpenAPI | JSON document, parameters, request/response bodies, component schemas, `response_model`, typed async |
| Docs | Swagger UI, ReDoc, configurable URLs and CDN/self-hosted assets |
| Responses | Text/JSON/HTML/redirect/error helpers, cookies, file responses |
| Static files | Mounted directories, MIME lookup, index files, HEAD, traversal protection |
| Runtime | Asio HTTP/1.1, keep-alive, per-core workers, `SO_REUSEPORT`, optional io_uring |
| Async | `fiasco::async` (untyped) and `fiasco::async_t<T>` (typed) |

Not implemented yet: response/request streaming, SSE, WebSockets, TLS,
multipart/file uploads, security helpers, `app.state`, dependency overrides,
`url_path_for`, and imperative `add_route` registration. C++20 modules are
parked: the sources remain under `modules/`, but there is no module build target.

## Examples and demo

The focused examples live in [`examples/`](examples/README.md). They cover
routing, parameter sources, models, validation, DI, middleware, errors,
responses, OpenAPI, static files, cookies, and file downloads.

The 30-second tour is [`demo/demo.sh`](demo/demo.sh):

```shell
./demo/demo.sh
```

## Third-party dependencies

Fetched at configure time:

- [nlohmann/json](https://github.com/nlohmann/json) pinned commit — JSON
- [llhttp](https://github.com/nodejs/llhttp) v9.4.3 — HTTP/1.1 parser
- [Asio](https://github.com/chriskohlhoff/asio) 1.38.2 — async I/O
- [Catch2](https://github.com/catchorg/Catch2) v3.16.1 — tests only

## License

MIT — see [LICENSE](LICENSE).
