#!/usr/bin/env python3
"""Verify actual native O2 arguments and retained assertions before physical use."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex

parser = argparse.ArgumentParser()
parser.add_argument("--database", required=True, type=Path)
parser.add_argument("--sdkconfig", required=True, type=Path)
parser.add_argument("--output", required=True, type=Path)
args = parser.parse_args()
try:
    entries = json.loads(args.database.read_text("utf-8"))
    matches = [row for row in entries if Path(row["file"]).as_posix() == "src/main.cpp"]
    if len(matches) != 1:
        raise ValueError("require exactly one actual application src/main.cpp command")
    command = matches[0].get("arguments") or shlex.split(matches[0]["command"])
    optimisations = [value for value in command if value.startswith("-O")]
    if not optimisations or any(value != "-O2" for value in optimisations):
        raise ValueError(f"actual application optimisation is not exclusively O2: {optimisations}")
    if any(value.startswith("-DNDEBUG") for value in command):
        raise ValueError("NDEBUG would disable checks")
    required = {"-DEDP_TEST_BATCH_WORKER", "-DEDP_WORKER_DWELL_NS=500000000LL",
                "-DEDP_WORKER_OPTIMISED_BUILD=1"}
    if not required.issubset(command):
        raise ValueError("wrong managed worker/dwell/optimisation configuration")
    config = set(args.sdkconfig.read_text("utf-8").splitlines())
    if "CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE=y" not in config or \
            "CONFIG_COMPILER_OPTIMIZATION_ASSERTION_LEVEL=2" not in config:
        raise ValueError("native assertion checks must remain enabled")
    if "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=160" not in config:
        raise ValueError("controlled O2 experiment requires unchanged160MHz CPU")
    report = {"status": "Verified", "application_optimisation": "O2",
              "optimisation_flags": optimisations, "assertions": "enabled-level2",
              "cpu_mhz": 160, "dwell_ns": 500000000,
              "sdkconfig_sha256": hashlib.sha256(args.sdkconfig.read_bytes()).hexdigest()}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", "utf-8")
except (OSError, ValueError, KeyError, TypeError) as error:
    raise SystemExit(f"NATIVE_COMPILE_PROFILE=FAIL reason={error}")
print("NATIVE_COMPILE_PROFILE=PASS optimisation=O2 assertions=enabled-level2 cpu_mhz=160 dwell_ns=500000000")
