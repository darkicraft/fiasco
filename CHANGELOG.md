# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- HTTP/1.1 server with keep-alive, `Connection: close`, and malformed-request
  handling.
- Multi-threaded Asio-based I/O with `SIGINT`/`SIGTERM` graceful shutdown.
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
- Declarative per-server configuration and per-server logging with configurable
  sinks.
- C++20 modules (experimental, opt-in).
- CI for GCC 16, ASan, TSan, and C++20 modules.

## [v0.1.0]

First release. The annotation-driven route registration, OpenAPI document,
Swagger UI, ReDoc, static files, response cookies, and the bundled documentation
assets land here.

[Unreleased]: https://github.com/darkicraft/fiasco/commits/main
[v0.1.0]: https://github.com/darkicraft/fiasco/releases/tag/v0.1.0
