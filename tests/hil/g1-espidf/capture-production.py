#!/usr/bin/env python3
"""Independent physical UART HIL capture + canonical EDLG framing verification."""
from pathlib import Path
from io import BytesIO
import sys
import time
import serial

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from edp_logging.framing import Frame, FrameCodec, scan_stream

output = Path(sys.argv[1])
expect = (
  "EDP_LOGGING_HIL|START|G1_WROOM_REAL_EDP_PROVIDERS",
  "EDP_LOGGING_HIL|CLOCK|CANONICAL_1MHZ",
  "EDP_LOGGING_HIL|PASS|FIFO_MEMORY_OWNERSHIP",
  "EDP_LOGGING_HIL|PASS|DUAL_CORE_PRODUCER_WAKE",
  "EDP_LOGGING_HIL|PASS|STANDALONE_CLOCK_PRODUCER_CBOR_EDLG",
  "EDP_LOGGING_HIL|PASS|SINK_REJECTION_ACCOUNTING",
  "EDP_LOGGING_HIL|PASS|DRAIN_SLOT_ISOLATION",
  "EDP_LOGGING_HIL|PASS|SHUTDOWN_AND_TEARDOWN",
  "EDP_LOGGING_HIL|RESULT|PASS",
)
all_lines = []
try:
  with serial.Serial("/dev/serial0",115200,timeout=0.3) as uart:
    print("EDP_LOGGING_PHYSICAL_CAPTURE_READY",flush=True)
    deadline=time.monotonic()+55
    while time.monotonic()<deadline:
      line=uart.readline().decode("utf-8",errors="replace").strip()
      if line:
        all_lines.append(line)
        print(line,flush=True)
        if "EDP_LOGGING_HIL|RESULT|" in line:break
finally:
  output.write_text("\n".join(all_lines)+"\n",encoding="utf-8")
matched=[e for e in expect if any(e in l for l in all_lines)]
failed=[l for l in all_lines if "EDP_LOGGING_HIL|FAIL|" in l]
wire_lines=[l for l in all_lines if l.startswith("EDP_LOGGING_HIL|CBOR_EDLG_HEX|")]
wire_ok=False
if len(wire_lines)==1:
  try:
    wire=bytes.fromhex(wire_lines[0].split("|",2)[2])
    frames=list(scan_stream(BytesIO(wire),max_payload=96))
    if len(frames)==1 and isinstance(frames[0],Frame):
      f=frames[0]
      # HIL payload is a TEST-ONLY positional CBOR array: ticket=1, sample
      # monotonic timestamp, value=515. It is NOT a governed LogRecord.
      wire_ok=(f.codec==FrameCodec.Cbor and
               len(f.payload)>5 and f.payload.startswith(bytes((0x83,0x01))) and
               f.payload.endswith(bytes((0x19,0x02,0x03))))
  except (ValueError,AssertionError) as error:
    print("EDP_LOGGING_HIL_FRAME_PARSE_FAILURE",repr(error),flush=True)
print(f"EDP_LOGGING_HIL_VERIFY|markers={len(matched)}/{len(expect)}|"
      f"fail={len(failed)}|edlg_crc_cbor={wire_ok}",flush=True)
sys.exit(0 if len(matched)==len(expect) and not failed and wire_ok else 1)
