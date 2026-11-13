# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Annotation route factories (`fiasco::get`, `post`, `put`, `patch`, `del`) and
  namespace registration with `register_api<^^ns>()`.
- Registration-time route descriptors, `router_options`, duplicate-route and
  empty-namespace checks, and FastAPI's documented `:path` converter.
- OpenAPI 3.1 output at `openapi_url`, including paths, parameters, request and
  response bodies, reflected component schemas, and field constraints.
- Swagger UI and ReDoc, with configurable URLs, CDN or self-hosted assets,
  Swagger UI parameters, and OAuth initialization.
- Typed async handlers with `fiasco::async_t<T>` and explicit `response_model`
  annotations for OpenAPI.
- Response cookies with multiple `Set-Cookie` headers, UTC datetime expiry,
  `delete_cookie`, and cookie attribute validation.
- Static file mounts with MIME inference, optional `index.html`, `GET`/`HEAD`,
  traversal protection, and 405 responses.
- Buffered file responses and attachment downloads via `response::file`.
- 405 `Allow` headers and trailing-slash redirects.
- HTTP/1.1 server with keep-alive, `Connection: close`, and malformed-request
  handling.
- Per-core Asio I/O on Linux/BSD with `SO_REUSEPORT`; shared IOCP model on
  Windows; opt-in io_uring with epoll fallback. `SIGINT`/`SIGTERM` graceful
  shutdown.
- Path parameters bound by name with type conversion.
- Query parameters bound by name with type conversion.
- Explicit parameter sources: `path`, `query`, `header`, `cookie`, and `body`.
- JSON request/response models driven by reflection, with no macros.
- Nested models and `vector` / `map` / `pair` / `tuple` / `optional` fields.
- Multiple body parameters, automatically embedded by name.
- Pydantic-compatible required-versus-nullable rules.
- `[[= field{...}]]` constraints: `gt`, `ge`, `lt`, `le`, `multiple_of`,
  `min_length`, `max_length`, `pattern`, and `alias`.
- FastAPI-compatible `422` validation payloads (`detail`, `type`, `loc`, `msg`,
  `input`, `ctx`).
- Type-erased JSON wrapper.
- Synchronous dependency injection with per-request caching.
- `http_error` and custom exception handlers.
- Application middleware and a built-in CORS middleware.
- Coroutine/async handlers.
- Sub-router mounting with path prefixes.
- Mounted sub-applications.
- In-process test client over the application exchange.
- Three-layer split: `fiasco_core` (Asio), `fiasco_contract` (boundary),
  `fiasco_http` (transport), `fiasco_app` (framework), and the `fiasco`
  umbrella.
- Type-erased `application_handler`, `response_sink`, and `body_source`.
- Declarative per-server configuration.
- CI for GCC 16, ASan, and TSan.

### Changed

- The transport now drives a type-erased application handler; the handler is
  still free of sockets and Asio beyond the temporary `task` alias.
- `fiasco_http` owns the HTTP session and parser; `fiasco_core` is Asio only.
- Route declaration is annotation-only: public `app.get/post/...` and
  `router.get/...` shortcuts were removed. Imperative `add_route` is deferred.
- `fiasco::server` and `server_config` are public; `app.run()` is zero-config
  sugar, and runtime configuration is passed to `fiasco::server`.
- Examples use the zero-config `app.run()` form.
- C++20 module targets and the module CI job are parked; the sources remain
  under `modules/` but are not built.

### Removed

- Per-server logging (`log_registry`) and access logging are removed from the
  runtime. Logging will return via spdlog/Quill at the runtime layer.
- The redundant `all_in_one` example.

[Unreleased]: https://github.com/darkicraft/fiasco/commits/main
