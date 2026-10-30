#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
OUT="$(mktemp)"
trap 'rm -f "$OUT"' EXIT
FLAGS=()
if test "${EDP_BATCH_UBSAN:-0}" = 1; then
 FLAGS=(-fsanitize=undefined -fno-sanitize-recover=all)
fi
INCLUDES=(-I"$ROOT/src" -I"$ROOT/tests")
for repo in EDP-System EDP-Platform EDP-Memory EDP-Platform-Portable EDP-Clock EDP-BoundedTypes EDP-BoundedTopology EDP-Persistence EDP-Persistence-ESP-IDF; do
 INCLUDES+=("-I$DEPS/$repo/src")
done
"${CXX:-g++}" -std=c++20 -pthread -Wall -Wextra -Wpedantic -Werror "${FLAGS[@]}" "${INCLUDES[@]}" "$ROOT/tests/BoundedEdlsBatchTests.cpp" -o "$OUT"
"$OUT"
echo "EDP-Logging sealed bounded batching PASS (${CXX:-g++})"
