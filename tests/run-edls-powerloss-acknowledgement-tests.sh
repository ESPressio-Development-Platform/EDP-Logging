#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$(printenv EDP_LOGGING_DEPENDENCY_ROOT || true)"
if test -z "$DEPS"; then DEPS="$(cd "$ROOT/.." && pwd)"; fi
CC="$(printenv CXX || true)"
if test -z "$CC"; then CC=g++; fi
BIN="$(mktemp)"
trap 'rm -f "$BIN"' EXIT
FLAGS=()
if test "${EDP_ACK_UBSAN:-0}" = 1; then
  FLAGS=(-fsanitize=undefined -fno-sanitize-recover=all)
fi
"$CC" "${FLAGS[@]}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread \
  -Wno-unused-function -I"$ROOT/src" -I"$ROOT/tests" -I"$DEPS/EDP-Persistence/src" \
  -I"$DEPS/EDP-Persistence-ESP-IDF/src" \
  -I"$DEPS/EDP-Memory/src" -I"$DEPS/EDP-Platform-Portable/src" \
  -I"$DEPS/EDP-Platform/src" -I"$DEPS/EDP-System/src" \
  -I"$DEPS/EDP-BoundedTopology/src" \
  -I"$DEPS/EDP-Threading/src" -I"$DEPS/EDP-Clock/src" \
  "$ROOT/tests/EdlsPowerLossAcknowledgementTests.cpp" -o "$BIN"
"$BIN"
echo "EDP-Logging strict PowerLoss NVS ACK + immutable EDLS recovery PASS ($CC)"
