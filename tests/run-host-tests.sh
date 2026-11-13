#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EDP_BYTEOPS_DEPS="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
OUTPUT="${ROOT}/tests/.logging-sequence-tests"
trap 'rm -f "${OUTPUT}"' EXIT

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -I"${ROOT}/src" "${ROOT}/tests/AttemptSequenceTests.cpp" -o "${OUTPUT}"

"${OUTPUT}"

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -I"${ROOT}/src" -I"$EDP_BYTEOPS_DEPS/EDP-Memory/src" -I"$EDP_BYTEOPS_DEPS/EDP-Platform/src" -I"$EDP_BYTEOPS_DEPS/EDP-Platform-Portable/src" -I"$EDP_BYTEOPS_DEPS/EDP-System/src" -I"$EDP_BYTEOPS_DEPS/EDP-BoundedTopology/src" "${ROOT}/tests/EdlgFrameTests.cpp" -o "${OUTPUT}"
"${OUTPUT}"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -I"${ROOT}/src" "${ROOT}/tests/EdlsSegmentTests.cpp" -o "${OUTPUT}"
"${OUTPUT}"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -I"${ROOT}/src" "${ROOT}/tests/InstrumentationTests.cpp" -o "${OUTPUT}"
"${OUTPUT}"
echo "EDP-Logging C++20 foundation: PASS"
