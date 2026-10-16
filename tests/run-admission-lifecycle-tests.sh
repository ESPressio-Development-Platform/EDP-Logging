#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
OUTPUT="$ROOT/tests/.admission-lifecycle-tests"
trap 'rm -f "$OUTPUT"' EXIT

for repo in EDP-Memory EDP-Threading EDP-Clock EDP-Platform EDP-System EDP-BoundedTopology; do
    test -d "$WORKSPACE/$repo/src" || { echo "Missing $repo dependency" >&2; exit 2; }
done

"${CXX:-c++}" -std=c++20 -pthread -Wall -Wextra -Wpedantic -Werror \
    -I"$ROOT/src" \
    -I"$ROOT/tests" \
    -I"$WORKSPACE/EDP-Memory" \
    -I"$WORKSPACE/EDP-Memory/src" \
    -I"$WORKSPACE/EDP-Threading/src" \
    -I"$WORKSPACE/EDP-Clock/src" \
    -I"$WORKSPACE/EDP-Platform/src" \
    -I"$WORKSPACE/EDP-System/src" \
    -I"$WORKSPACE/EDP-BoundedTopology/src" \
    "$ROOT/tests/AdmissionLifecycleTests.cpp" -o "$OUTPUT"
"$OUTPUT"
echo "EDP-Logging composed G1 cancellation and admission quiescence: PASS"
