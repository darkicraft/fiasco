# Fiasco examples

The examples are split by feature: each target is a small, standalone program
that demonstrates one part of the API. Start with `hello`, then follow the
categories below. All route examples use the annotation API and
`app.register_api<^^ns>()`.

Every example is self-contained and runs on the default `8080` via
`app.run()`. Run one at a time when they share a port.

```shell
./build/examples/path_params
curl 'localhost:8080/items/42'
```

`static_files` and `response_file` resolve their sample files relative to the
repository root, so run those from the repository root. The parked module
example is not built.

Build a single target from the repository root:

```shell
cmake --build build --target hello
```

## Basics

| Target | Source | Demonstrates |
|---|---|---|
| `hello` | `basics/hello.cpp` | The smallest useful server: one route and a text response. |

## Async

| Target | Source | Demonstrates |
|---|---|---|
| `async_handlers` | `async/basic.cpp` | Sync vs `fiasco::async` handlers, typed `fiasco::async_t<T>`, and explicit responses. |

## Middleware

| Target | Source | Demonstrates |
|---|---|---|
| `middleware` | `middleware/basic.cpp` | `app.middleware`, last-added-outermost ordering, request/response mutation, and short-circuiting. |
| `cors` | `middleware/cors.cpp` | Built-in CORS middleware with origins, methods, headers, credentials, and preflight. |

## Routing

| Target | Source | Demonstrates |
|---|---|---|
| `routing_methods` | `routing/methods.cpp` | GET/POST/PUT/PATCH/DELETE, static and parameter routes, 404, and 405. |
| `annotation_routes` | `routing/annotations.cpp` | Namespace-annotated free functions, `register_api`, body binding, and `:path`. |
| `subrouters` | `routing/subrouters.cpp` | Router prefixes, nested `include_router`, and `print_routes()`. |

## OpenAPI

| Target | Source | Demonstrates |
|---|---|---|
| `openapi` | `openapi/basic.cpp` | `app_options` metadata, router tags, parameter constraints, request/response schemas, explicit `response_model`, Swagger UI, and ReDoc. |

## Static files

| Target | Source | Demonstrates |
|---|---|---|
| `static_files` | `static/basic.cpp` | `mount_static`, `index.html`, content types, and `HEAD`. |

## Parameters

| Target | Source | Demonstrates |
|---|---|---|
| `path_params` | `params/path.cpp` | Name-bound, order-independent path parameters across scalar and fixed-width types. |
| `query_params` | `params/query.cpp` | Typed query binding, optional values, percent decoding, duplicates, and missing/parse errors. |
| `param_sources` | `params/sources.cpp` | Explicit `path`, `query`, `body`, `header`, and `cookie` markers with aliases and optionals. |

## Models

| Target | Source | Demonstrates |
|---|---|---|
| `body_model` | `models/body.cpp` | A single reflected JSON request model and reflected response model. |
| `multiple_bodies` | `models/multiple_bodies.cpp` | Several body parameters embedded by name into one JSON object. |
| `collections` | `models/collections.cpp` | Nested structs, `vector`, `map`, `pair`, `tuple`, and `optional` in requests and responses. |
| `requiredness` | `models/requiredness.cpp` | Required vs nullable vs defaulted members, including absent and `null` cases. |

## Validation

| Target | Source | Demonstrates |
|---|---|---|
| `constraints` | `validation/constraints.cpp` | `gt`/`ge`/`lt`/`le`/`multiple_of`, length, `pattern`, `alias`, and collected `422` errors. |

## Dependencies

| Target | Source | Demonstrates |
|---|---|---|
| `dependencies` | `dependencies/basic.cpp` | Nested providers, request/header injection, per-request cache reuse, and RAII cleanup. |
| `dependency_caching` | `dependencies/caching.cpp` | Default provider caching vs `use_cache = false`, with per-request call counts. |
| `dependency_graph` | `dependencies/graph.cpp` | Advanced manual stress test: a large, redundant provider graph must run each provider once. |

## Errors

| Target | Source | Demonstrates |
|---|---|---|
| `http_errors` | `errors/http_error.cpp` | `http_error` with status, string or structured detail, and response headers. |
| `exception_handlers` | `errors/custom_handler.cpp` | Domain exceptions mapped by inferred and explicit handler types. |

## Responses

| Target | Source | Demonstrates |
|---|---|---|
| `responses` | `responses/factories.cpp` | Response factories, automatic return wrapping, status/header mutation, redirect, and empty responses. |
| `response_cookies` | `responses/cookies.cpp` | `set_cookie`, multiple `Set-Cookie` headers, datetimes, and `delete_cookie`. |
| `response_file` | `responses/download.cpp` | `response::file`, inferred media types, inline serving, and attachment downloads. |

## Server

| Target | Source | Demonstrates |
|---|---|---|
| `threading` | `server/threading.cpp` | Concurrent sleep/CPU/thread-identity probes. |

## JSON

| Target | Source | Demonstrates |
|---|---|---|
| `json_wrapper` | `json/wrapper.cpp` | Recursive `fiasco::json` construction, objects/arrays, dump/parse, and typed access. |

## Modules

C++20 modules are parked for now. The sources under `modules/` are retained,
but the module target and example are not built until the module surface is
repaired.

Each example's source header contains the exact `curl` commands for the routes
it registers.
