# Fiasco examples

The examples are split by feature: each target is a small, standalone program
that demonstrates one part of the API. Start with `hello`, then follow the
categories below. `all_in_one` is intentionally different: it combines several
features into a heavier workload and is not the place to learn a single API.

Every example is self-contained. All runnable examples read `FIASCO_PORT`
(default `8080`), so several can run at once without editing code:

```shell
FIASCO_PORT=9001 ./build/examples/path_params
curl 'localhost:9001/items/42'
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
| `server_configuration` | `server/configuration.cpp` | `server_config`, host/port/workers, port `0`, and `wait_until_started`/`port()` lifecycle helpers. |
| `threading` | `server/threading.cpp` | Worker configuration and concurrent sleep/CPU/thread-identity probes. |

## JSON

| Target | Source | Demonstrates |
|---|---|---|
| `json_wrapper` | `json/wrapper.cpp` | Recursive `fiasco::json` construction, objects/arrays, dump/parse, and typed access. |

## Modules

C++20 modules are parked for now. The sources under `modules/` are retained,
but the module target and example are not built until the module surface is
repaired.

## All-in-one

| Target | Source | Demonstrates |
|---|---|---|
| `all_in_one` | `all_in_one/main.cpp` | A heavier analytics/batch workload combining models, constraints, sub-routers, responses, JSON, and threading. |

A representative request:

```shell
curl -X POST 'localhost:8080/v1/analyze/7/3/0.1/true?page=3&verbose=true&q=hi' \
  -H 'Content-Type: application/json' \
  -d '{
    "req": {
      "label": "aabbbcc",
      "user-name": "ada",
      "weights": [1, 2, 3],
      "offsets": {"a": 1.5, "b": -2.0},
      "window": [2, 9],
      "meta": [7, "x", true],
      "regions": [{"x": 5, "y": 5, "note": null}],
      "focus": null
    },
    "tuning": {"percent": 50, "boxes": 9, "damping": 1.5, "code": "abc", "matrix_n": 5}
  }'
```

The other focused examples are the reference for each feature used here.

Each example's source header also contains the exact `curl` commands for the
routes it registers.
