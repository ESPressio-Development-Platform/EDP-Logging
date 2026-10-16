#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
OUTPUT="$ROOT/tests/.composed-admission-$(basename "${CXX:-c++}")"
trap 'rm -f "$OUTPUT"' EXIT
for repo in EDP-Memory EDP-Threading EDP-Clock EDP-Platform EDP-System EDP-BoundedTopology; do
    if [[ ! -d "$WORKSPACE/$repo/src" ]]; then
        echo "Missing required EDP contract sibling $repo in $WORKSPACE" >&2
        exit 2
    fi
done
"${CXX:-c++}" -std=c++20 -pthread -Wall -Wextra -Wpedantic -Werror \
    -I"$ROOT/src" \
    -I"$WORKSPACE/EDP-Memory" \
    -I"$WORKSPACE/EDP-Memory/src" \
    -I"$WORKSPACE/EDP-Threading/src" \
    -I"$WORKSPACE/EDP-Clock/src" \
    -I"$WORKSPACE/EDP-Platform/src" \
    -I"$WORKSPACE/EDP-System/src" \
    -I"$WORKSPACE/EDP-BoundedTopology/src" \
    "$ROOT/tests/AdmissionWakeCoordinatorTests.cpp" \
    -o "$OUTPUT"
"$OUTPUT"
echo "Composed EDP-Memory + EDP-Threading multi-producer admission: PASS"
