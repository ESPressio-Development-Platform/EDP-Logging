#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export EDP_TYPED_SOURCE="$ROOT/tests/GeneratedEdlsBatchServiceIntegrationTests.cpp"
export EDP_TYPED_ACK_UBSAN="${EDP_TYPED_ACK_UBSAN:-1}"
exec bash "$ROOT/tests/run-generated-edls-strict-ack-task-integration-tests.sh"
