#!/usr/bin/env python3
"""Generate test-only full diagnostic schema through EDP-Localisation owner.

Deterministic schema for native ESP32 HIL, same as host integration runner.
The generated result is ignored source output, not checked-in generated code.
"""
import argparse
import json
import sys
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument("--localisation",type=Path,required=True)
parser.add_argument("--output",type=Path,required=True)
args=parser.parse_args()
sys.path[:0]=[
    str(args.localisation/"tools"),
    str(args.localisation/"tools"/"tests"),
]
from test_toolchain import ToolchainTests,write_json
from edp_localisation.generator import compile_generated_set
import tempfile

with tempfile.TemporaryDirectory(prefix="edp-strict-typed-gen-") as temporary:
    fixture=Path(temporary)
    source,platform,inventory=ToolchainTests().make_fixture(fixture)
    catalog=source/"strings_catalogue.json"
    data=json.loads(catalog.read_text(encoding="utf-8"))
    data["domains"][0]["subDomains"][0]["strings"][2]["parameters"]=[
        {"index":0,"status":"active","type":"runtime-incarnation-id","symbol":"Incarnation"},
        {"index":1,"status":"active","type":"clock-era","symbol":"Era"},
    ]
    write_json(catalog,data)
    compile_generated_set(source,platform,[inventory],args.output,"Fixture::Diagnostic")
    contract=args.output/"GeneratedLocalisationContract.hpp"
    if not contract.is_file():
        raise RuntimeError("EDP-Localisation omitted the required test generated header")
    print(f"GENERATED_TYPED_CONTRACT=PASS {contract}",flush=True)
