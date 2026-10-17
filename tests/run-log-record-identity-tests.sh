#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$(printenv EDP_LOGGING_DEPENDENCY_ROOT || true)"
if test -z "$DEPS"; then DEPS="$(cd "$ROOT/.." && pwd)"; fi
CC="$(printenv CXX || true)"
if test -z "$CC"; then CC=g++; fi
OUTPUT="$(mktemp)"
trap 'rm -f "$OUTPUT"' EXIT
"$CC" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -I"$ROOT/src" -I"$DEPS/EDP-System/src" \
  "$ROOT/tests/LogRecordIdentityTests.cpp" -o "$OUTPUT"
"$OUTPUT"
echo "EDP-Logging permanent System LogRecord identity PASS ($CC)"
