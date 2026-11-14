# Benchmarks

One scenario, one reference, one tool. The goal is to measure the framework's
overhead on a realistic request, not to win a synthetic benchmark.

## Scenario

`POST /validate` with a JSON body:

```json
{"username":"ada","age":36}
```

The server binds the body to a model, validates it, and returns:

```json
{"username":"ada","age":37}
```

The same request is served by two binaries:

- `fiasco_json_bind` — Fiasco's annotation routing, reflection, validation, and
  JSON binding.
- `raw_json_bind` — a hand-written Asio/Beast HTTP/1.1 server with the same
  `nlohmann::json` parse and the same validation. This is the baseline.

## Build

```shell
cmake -S . -B build -DFIASCO_BUILD_BENCHMARKS=ON
cmake --build build --target fiasco_json_bind raw_json_bind
```

## Run

```shell
./benchmarks/run.sh fiasco 10 100 4
./benchmarks/run.sh raw 10 100 4
```

`run.sh` starts the server, waits for the port, runs `wrk` with the request
script in `post.lua`, and prints the commit, host, CPU, worker count, `wrk`
thread count, and tool version next to the result.

## Results

| Target | Requests/sec | Median | Commit | Host | CPU | Workers | Tool |
|---|---|---|---|---|---|---|---|
| Fiasco, epoll | 81,023 / 82,251 / 80,632 | 81,023 | `dd82c16` | WSL2, Linux 6.18.33.2 | AMD Ryzen 5 PRO 5650U | 4 | wrk 4.1.0, `-t4` |
| Fiasco, io_uring | 95,829 / 101,936 / 104,017 | 101,936 | `dd82c16` | WSL2, Linux 6.18.33.2 | AMD Ryzen 5 PRO 5650U | 4 | wrk 4.1.0, `-t4` |
| Raw Asio/Beast | 93,411 / 91,196 / 93,418 | 93,411 | `dd82c16` | WSL2, Linux 6.18.33.2 | AMD Ryzen 5 PRO 5650U | 4 | wrk 4.1.0, `-t4` |

Three interleaved rounds each, 10 seconds, 100 connections, 4 Fiasco workers,
4 raw workers, 4 `wrk` threads, Release build. io_uring is enabled with
`-DFIASCO_WITH_IO_URING=ON` (liburing 2.14). Fiasco's io_uring backend is about
**+26%** over its epoll backend on this run and slightly ahead of the raw
baseline. The raw server still does less work per request; treat it as a
reference, not an apples-to-apples competitor.

## Caveats

- The numbers are from a single machine (WSL2, 12 threads) and are noisy; the
  absolute values move between sessions. Run the same protocol on the target
  hardware before quoting a number.
- The raw server is a baseline, not a competitor. It does not implement the
  routing, reflection, validation, or error handling that Fiasco does.
- The benchmark is not a regression test. Do not gate CI on it.
