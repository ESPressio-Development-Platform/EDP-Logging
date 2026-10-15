#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT="${ROOT}/tests/.logging-sequence-tests"
trap 'rm -f "${OUTPUT}"' EXIT

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -I"${ROOT}/src" "${ROOT}/tests/AttemptSequenceTests.cpp" -o "${OUTPUT}"

"${OUTPUT}"

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -I"${ROOT}/src" "${ROOT}/tests/EdlgFrameTests.cpp" -o "${OUTPUT}"
"${OUTPUT}"
echo "EDP-Logging C++20 foundation: PASS"
