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
PASS=re.compile(r"EDP_LOAD\|PASS\|count=256\|ack=256\|window=255,256\|")

def control(command: str, *, expected_power: str|None=None) -> str:
    p=subprocess.run([CONTROL,command],text=True,capture_output=True,
        check=True,timeout=15)
    out=p.stdout.strip()
    print(f"HOST|{command}|{out}",flush=True)
    if expected_power is not None and f"POWER={expected_power}" not in out:
        raise RuntimeError("Incorrect hardware power state")
    return out

def main(directory: str) -> int:
    root=Path(directory)
    root.mkdir(parents=True,exist_ok=False)
    start=time.monotonic()
    with serial.Serial("/dev/serial0",115200,timeout=0.2) as uart:
        uart.reset_input_buffer()
        control("power-on",expected_power="ON")
        control("release")
        with (root/"serial.txt").open("w",encoding="utf-8") as transcript:
            while time.monotonic()-start<900:
                packet=uart.readline()
                if not packet:continue
                line=packet.decode("utf-8","replace").strip()
                transcript.write(line+"\n")
                transcript.flush()
                if "EDP_LOAD|" in line or "EDP_EDLS|HALT|" in line:
                    print("UART|"+line,flush=True)
                if "EDP_EDLS|HALT|" in line:
                    return 1
                if PASS.search(line):
                    print(f"HOST|LOAD_QUALIFICATION=PASS|elapsed_s={time.monotonic()-start:.3f}",
                          flush=True)
                    return 0
    print("HOST|LOAD_QUALIFICATION=FAIL|TIMEOUT",flush=True)
    return 2

if __name__=="__main__":
    try:
        sys.exit(main(sys.argv[1]))
    except Exception as exc:
        print(f"HOST|LOAD_QUALIFICATION=FAIL|{exc}",file=sys.stderr,flush=True)
        sys.exit(3)
