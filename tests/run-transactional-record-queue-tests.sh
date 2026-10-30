#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIBLINGS="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
for repo in EDP-Memory EDP-Threading EDP-Clock EDP-Platform EDP-System EDP-BoundedTopology; do
  test -d "$SIBLINGS/$repo/src" || {
    echo "Missing pinned $repo source at $SIBLINGS" >&2
    exit 2
  }
done

OUTPUT="$(mktemp)"
trap 'rm -f "$OUTPUT"' EXIT
FLAGS=()
if test "${EDP_QUEUE_UBSAN:-0}" = 1; then FLAGS=(-fsanitize=undefined -fno-sanitize-recover=all); fi
for suite in TransactionalRecordQueueTests TransactionalRecordQueueMemoryTests TransactionalRecordQueueDeadlineTests; do
  "${CXX:-c++}" "${FLAGS[@]}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread \
    -I"$ROOT/src" -I"$ROOT/tests" \
    -I"$SIBLINGS/EDP-Memory" -I"$SIBLINGS/EDP-Memory/src" \
    -I"$SIBLINGS/EDP-Threading/src" -I"$SIBLINGS/EDP-Clock/src" \
    -I"$SIBLINGS/EDP-System/src" -I"$SIBLINGS/EDP-Platform/src" \
    -I"$SIBLINGS/EDP-BoundedTopology/src" \
    "$ROOT/tests/$suite.cpp" -o "$OUTPUT"
  "$OUTPUT"
  echo "EDP-Logging $suite: PASS"
done
echo "EDP-Logging transactional queue + real EDP-Memory integration: PASS"
