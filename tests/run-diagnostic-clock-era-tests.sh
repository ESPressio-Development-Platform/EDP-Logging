#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$ROOT/.." && pwd)}"
TEMP="$(mktemp -d)"
trap 'rm -rf "$TEMP"' EXIT
for repo in EDP-Serialisation EDP-Localisation EDP-Clock EDP-System EDP-BoundedTypes EDP-BoundedTopology EDP-Memory EDP-Platform EDP-Platform-Portable; do
    test -d "$WORKSPACE/$repo/src" || { echo "Missing $repo" >&2; exit 2; }
done

EDP_LOCALISATION_TEST_ROOT="$WORKSPACE/EDP-Localisation" EDP_TEMP="$TEMP" python3 - <<'PY'
import json, os, sys
from pathlib import Path
localisation=Path(os.environ['EDP_LOCALISATION_TEST_ROOT'])
tmp=Path(os.environ['EDP_TEMP'])
sys.path[:0]=[str(localisation/'tools'),str(localisation/'tools'/'tests')]
from test_toolchain import ToolchainTests,write_json
from edp_localisation.generator import compile_generated_set
source,platform,inventory=ToolchainTests().make_fixture(tmp)
catalogue_path=source/'strings_catalogue.json'
catalogue=json.loads(catalogue_path.read_text('utf-8'))
catalogue['domains'][0]['subDomains'][0]['strings'][0]['parameters']=[
    {'index':0,'status':'active','type':'clock-era','symbol':'Era'},
]
write_json(catalogue_path,catalogue)
compile_generated_set(source,platform,[inventory],tmp/'generated','Fixture::Diagnostic')
PY

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
    -I"$ROOT/src" -I"$TEMP/generated" \
    -I"$WORKSPACE/EDP-Serialisation/src" \
    -I"$WORKSPACE/EDP-Localisation/src" \
    -I"$WORKSPACE/EDP-Clock/src" \
    -I"$WORKSPACE/EDP-System/src" \
    -I"$WORKSPACE/EDP-BoundedTypes/src" \
    -I"$WORKSPACE/EDP-BoundedTopology/src" \
    -I"$WORKSPACE/EDP-Memory/src" \
    -I"$WORKSPACE/EDP-Platform/src" \
    -I"$WORKSPACE/EDP-Platform-Portable/src" \
    "$ROOT/tests/DiagnosticClockEraGeneratedTests.cpp" \
    -o "$TEMP/clock-era-contract"
"$TEMP/clock-era-contract"
if "${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
    -I"$ROOT/src" -I"$TEMP/generated" \
    -I"$WORKSPACE/EDP-Serialisation/src" \
    -I"$WORKSPACE/EDP-Localisation/src" \
    -I"$WORKSPACE/EDP-Clock/src" \
    -I"$WORKSPACE/EDP-System/src" \
    -I"$WORKSPACE/EDP-BoundedTypes/src" \
    -I"$WORKSPACE/EDP-BoundedTopology/src" \
    -I"$WORKSPACE/EDP-Memory/src" \
    -I"$WORKSPACE/EDP-Platform/src" \
    -I"$WORKSPACE/EDP-Platform-Portable/src" \
    -fsyntax-only "$ROOT/tests/CompileFailDiagnosticBoundedWrongKind.cpp" \
    >"$TEMP/negative.log" 2>&1; then
    echo "ERROR: incorrect ClockEra argument types unexpectedly compiled" >&2
    exit 1
fi
grep -q "Diagnostic argument types/widths/count" "$TEMP/negative.log" || {
    cat "$TEMP/negative.log" >&2
    exit 1
}
echo "Real Localisation generated ClockEra diagnostic G3 schema CBOR+JSON integration PASS"
