#!/usr/bin/env python3
"""RPI400 GPIO17 relay-controlled interruption of dedicated Logging G7 HIL.

This is an empirical relay interruption, not a guarantee of electrical VDD=0:
RPI400 signal lines can parasitically power ESP32 unless separately measured.
Never formats SPIFFS or erases any existing keys/files.
"""
import pathlib
import serial
import subprocess
import sys
import time
from datetime import datetime, timezone

CONTROLLER = "/srv/espressio/bin/esp32-integrated"
MARKER = "EDP_TORN|"

def action(name):
    completed = subprocess.run(
        [CONTROLLER, name],
        check=True, capture_output=True, text=True, timeout=8
    )
    print(f"HOST|{datetime.now(timezone.utc).isoformat()}|{completed.stdout.strip()}", flush=True)
    return completed.stdout.strip()

def main():
    if len(sys.argv) != 2:
        raise RuntimeError("usage: relay_cut_monitor.py EVIDENCE_DIRECTORY")
    out = pathlib.Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=False)
    action("status")
    # V2 requires an explicit power-on preflight immediately before UART open.
    # Controller idempotently leaves an already powered, reset-held MCU alone.
    action("power-on")
    # The flashing script has placed EN in reset and left the UART free.
    started = time.monotonic()
    marker_lines = []
    current = bytearray()
    cut = False
    second_boot = False
    recovery = False
    native_return_seen = False
    uart = serial.Serial("/dev/serial0", 115200, timeout=0.10)
    try:
        uart.reset_input_buffer()
        with (out / "serial.raw").open("wb") as binary, (out / "serial.txt").open("w", encoding="utf-8") as decoded:
            action("release")
            while time.monotonic() - started < 80.0:
                data = uart.read(4096)
                if not data:
                    continue
                binary.write(data)
                binary.flush()
                for byte in data:
                    if byte == 10 or len(current) >= 4095:
                        line = current.decode("utf-8", errors="replace").strip()
                        current.clear()
                        if line:
                            decoded.write(line + "\n")
                            decoded.flush()
                            if MARKER in line:
                                marker = line[line.index(MARKER):]
                                marker_lines.append(marker)
                                print(f"UART|{time.monotonic()-started:.4f}|{marker}", flush=True)
                                if "|PARTIAL_WRITE_RETURN|" in marker:
                                    native_return_seen = True
                                if "|PARTIAL_PUBLISHED" in marker and not cut:
                                    # Cut relay 1 immediately after receipt of the native
                                    # ReplaceFile entry marker, without issuing ESP32 reset.
                                    cut = True
                                    print("HOST|RELAY_CUT_TRIGGER=PARTIAL_PUBLISHED", flush=True)
                                    action("power-off")
                                    off_start = time.monotonic()
                                    print(f"HOST|POWER_OFF_MONOTONIC={off_start:.6f}", flush=True)
                                    time.sleep(1.4)
                                    action("status")
                                    action("power-on")
                                    print(f"HOST|REENERGISED_AFTER_S={time.monotonic()-off_start:.4f}", flush=True)
                                if cut and "|BOOT|" in marker:
                                    second_boot = True
                                if cut and "|RECOVERY|" in marker:
                                    recovery = True
                                if cut and "|PASS|PROTECTED_GENERATION_SURVIVED_AND_FAILED_CLOSED" in marker:
                                    print("HOST|RELAY_CUT_RECOVERY_PRESERVED=PASS", flush=True)
                                    print(f"HOST|RECOVERY_SECOND_BOOT_SEEN={second_boot}", flush=True)
                                    print(f"HOST|NATIVE_RETURN_BEFORE_CUT={native_return_seen}", flush=True)
                                    return 0 if second_boot and not native_return_seen else 3
                                if cut and ("|HALT|" in marker or "|RECOVERY|MOUNT_FAILED" in marker):
                                    print("HOST|RELAY_CUT_RECOVERY=FAIL", flush=True)
                                    return 4
                        continue
                    current.append(byte)
        print(f"HOST|TIMEOUT|cut={cut}|recovery={recovery}|second_boot={second_boot}", flush=True)
        return 5
    finally:
        uart.close()
        # Unconditionally switch off when the active test is complete.
        action("power-off")

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"HOST|ERROR|{type(exc).__name__}: {exc}", file=sys.stderr, flush=True)
        raise SystemExit(9)
