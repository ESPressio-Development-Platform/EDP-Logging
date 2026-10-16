#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$(cd "$ROOT/.." && pwd)"
if test -n "$(printenv EDP_LOGGING_DEPENDENCY_ROOT)"; then DEPS="$(printenv EDP_LOGGING_DEPENDENCY_ROOT)"; fi
OUT="$(mktemp)"
trap 'rm -f "$OUT"' EXIT
"$1" -std=c++20 -pthread -Wall -Wextra -Wpedantic -Werror -Wno-unused-function \
 -I"$ROOT/src" -I"$ROOT/tests" -I"$DEPS/EDP-Memory" -I"$DEPS/EDP-Memory/src" \
 -I"$DEPS/EDP-Threading/src" -I"$DEPS/EDP-Clock/src" \
 -I"$DEPS/EDP-Platform/src" -I"$DEPS/EDP-System/src" \
 -I"$DEPS/EDP-BoundedTopology/src" \
 "$ROOT/tests/RecordProductionTests.cpp" -o "$OUT"
"$OUT"
echo "EDP-Logging record producer: PASS ($1)"
