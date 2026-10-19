#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd -P)"
DEPS="${EDP_LOGGING_DEPENDENCY_ROOT:?Provide pinned EDP owner dependency closure}"
for compiler in g++ clang++; do
    bin="$(mktemp)"
    trap 'rm -f "$bin"' EXIT
    "$compiler" -std=c++20 -pthread -Wall -Wextra -Wpedantic -Werror \
      -I"$ROOT/src" -I"$DEPS/EDP-Serialisation/src" \
      -I"$DEPS/EDP-System/src" -I"$DEPS/EDP-Clock/src" \
      -I"$DEPS/EDP-BoundedTypes/src" -I"$DEPS/EDP-Platform/src" \
      -I"$DEPS/EDP-Platform-Portable/src" \
      -I"$DEPS/EDP-Localisation/src" -I"$DEPS/EDP-Memory/src" \
      -I"$DEPS/EDP-BoundedTopology/src" -I"$DEPS/EDP-Threading/src" \
      "$ROOT/tests/G4ApprovedFieldWireTests.cpp" -o "$bin"
    "$bin"
    echo "G4_APPROVED_FIELD_WIRE|$compiler|PASS"
    rm -f "$bin"
done
