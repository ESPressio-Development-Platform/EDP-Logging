#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
TEMP="$(mktemp -d)"
trap 'rm -rf "$TEMP"' EXIT
for repo in EDP-Serialisation EDP-Localisation EDP-Clock EDP-System EDP-BoundedTypes EDP-BoundedTopology EDP-Memory EDP-Platform EDP-Platform-Portable EDP-Persistence EDP-Persistence-ESP-IDF EDP-Threading; do
    test -d "$WORKSPACE/$repo/src" || { echo "Missing $repo" >&2; exit 2; }
done
EDP_LOCALISATION_TEST_ROOT="$WORKSPACE/EDP-Localisation" EDP_TEMP="$TEMP" python3 - <<'PY'
import json, os, sys
from pathlib import Path
localisation=Path(os.environ["EDP_LOCALISATION_TEST_ROOT"])
tmp=Path(os.environ["EDP_TEMP"])
sys.path[:0]=[str(localisation/"tools"),str(localisation/"tools"/"tests")]
from test_toolchain import ToolchainTests,write_json
from edp_localisation.generator import compile_generated_set
source,platform,inventory=ToolchainTests().make_fixture(tmp)
catalogue=source/"strings_catalogue.json"
data=json.loads(catalogue.read_text("utf-8"))
data["domains"][0]["subDomains"][0]["strings"][2]["parameters"]=[
    {"index":0,"status":"active","type":"runtime-incarnation-id","symbol":"Incarnation"},
    {"index":1,"status":"active","type":"clock-era","symbol":"Era"}
]
write_json(catalogue,data)
compile_generated_set(source,platform,[inventory],tmp/"generated","Fixture::Diagnostic")
PY

COMPILER="${CXX:-g++}"
FLAGS=()
if test "${EDP_TYPED_ACK_UBSAN:-0}" = 1; then
    FLAGS=(-fsanitize=undefined -fno-sanitize-recover=all)
fi
"$COMPILER" "${FLAGS[@]}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread \
  -Wno-unused-function -I"$ROOT/src" -I"$ROOT/tests" -I"$TEMP/generated" \
  -I"$WORKSPACE/EDP-Serialisation/src" \
  -I"$WORKSPACE/EDP-Localisation/src" \
  -I"$WORKSPACE/EDP-Clock/src" \
  -I"$WORKSPACE/EDP-System/src" \
  -I"$WORKSPACE/EDP-BoundedTypes/src" \
  -I"$WORKSPACE/EDP-BoundedTopology/src" \
  -I"$WORKSPACE/EDP-Memory" -I"$WORKSPACE/EDP-Memory/src" \
  -I"$WORKSPACE/EDP-Memory" \
  -I"$WORKSPACE/EDP-Platform/src" \
  -I"$WORKSPACE/EDP-Platform-Portable/src" \
  -I"$WORKSPACE/EDP-Persistence/src" \
  -I"$WORKSPACE/EDP-Persistence-ESP-IDF/src" \
  -I"$WORKSPACE/EDP-Threading/src" \
  "${EDP_TYPED_SOURCE:-$ROOT/tests/GeneratedEdlsStrictAckTaskIntegrationTests.cpp}" \
  -o "$TEMP/test"
"$TEMP/test"
echo "Existing Memory + Threading Task -> typed CBOR -> durable NVS-ACKed EDLS PASS ($COMPILER)"
