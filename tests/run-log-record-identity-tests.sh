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
  -I"$DEPS/EDP-Clock/src" -I"$DEPS/EDP-Localisation/src" \
  -I"$DEPS/EDP-Serialisation/src" \
  "$ROOT/tests/LogRecordIdentityTests.cpp" -o "$OUTPUT"
"$OUTPUT"
echo "EDP-Logging governed 15-field SchemaType and bounded-owned-arguments PASS ($CC)"
