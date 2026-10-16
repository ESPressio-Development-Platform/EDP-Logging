#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIBLINGS="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
for repo in EDP-Memory EDP-Threading EDP-Clock EDP-Platform EDP-System EDP-BoundedTopology; do
  test -d "$SIBLINGS/$repo/src" || { echo "Missing $repo under $SIBLINGS" >&2; exit 2; }
done
OUTPUT="$(mktemp)"
trap 'rm -f "$OUTPUT"' EXIT
"${CXX:-c++}" -std=c++20 -pthread -Wall -Wextra -Wpedantic -Werror \
  -I"$ROOT/src" -I"$ROOT/tests" \
  -I"$SIBLINGS/EDP-Memory" -I"$SIBLINGS/EDP-Memory/src" \
  -I"$SIBLINGS/EDP-Threading/src" -I"$SIBLINGS/EDP-Clock/src" \
  -I"$SIBLINGS/EDP-Platform/src" -I"$SIBLINGS/EDP-System/src" \
  -I"$SIBLINGS/EDP-BoundedTopology/src" \
  "$ROOT/tests/BoundedRecordAdmissionTests.cpp" -o "$OUTPUT"
"$OUTPUT"
echo "EDP-Logging bounded record admission Memory/Threading/queue: PASS"
