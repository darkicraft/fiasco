#!/usr/bin/env bash
set -euo pipefail

# 30-second tour. Build the example first:
#   cmake --build build --target annotation_routes
#
# Record it with:
#   asciinema rec -c ./demo/demo.sh demo.cast
#
# The script starts annotation_routes and prints the interesting responses:
# annotation routing, a JSON body, validation, :path capture, and OpenAPI.

pid=""
cleanup() {
  [[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true
}
trap cleanup EXIT

FIASCO_PORT=8080 ./build/examples/annotation_routes >/tmp/fiasco-demo.log 2>&1 &
pid=$!
sleep 0.5

echo "=== annotation route ==="
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
  -d '{}'

echo "=== :path capture ==="
curl -s localhost:8080/files/reports/2026.txt
echo

echo "=== OpenAPI ==="
curl -s -o /dev/null -w 'openapi.json: %{http_code}\n' localhost:8080/openapi.json
curl -s -o /dev/null -w 'swagger UI:   %{http_code}\n' localhost:8080/docs
curl -s -o /dev/null -w 'ReDoc:        %{http_code}\n' localhost:8080/redoc
