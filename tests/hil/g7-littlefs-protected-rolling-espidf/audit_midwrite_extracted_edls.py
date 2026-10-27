#!/usr/bin/env python3
"""Independent physical media audit after cutting sustained unacknowledged writes.

Full EDLS/EDLG integrity, retained acknowledgement and physical names are
inspected without replaying the firmware's own UART claims or repairing media.
An unacknowledged candidate, if any, is NEVER accepted as a retained log.
"""
from __future__ import annotations

from io import BytesIO
from pathlib import Path
import hashlib
import sys

from edp_logging.archive import parse_segment_header
from edp_logging.framing import Frame, FrameCodec, scan_stream

DEVICE = bytes([0xA6])*16
PAYLOAD = bytes([0x83,0x01,0x02,0x03])

def audit(root: Path) -> tuple[int,int,str]:
    if not root.is_dir():
        raise ValueError("LittleFS physical extraction missing")
    names={p.name for p in root.iterdir()}
    required={"seed.txt","slot0","slot1"}
    if not (required <= names <= required|{"slot2"}):
        raise ValueError(f"unexpected physical file set {names!r}")
    if (root/"seed.txt").read_bytes()!=b"EDP1":
        raise ValueError("provisioned physical partition seed wrong")
    for slot in (0,1):
        f=root/f"slot{slot}"
        if not f.is_file() or f.is_symlink():
            raise ValueError(f"protected slot{slot} not a normal file")
        data=f.read_bytes()
        if len(data)!=64:
            raise ValueError(f"protected slot{slot} wrong size {len(data)}")
        header=parse_segment_header(data[:40])
        if (header.slot,header.generation,header.incarnation)!=(
            slot,slot+1,3
        ) or header.device_identifier!=DEVICE:
            raise ValueError(f"protected slot{slot} identity invalid")
        frames=list(scan_stream(BytesIO(data[40:]),max_payload=32))
        if len(frames)!=1 or not isinstance(frames[0],Frame):
            raise ValueError(f"protected slot{slot} EDLS/EDLG not complete")
        frame=frames[0]
        if frame.codec!=FrameCodec.Cbor or frame.payload!=PAYLOAD or frame.recovered_candidate:
            raise ValueError(f"protected slot{slot} CRC or payload invalid")
    candidate="absent"
    if "slot2" in names:
        f=root/"slot2"
        if not f.is_file() or f.is_symlink():
            raise ValueError("unacknowledged candidate not normal file")
        raw=f.read_bytes()
        if len(raw)>262144:
            raise ValueError("unacknowledged candidate exceeds provider bound")
        candidate=f"{len(raw)}B SHA256={hashlib.sha256(raw).hexdigest()}"
    return (1,2,candidate)

if __name__=="__main__":
    if len(sys.argv)!=2:
        raise SystemExit("usage: audit_midwrite_extracted_edls.py <raw LittleFS extract>")
    try:
        oldest,newest,candidate=audit(Path(sys.argv[1]))
    except (ValueError,OSError) as exc:
        print(f"OFFLINE_MIDWRITE_EDLS_AUDIT=FAIL reason={exc}",flush=True)
        raise SystemExit(1)
    print(
        f"OFFLINE_MIDWRITE_EDLS_AUDIT=PASS oldest={oldest} "
        f"newest={newest} candidate={candidate}",flush=True)
