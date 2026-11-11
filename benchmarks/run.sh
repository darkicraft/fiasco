#!/usr/bin/env bash
set -euo pipefail

# Usage: ./run.sh <fiasco|raw> [seconds] [connections] [wrk_threads]
#
# Build the benchmarks first:
#   cmake -S . -B build -DFIASCO_BUILD_BENCHMARKS=ON
#   cmake --build build --target fiasco_json_bind raw_json_bind

target="${1:-fiasco}"
seconds="${2:-10}"
connections="${3:-100}"
wrk_threads="${4:-4}"
workers="${FIASCO_WORKERS:-4}"
export FIASCO_WORKERS="$workers"

case "$target" in
  fiasco) binary="./build/benchmarks/fiasco_json_bind"; port=8080 ;;
  raw)    binary="./build/benchmarks/raw_json_bind"; port=8081 ;;
  *) echo "usage: $0 <fiasco|raw> [seconds] [connections] [wrk_threads]" >&2; exit 2 ;;
esac

if [[ ! -x "$binary" ]]; then
  echo "missing $binary; build with -DFIASCO_BUILD_BENCHMARKS=ON" >&2
  exit 1
fi

"$binary" "$port" &
server_pid=$!
trap 'kill "$server_pid" 2>/dev/null || true' EXIT

for _ in $(seq 1 100); do
  if (exec 3<>"/dev/tcp/127.0.0.1/$port") 2>/dev/null; then
    exec 3>&-
    break
  fi
  sleep 0.05
done

echo "target: $target"
echo "binary: $binary"
echo "port: $port"
echo "seconds: $seconds"
echo "connections: $connections"
echo "workers: $workers"
echo "wrk threads: $wrk_threads"
echo "commit: $(git rev-parse --short HEAD)"
echo "host: $(uname -srm)"
echo "cpu: $(lscpu | sed -n 's/^Model name: *//p' | head -1)"
echo "threads: $(nproc)"
echo "wrk: $(wrk --version 2>&1 | head -1)"

wrk -t"$wrk_threads" -c"$connections" -d"${seconds}s" -s benchmarks/post.lua "http://127.0.0.1:$port/validate"
