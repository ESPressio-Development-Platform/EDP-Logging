#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
CC="${1:-g++}"
OUT="$(mktemp)"
trap 'rm -f "$OUT"' EXIT
"$CC" -std=c++20 -pthread -Wall -Wextra -Wpedantic -Werror \
 -I"$ROOT/src" -I"$DEPS/EDP-Persistence/src" \
 -I"$DEPS/EDP-System/src" -I"$DEPS/EDP-Platform/src" \
 "$ROOT/tests/PowerLossEdlsSnapshotsTests.cpp" -o "$OUT"
"$OUT"
echo "EDP-Logging PowerLoss KeyValueStorage EDLS snapshots: PASS ($CC)"
