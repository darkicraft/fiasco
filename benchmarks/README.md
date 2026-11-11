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

- `fiasco_json_bind` — Fiasco's reflection, validation, and routing on top of
  `tcp_server`.
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
| Fiasco, epoll | 48,691 / 46,568 / 47,599 / 51,117 / 44,765 | 47,599 | `edd9ef2` | WSL2, Linux 6.18.33.2 | AMD Ryzen 5 PRO 5650U | 4 | wrk 4.1.0, `-t4` |
| Fiasco, io_uring | 65,318 / 67,696 / 62,966 / 54,576 / 70,296 | 65,318 | `edd9ef2` | WSL2, Linux 6.18.33.2 | AMD Ryzen 5 PRO 5650U | 4 | wrk 4.1.0, `-t4` |
| Raw Asio/Beast | 78,259 / 84,740 / 83,565 / 69,124 / 88,864 | 83,565 | `edd9ef2` | WSL2, Linux 6.18.33.2 | AMD Ryzen 5 PRO 5650U | 4 | wrk 4.1.0, `-t4` |

Five interleaved rounds each, 10 seconds, 100 connections, 4 Fiasco workers,
4 raw workers, 4 `wrk` threads, Release build. io_uring is enabled with
`-DFIASCO_WITH_IO_URING=ON` (liburing 2.14). Fiasco's io_uring backend is about
**+37%** over its epoll backend on this run. The raw server is a baseline, not
an apples-to-apples competitor: it does less work per request.

## Caveats

- The numbers are from a single machine (WSL2, 12 threads) and are noisy; the
  absolute values move between sessions. Run the same protocol on the target
  hardware before quoting a number.
- The raw server is a baseline, not a competitor. It does not implement the
  routing, reflection, validation, or error handling that Fiasco does.
- The benchmark is not a regression test. Do not gate CI on it.
