#!/usr/bin/env python3
"""Test-only bounded UART receipt monitor. GPIO17 is driven ONLY by appliance script."""
from pathlib import Path
import os
import re
import subprocess
import sys
import time
import serial

CONTROL="/srv/espressio/bin/esp32-integrated"
PATTERNS={
    "typed-managed-batch-demo": re.compile(r"EDP_TYPED_WORKER_DEMO\|PASS\|incarnation=[1-9][0-9]*\|records=9\|durable=9\|retired=9\|ack_segment=[1-9][0-9]*\|retained_records=5\|dwell_ns=[0-9]+$"),
    "batch-worker-500-o2-6000": re.compile(r"EDP_WORKER\|PASS\|scheduled_records=6000\|producers=2\|target_rps=100\|[^\n]*\|scheduled_dwell_ns=500000000\|application_optimisation=o2\|cpu_mhz=160\|"),
    "configurable-batch-demo": re.compile(r"EDP_BATCH_DEMO\|PASS\|incarnation=[1-9][0-9]*\|dwell_ns=[0-9]+\|full_records=4\|idle_records=1\|ack_segment=[1-9][0-9]*$"),
    "batch-worker-500-o2": re.compile(r"EDP_WORKER\|PASS\|scheduled_records=2000\|producers=2\|target_rps=100\|[^\n]*\|scheduled_dwell_ns=500000000\|application_optimisation=o2\|cpu_mhz=160\|"),
    "batch-worker-500": re.compile(r"EDP_WORKER\|PASS\|scheduled_records=2000\|producers=2\|target_rps=100\|[^\n]*\|scheduled_dwell_ns=500000000\|"),
    "batch-worker": re.compile(r"EDP_WORKER\|PASS\|scheduled_records=2000\|producers=2\|target_rps=100\|"),
    "single-record": re.compile(r"EDP_PROFILE\|PASS\|count=256\|ack=256\|window=255,256\|"),
    "sealed-batch": re.compile(r"EDP_BATCH\|PASS\|records=256\|ack_segment=16\|window=15,16\|first=225\|last=256\|"),
    "sealed-batch-64": re.compile(r"EDP_BATCH\|PASS\|records=256\|ack_segment=4\|window=3,4\|first=129\|last=256\|"),
}

def control(command: str, *, expected_power: str|None=None) -> str:
    p=subprocess.run([CONTROL,command],text=True,capture_output=True,
        check=True,timeout=15)
    out=p.stdout.strip()
    print(f"HOST|{command}|{out}",flush=True)
    if expected_power is not None and f"POWER={expected_power}" not in out:
        raise RuntimeError("Incorrect hardware power state")
    return out

def main(directory: str) -> int:
    pattern=PATTERNS[os.environ.get("EDP_PROFILE_KIND","single-record")]
    root=Path(directory)
    root.mkdir(parents=True,exist_ok=False)
    start=time.monotonic()
    control("power-on",expected_power="ON")
    with serial.Serial("/dev/serial0",115200,timeout=0.2) as uart:
        uart.reset_input_buffer()
        control("release")
        with (root/"serial.txt").open("w",encoding="utf-8") as transcript:
            while time.monotonic()-start<900:
                packet=uart.readline()
                if not packet:continue
                line=packet.decode("utf-8","replace").strip()
                transcript.write(line+"\n")
                transcript.flush()
                if "EDP_PROFILE|" in line or "EDP_BATCH|" in line or "EDP_WORKER|" in line or "EDP_EDLS|HALT|" in line or "EDP_BATCH_DEMO|" in line or "EDP_TYPED_WORKER_DEMO|" in line:
                    print("UART|"+line,flush=True)
                if "EDP_EDLS|HALT|" in line or "EDP_BATCH_DEMO|FAIL|" in line or "EDP_TYPED_WORKER_DEMO|FAIL|" in line:
                    return 1
                if pattern.search(line):
                    print(f"HOST|PROFILE_QUALIFICATION=PASS|elapsed_s={time.monotonic()-start:.3f}",
                          flush=True)
                    return 0
    print("HOST|PROFILE_QUALIFICATION=FAIL|TIMEOUT",flush=True)
    return 2

if __name__=="__main__":
    try:
        sys.exit(main(sys.argv[1]))
    except Exception as exc:
        print(f"HOST|PROFILE_QUALIFICATION=FAIL|{exc}",file=sys.stderr,flush=True)
        sys.exit(3)
