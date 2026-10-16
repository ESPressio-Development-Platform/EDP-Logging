#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
OUTPUT="$ROOT/tests/.diagnostic-argument-tests"
trap 'rm -f "$OUTPUT"' EXIT
for repo in EDP-Serialisation EDP-System EDP-BoundedTypes EDP-BoundedTopology EDP-Memory EDP-Platform EDP-Platform-Portable EDP-Localisation; do
    test -d "$WORKSPACE/$repo/src" || { echo "Missing $repo" >&2; exit 2; }
done
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
    -I"$ROOT/src" \
    -I"$WORKSPACE/EDP-Serialisation/src" \
    -I"$WORKSPACE/EDP-System/src" \
    -I"$WORKSPACE/EDP-BoundedTypes/src" \
    -I"$WORKSPACE/EDP-BoundedTopology/src" \
    -I"$WORKSPACE/EDP-Memory/src" \
    -I"$WORKSPACE/EDP-Platform/src" \
    -I"$WORKSPACE/EDP-Platform-Portable/src" \
    -I"$WORKSPACE/EDP-Localisation/src" \
    "$ROOT/tests/DiagnosticArgumentEncodingTests.cpp" -o "$OUTPUT"
"$OUTPUT"
echo "G3 diagnostic generated-schema-shape argument encoding: PASS"
