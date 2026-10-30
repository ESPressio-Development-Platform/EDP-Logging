#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export EDP_RECORD_SOURCE="$ROOT/tests/CoordinatorSinkServiceTests.cpp"
export EDP_RECORD_UBSAN="${EDP_RECORD_UBSAN:-1}"
exec bash "$ROOT/tests/run-record-production-tests.sh" "${1:-g++}"
