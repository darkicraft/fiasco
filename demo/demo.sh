#!/usr/bin/env bash
set -euo pipefail

# 30-second tour. Build the examples first:
#   cmake --build build --target hello body_model
#
# Record it with:
#   asciinema rec -c ./demo/demo.sh demo.cast
#
# The script starts two servers on localhost and prints the interesting
# responses. It is intentionally short: one request, one JSON body, one
# validation error.

hello_pid=""
body_pid=""
cleanup() {
  [[ -n "$hello_pid" ]] && kill "$hello_pid" 2>/dev/null || true
  [[ -n "$body_pid" ]] && kill "$body_pid" 2>/dev/null || true
}
trap cleanup EXIT

echo "=== hello ==="
FIASCO_PORT=8080 ./build/examples/hello &
hello_pid=$!
sleep 0.5
curl -s localhost:8080/
echo

echo "=== reflected JSON body ==="
FIASCO_PORT=8081 ./build/examples/body_model &
body_pid=$!
sleep 0.5
curl -s -X POST localhost:8081/users \
  -H 'Content-Type: application/json' \
  -d '{"username":"ada","age":36}'
echo

echo "=== validation error ==="
curl -s -o /dev/null -w '%{http_code}\n' \
  -X POST localhost:8081/users \
  -H 'Content-Type: application/json' \
  -d '{"username":"ada"}'
