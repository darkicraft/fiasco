#!/usr/bin/env bash
set -euo pipefail

# 30-second tour. Build the all-in-one server first:
#   cmake --build build --target all_in_one
#
# Record it with:
#   asciinema rec -c ./demo/demo.sh demo.cast
#
# The script starts examples/all_in_one/main and prints the interesting
# responses: routing, parameters, models, validation, DI, middleware, errors,
# cookies, files/ranges, static files, SSE, JSON Lines and OpenAPI.

pid=""
cleanup() {
  [[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true
}
trap cleanup EXIT

./build/examples/all_in_one/main >/tmp/fiasco-demo.log 2>&1 &
pid=$!
sleep 0.5

echo "=== root ==="
curl -s localhost:8080/
echo

echo "=== typed path parameter ==="
curl -s localhost:8080/items/42
echo

echo "=== reflected JSON body ==="
curl -s -X POST localhost:8080/items \
  -H 'Content-Type: application/json' \
  -d '{"name":"widget","quantity":3}'
echo

echo "=== validation error ==="
curl -s -o /dev/null -w '%{http_code}\n' \
  -X POST localhost:8080/items \
  -H 'Content-Type: application/json' \
  -d '{"name":"widget","quantity":0}'

echo "=== query binding ==="
curl -s 'localhost:8080/search?q=widget&limit=5'
echo

echo "=== dependency injection ==="
curl -s localhost:8080/me -H 'Authorization: Bearer ada'
echo

echo "=== typed async ==="
curl -s localhost:8080/async/typed
echo

echo "=== custom exception handler ==="
curl -s -o /dev/null -w '%{http_code}\n' localhost:8080/error

echo "=== cookies ==="
curl -s -D - -o /dev/null localhost:8080/cookie | grep -E '^(HTTP|Set-Cookie)'

echo "=== streamed file range ==="
curl -s -D - -H 'Range: bytes=0-9' localhost:8080/download | sed -n '1,12p'

echo "=== static file ==="
curl -s localhost:8080/static/style.css | head -3

echo "=== SSE ==="
curl -sN --max-time 2 localhost:8080/events

echo "=== JSON Lines ==="
curl -s localhost:8080/lines

echo "=== OpenAPI and docs ==="
curl -s -o /dev/null -w 'openapi.json: %{http_code}\n' localhost:8080/openapi.json
curl -s -o /dev/null -w 'swagger UI:   %{http_code}\n' localhost:8080/docs
curl -s -o /dev/null -w 'ReDoc:        %{http_code}\n' localhost:8080/redoc
