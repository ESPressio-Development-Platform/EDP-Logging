#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$(printenv EDP_LOGGING_DEPENDENCY_ROOT || true)"
if test -z "$DEPS"; then DEPS="$(cd "$ROOT/.." && pwd)"; fi
CC="$(printenv CXX || true)"
if test -z "$CC"; then CC=g++; fi
EXE="$(mktemp)"
trap 'rm -f "$EXE"' EXIT
FLAGS=()
if test "${EDP_INSPECTION_UBSAN:-0}" = 1; then
  FLAGS=(-fsanitize=undefined -fno-sanitize-recover=all)
fi
"$CC" "${FLAGS[@]}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread -I"$ROOT/src" -I"$DEPS/EDP-Persistence/src" -I"$DEPS/EDP-System/src" -I"$DEPS/EDP-Platform/src" "$ROOT/tests/EdlsArchiveInspectionTests.cpp" -o "$EXE"
"$EXE"
echo "EDP-Logging bounded EDLS archive inspection PASS ($CC)"
