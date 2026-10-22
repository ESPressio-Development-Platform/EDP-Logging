#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$(cd "$ROOT/.." && pwd)"
if test -n "$(printenv EDP_LOGGING_DEPENDENCY_ROOT)"; then DEPS="$(printenv EDP_LOGGING_DEPENDENCY_ROOT)"; fi
OUT="$(mktemp)"
ERROR="$(mktemp)"
trap 'rm -f "$OUT" "$ERROR"' EXIT
"$1" -std=c++20 -pthread -Wall -Wextra -Wpedantic -Werror -Wno-unused-function \
 -I"$ROOT/src" -I"$ROOT/tests" -I"$DEPS/EDP-Memory" -I"$DEPS/EDP-Memory/src" \
 -I"$DEPS/EDP-Threading/src" -I"$DEPS/EDP-Clock/src" \
 -I"$DEPS/EDP-Platform/src" -I"$DEPS/EDP-System/src" \
 -I"$DEPS/EDP-BoundedTopology/src" \
 "$ROOT/tests/RecordProductionTests.cpp" -o "$OUT"
"$OUT"
# This is a *negative* compile contract, not an expected runtime failure.
# The diagnostic must identify the precise bound, not an unrelated error.
for count in 0 9; do
 if "$1" -std=c++20 -fsyntax-only -DEDP_TEST_SINK_COUNT="$count" \
  -I"$ROOT/src" -I"$DEPS/EDP-Memory/src" \
  -I"$DEPS/EDP-Threading/src" -I"$DEPS/EDP-Clock/src" \
  -I"$DEPS/EDP-Platform/src" -I"$DEPS/EDP-System/src" \
  -I"$DEPS/EDP-BoundedTopology/src" \
  "$ROOT/tests/CompileFailRecordFanoutSinkCount.cpp" >"$ERROR" 2>&1; then
  echo "ERROR: sink count $count unexpectedly compiled ($1)" >&2
  exit 1
 fi
 if ! grep -q 'Record fanout requires 1..8 sinks' "$ERROR"; then
  cat "$ERROR" >&2
  echo "ERROR: sink count $count rejected for an unrelated reason ($1)" >&2
  exit 1
 fi
done
echo "EDP-Logging record producer: PASS ($1, including zero/nine-sink compile-fail)"
