#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIBLINGS="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
CXX="${XTENSA_CXX:-xtensa-esp32-elf-g++}"
for repo in EDP-Memory EDP-Threading EDP-Clock EDP-System EDP-Platform EDP-BoundedTopology; do
  test -d "$SIBLINGS/$repo/src" || {
    echo "Missing required $repo source under $SIBLINGS" >&2
    exit 2
  }
done
"$CXX" -std=gnu++20 -Wall -Wextra -Wpedantic -Werror -fsyntax-only \
  -I"$ROOT/src" -I"$SIBLINGS/EDP-Memory/src" \
  -I"$SIBLINGS/EDP-Threading/src" -I"$SIBLINGS/EDP-Clock/src" \
  -I"$SIBLINGS/EDP-System/src" -I"$SIBLINGS/EDP-Platform/src" \
  -I"$SIBLINGS/EDP-BoundedTopology/src" \
  "$ROOT/tests/TransactionalRecordQueueTests.cpp"
echo "EDP-Logging ESP32 Xtensa C++20 queue and RAII compile-time contracts: PASS (syntax only)"
