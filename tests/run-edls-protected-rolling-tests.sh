#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$(printenv EDP_LOGGING_DEPENDENCY_ROOT || true)"
if test -z "$DEPS"; then DEPS="$(cd "$ROOT/.." && pwd)"; fi
CC="$(printenv CXX || true)"
if test -z "$CC"; then CC=g++; fi
EXE="$(mktemp)"
FLAGS=()
if test "$(printenv EDP_EDLS_UBSAN || true)" = 1; then FLAGS=(-fsanitize=undefined -fno-sanitize-recover=all); fi
trap 'rm -f "$EXE"' EXIT
"$CC" "${FLAGS[@]}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread \
  -I"$ROOT/src" -I"$DEPS/EDP-Memory/src" -I"$DEPS/EDP-Platform-Portable/src" -I"$DEPS/EDP-BoundedTopology/src" -I"$DEPS/EDP-Persistence/src" \
  -I"$DEPS/EDP-System/src" -I"$DEPS/EDP-Platform/src" \
  "$ROOT/tests/EdlsProtectedRollingTests.cpp" -o "$EXE"
"$EXE"
echo "EDP-Logging finite immutable N+1 protected EDLS rolling PASS ($CC)"
