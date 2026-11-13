#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$(printenv EDP_LOGGING_DEPENDENCY_ROOT || true)"
if test -z "$DEPS"; then DEPS="$(cd "$ROOT/.." && pwd)"; fi
CC="$(printenv CXX || true)"
if test -z "$CC"; then CC=g++; fi
OUT="$(mktemp)"
trap 'rm -f "$OUT"' EXIT
"$CC" -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread \
 -I"$ROOT/src" -I"$DEPS/EDP-Memory/src" -I"$DEPS/EDP-Platform-Portable/src" -I"$DEPS/EDP-BoundedTopology/src" -I"$DEPS/EDP-Persistence/src" \
 -I"$DEPS/EDP-System/src" -I"$DEPS/EDP-Platform/src" \
 "$ROOT/tests/EdlgCheckpointTests.cpp" -o "$OUT"
"$OUT"
echo "EDP-Logging bounded EDLG KeyValueStorage checkpoint PASS ($CC)"
