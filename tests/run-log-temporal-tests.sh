#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
OUTPUT="$ROOT/tests/.log-temporal-evidence-tests"
trap 'rm -f "$OUTPUT"' EXIT
for repo in EDP-Clock EDP-System EDP-Platform; do
    [[ -d "$WORKSPACE/$repo/src" ]] || {
        echo "Missing $repo source at $WORKSPACE" >&2
        exit 2
    }
done
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
    -I"$ROOT/src" \
    -I"$WORKSPACE/EDP-Clock/src" \
    -I"$WORKSPACE/EDP-System/src" \
    -I"$WORKSPACE/EDP-Platform/src" \
    "$ROOT/tests/LogTemporalEvidenceTests.cpp" -o "$OUTPUT"
"$OUTPUT"
echo "Coherent Clock temporal evidence qualification: PASS"
