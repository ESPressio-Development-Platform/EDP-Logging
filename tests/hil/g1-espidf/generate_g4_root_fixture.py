#!/usr/bin/env python3
"""Generate one real EDP-Localisation diagnostic schema for Logging G4 HIL.

The output is a deterministic TEST FIXTURE, never an application static
manual replacement for the authoritative Localisation generator.
"""
import json
import sys
from pathlib import Path

def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_g4_root_fixture.py OWNER_LOCALISATION TARGET")
    owner = Path(sys.argv[1]).resolve()
    target = Path(sys.argv[2]).resolve()
    sys.path[:0] = [str(owner / "tools"), str(owner / "tools" / "tests")]
    from test_toolchain import ToolchainTests, write_json
    from edp_localisation.generator import compile_generated_set
    scratch = target.parent / "_g4_generated_input"
    scratch.mkdir(parents=True, exist_ok=False)
    source, platform, inventory = ToolchainTests().make_fixture(scratch)
    catalogue_path = source / "strings_catalogue.json"
    catalogue = json.loads(catalogue_path.read_text("utf-8"))
    # Only a real owner generated declared diagnostic message may certify
    # the typed root projected Field13 packet.
    catalogue["domains"][0]["subDomains"][0]["strings"][2]["parameters"] = [
        {"index": 0, "status": "active", "type": "runtime-incarnation-id",
         "symbol": "ObservedIncarnation"},
        {"index": 1, "status": "active", "type": "clock-era",
         "symbol": "ObservedEra"},
    ]
    write_json(catalogue_path, catalogue)
    compile_generated_set(source, platform, [inventory], target,
                          "Fixture::Diagnostic")
    assert (target / "GeneratedLocalisationContract.hpp").exists()
    print("G4_ROOT_GENERATED_SCHEMA=PASS")

if __name__ == "__main__":
    main()
