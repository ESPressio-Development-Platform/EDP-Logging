#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
OUTPUT="$ROOT/tests/.bounded-admission-tests"
trap 'rm -f "$OUTPUT"' EXIT

for name in EDP-Clock EDP-Memory EDP-Threading EDP-Platform EDP-System EDP-BoundedTopology; do
  if [[ ! -d "$WORKSPACE/$name/src" ]]; then
    echo "ERROR: Missing required optional G1 test sibling $name at $WORKSPACE" >&2
    exit 2
  fi
done
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread \
  -I"$ROOT/src" \
  -I"$WORKSPACE/EDP-Clock/src" \
  -I"$WORKSPACE/EDP-Memory/src" \
  -I"$WORKSPACE/EDP-Threading/src" \
  -I"$WORKSPACE/EDP-Platform/src" \
  -I"$WORKSPACE/EDP-System/src" \
  -I"$WORKSPACE/EDP-BoundedTopology/src" \
  "$ROOT/tests/BoundedAdmissionTests.cpp" -o "$OUTPUT"
"$OUTPUT"
echo "EDP-Logging optional G1 bounded-admission foundation: PASS"
