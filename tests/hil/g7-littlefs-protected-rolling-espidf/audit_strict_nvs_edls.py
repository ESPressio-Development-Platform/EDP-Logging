#!/usr/bin/env python3
"""Read-only independent CRC review of real NVS-watermarked EDLS files.

Reuse ONLY existing Logging host parser/CRC contracts. Input must be
the exported private 0x170000/0x90000 partition from the approved RPI400,
not a reconstructed UART transcript or auto-repaired filesystem.
"""
from __future__ import annotations

from io import BytesIO
from pathlib import Path
import sys

from edp_logging.archive import parse_segment_header
from edp_logging.framing import Frame, FrameCodec, scan_stream

EXPECTED = {
    "slot0": (0, 4, 4),
    "slot2": (2, 3, 4),
}
EXPECTED_FILES = frozenset({"seed.txt", *EXPECTED})
PAYLOAD = bytes((0x83, 0x01, 0x02, 0x03))


def audit(root: Path) -> tuple[int, int]:
    """Return exact (oldest, newest) generations or fail closed."""
    if not root.is_dir():
        raise ValueError("missing extracted filesystem")
    names = {f.name for f in root.iterdir()}
    if names != EXPECTED_FILES:
        raise ValueError(
            f"wrong finite retained file set: expected {sorted(EXPECTED_FILES)} "
            f"actual {sorted(names)}"
        )
    if (root / "seed.txt").read_bytes() != b"EDP1":
        raise ValueError("invalid provisioned partition seed")

    for filename, (slot, generation, incarnation) in EXPECTED.items():
        target = root / filename
        if not target.is_file() or target.is_symlink():
            raise ValueError(f"record not a regular file: {filename}")
        data = target.read_bytes()
        if len(data) != 64:
            raise ValueError(f"wrong finite EDLS size: {filename}, {len(data)}")
        header = parse_segment_header(data[:40])
        if (header.slot, header.generation, header.incarnation) != (
            slot, generation, incarnation
        ):
            raise ValueError(f"wrong EDLS generation/slot/incarnation: {filename}")
        if header.device_identifier != bytes((0xA6,)) * 16:
            raise ValueError(f"wrong device identity: {filename}")
        frames = list(scan_stream(BytesIO(data[40:]), max_payload=32))
        if (len(frames) != 1 or not isinstance(frames[0], Frame) or
                frames[0].codec != FrameCodec.Cbor or
                frames[0].payload != PAYLOAD or
                frames[0].recovered_candidate):
            raise ValueError(f"invalid canonical EDLG CRC/frame: {filename}")
    return (3, 4)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: audit_extracted_edls.py <private-extracted-dir>")
    try:
        retained = audit(Path(sys.argv[1]))
    except (ValueError, OSError) as exc:
        print(f"OFFLINE_STRICT_NVS_ACK_EDLS_AUDIT=FAIL reason={exc}", flush=True)
        raise SystemExit(1)
    print(f"OFFLINE_STRICT_NVS_ACK_EDLS_AUDIT=PASS oldest={retained[0]} newest={retained[1]}", flush=True)
