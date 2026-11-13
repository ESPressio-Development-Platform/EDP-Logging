#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
CC="${1:-g++}"
OUT="$(mktemp)"
trap 'rm -f "$OUT"' EXIT
"$CC" -std=c++20 -pthread -Wall -Wextra -Wpedantic -Werror \
 -I"$ROOT/src" -I"$DEPS/EDP-Memory/src" -I"$DEPS/EDP-Platform-Portable/src" -I"$DEPS/EDP-BoundedTopology/src" -I"$DEPS/EDP-Persistence/src" \
 -I"$DEPS/EDP-System/src" -I"$DEPS/EDP-Platform/src" \
 "$ROOT/tests/PowerLossEdlsSnapshotsTests.cpp" -o "$OUT"
"$OUT"

for MODE in EDP_TEST_WEAK_RETENTION EDP_TEST_WEAK_ATOMICITY; do
  ERR="$(mktemp)"
  if "$CC" -std=c++20 -pthread -Wall -Wextra -Wpedantic -Werror \
      -D"$MODE" -I"$ROOT/src" -I"$DEPS/EDP-Memory/src" -I"$DEPS/EDP-Platform-Portable/src" -I"$DEPS/EDP-BoundedTopology/src" -I"$DEPS/EDP-Persistence/src" \
      -I"$DEPS/EDP-System/src" -I"$DEPS/EDP-Platform/src" \
      "$ROOT/tests/PowerLossEdlsSnapshotsTests.cpp" -o "$OUT" 2>"$ERR"; then
    echo "FAIL: accepted weak provider: $MODE" >&2
    rm -f "$ERR"
    exit 1
  fi
  if ! grep -q 'Strict EDLS snapshots require PowerLoss\|Strict EDLS snapshots reject non-PowerLoss-atomic' "$ERR"; then
    cat "$ERR" >&2
    rm -f "$ERR"
    echo "FAIL: wrong compile-fail reason: $MODE" >&2
    exit 1
  fi
  rm -f "$ERR"
done
echo "EDP-Logging PowerLoss KeyValueStorage EDLS snapshots and weak-provider rejection: PASS ($CC)"
