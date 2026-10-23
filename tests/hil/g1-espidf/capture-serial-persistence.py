#!/usr/bin/env python3
"""Actual UART HIL: one ESP32 NVS-committed restart and owner Serial TX frame."""
from pathlib import Path
from io import BytesIO
import sys
import time
import os
import serial
from g11_site_evidence import validate_site_markers

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from edp_logging.framing import Frame, FrameCodec, scan_stream

output=Path(sys.argv[1])
expected=(
    "EDP_LOGGING_HIL|START|G1_WROOM_REAL_EDP_PROVIDERS",
    "EDP_LOGGING_HIL|PASS|PERSISTENCE_COMMITTED_BEFORE_RESTART",
    "EDP_LOGGING_HIL|PASS|PERSISTENCE_RESTART_RECOVERY",
    "EDP_LOGGING_HIL|PASS|NATIVE_EDLS_VFS_SUCCESSOR_GUARD",
    "EDP_LOGGING_HIL|PASS|GOVERNED_LOG_RECORD_VALIDATION",
    "EDP_LOGGING_HIL|PASS|BOUNDED_COORDINATOR_QUANTUM",
    "EDP_LOGGING_HIL|PASS|EDP_THREADING_DEDICATED_COORDINATOR",
    "EDP_LOGGING_HIL|PASS|MEMORY_OWNED_ENCODING_SERIAL_COPY",
    "EDP_LOGGING_HIL|PASS|G10_ASYNC_TASK_OWNED_WORKER",
    "EDP_LOGGING_HIL|PASS|WROOM_WHOLE_APP_RESOURCE_SNAPSHOT",
    "EDP_LOGGING_HIL|PASS|FIFO_MEMORY_OWNERSHIP",
    "EDP_LOGGING_HIL|PASS|DUAL_CORE_PRODUCER_WAKE",
    "EDP_LOGGING_HIL|PASS|STANDALONE_CLOCK_PRODUCER_CBOR_EDLG",
    "EDP_LOGGING_HIL|PASS|SINK_REJECTION_ACCOUNTING",
    "EDP_LOGGING_HIL|PASS|MULTI_SINK_NATIVE_RECLAMATION",
    "EDP_LOGGING_HIL|PASS|DRAIN_SLOT_ISOLATION",
    "EDP_LOGGING_HIL|PASS|SHUTDOWN_AND_TEARDOWN",
    "EDP_LOGGING_HIL|PASS|SERIAL_OWNER_NATIVE_UART_EDLG",
    "EDP_LOGGING_HIL|RESULT|PASS",
)
all_lines=[]
wire_received=None
frame_verified=False
try:
    with serial.Serial("/dev/serial0",115200,timeout=0.5) as uart:
        print("EDP_LOGGING_NATIVE_HIL_CAPTURE_READY",flush=True)
        stop=time.monotonic()+75
        while time.monotonic()<stop:
            text=uart.readline().decode("utf-8",errors="replace").strip()
            if not text:
                continue
            all_lines.append(text)
            print(text,flush=True)
            if text.startswith("EDP_LOGGING_HIL|SERIAL_BINARY_START|"):
                exact=int(text.rsplit("|",1)[1])
                wire=bytearray()
                deadline=time.monotonic()+10
                while len(wire)<exact and time.monotonic()<deadline:
                    wire.extend(uart.read(exact-len(wire)))
                wire_received=bytes(wire)
                print("EDP_LOGGING_NATIVE_HIL_WIRE_HEX|"+wire_received.hex(),flush=True)
                frames=list(scan_stream(BytesIO(wire_received),max_payload=32))
                frame_verified=(len(wire_received)==exact and len(frames)==1 and
                                isinstance(frames[0],Frame) and
                                frames[0].codec==FrameCodec.Cbor and
                                frames[0].payload==bytes((0x83,0x01,0x02,0x03)))
                print("EDP_LOGGING_NATIVE_HIL_FRAME_CRC_VALID|"+str(frame_verified),flush=True)
            if "EDP_LOGGING_HIL|RESULT|" in text:
                break
finally:
    output.write_text("\n".join(all_lines)+"\n"+(
        "HIL_WIRE="+wire_received.hex()+"\n" if wire_received is not None else ""
    ),encoding="utf-8")
# The resource snapshot records observed whole-application measurements;
# it is not a fabricated per-Logging-library CPU/memory budget.
resource_lines=[
    line for line in all_lines if line.startswith("EDP_LOGGING_HIL|RESOURCE|")
]
resource_valid=False
if len(resource_lines)==1:
    fields=dict(
        part.split("=",1) for part in resource_lines[0].split("|")[2:]
        if "=" in part
    )
    names=(
        "free8_before","free8_after","free8_min",
        "app_main_stack_margin_bytes",
        "dedicated_worker_stack_margin_bytes","elapsed_us",
        "static_record_bytes","static_record_queue_bytes"
    )
    try:
        values={name:int(fields[name]) for name in names}
        resource_valid=(
            fields.get("app_scope")=="whole_test_bundle"
            and all(values[name]>0 for name in names)
            and values["free8_min"]<=values["free8_after"]
            and values["free8_min"]<=values["free8_before"]
            and values["dedicated_worker_stack_margin_bytes"]<=6144
        )
    except (KeyError, ValueError):
        resource_valid=False
# The G11 test-site assertion is deliberately separate from the existing
# nineteen canonical functional HIL assertions, and must occur exactly once.
site_mode=os.environ.get("EDP_LOGGING_G11_APP_SITE_ENABLED","0")
site_verified=(
    site_mode in ("0","1")
    and validate_site_markers(all_lines,int(site_mode),2)
)
matched=[e for e in expected if any(e in l for l in all_lines)]
failed=[l for l in all_lines if "EDP_LOGGING_HIL|FAIL|" in l]
print(f"EDP_LOGGING_NATIVE_HIL_SUMMARY|markers={len(matched)}/{len(expected)}|"
      f"fail={len(failed)}|binary_edlg_verified={frame_verified}|"
      f"resource_snapshot_verified={resource_valid}|g11_site_verified={site_verified}",flush=True)
sys.exit(0 if len(matched)==len(expected) and not failed
         and frame_verified and resource_valid and site_verified else 1)
