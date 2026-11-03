# Fiasco examples

The examples are split by feature: each target is a small, standalone program
that demonstrates one part of the API. Start with `hello`, then follow the
categories below. `all_in_one` is intentionally different: it combines several
features into a heavier workload and is not the place to learn a single API.

Every example is self-contained and reads `FIASCO_PORT` (default `8080`), so
several can run at once without editing code:

```shell
FIASCO_PORT=9001 ./build/examples/path_params
curl 'localhost:9001/items/42'
```

Build a single target from the repository root:

```shell
cmake --build build --target hello
```

## Basics

| Target | Source | Demonstrates |
|---|---|---|
| `hello` | `basics/hello.cpp` | The smallest useful server: one route and a text response. |

## Routing

| Target | Source | Demonstrates |
|---|---|---|
| `routing_methods` | `routing/methods.cpp` | GET/POST/PUT/PATCH/DELETE, static and parameter routes, 404, and 405. |
| `subrouters` | `routing/subrouters.cpp` | Router prefixes, nested `include_router`, and `print_routes()`. |

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

## Errors

| Target | Source | Demonstrates |
|---|---|---|

## Responses

| Target | Source | Demonstrates |
|---|---|---|

## Server

| Target | Source | Demonstrates |
|---|---|---|

## JSON

| Target | Source | Demonstrates |
|---|---|---|

## Modules

| Target | Source | Demonstrates |
|---|---|---|

## All-in-one

| Target | Source | Demonstrates |
|---|---|---|

Each example's source header also contains the exact `curl` commands for the
routes it registers.
