"""Offline structural frame/segment inspection; typed LogRecord decoding pending."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Sequence

from .archive import SEGMENT_HEADER_SIZE, SegmentHeaderError, parse_segment_header
from .framing import Frame, scan_stream


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Scan EDLG v1 frames without interpreting typed LogRecord arguments")
    parser.add_argument("path", type=Path, help="existing captured binary framing or archive slot")
    parser.add_argument("--archive", action="store_true", help="validate EDLS segment header before framing")
    parser.add_argument("--forensic", action="store_true", help="bounded resync; mark recovered candidate frames")
    parser.add_argument("--max-payload", type=int, default=65_536)
    parser.add_argument("--resync-limit", type=int, default=1_048_576)
    args = parser.parse_args(argv)

    failures = 0
    with args.path.open("rb") as source:
        base = 0
        if args.archive:
            try:
                header = parse_segment_header(source.read(SEGMENT_HEADER_SIZE))
            except SegmentHeaderError as error:
                print(json.dumps({"kind": "segment-error", "message": str(error)}))
                return 2
            print(json.dumps({
                "kind": "segment",
                "slot": header.slot,
                "generation": header.generation,
                "incarnation": header.incarnation,
                "deviceIdentifierHex": header.device_identifier.hex().upper(),
            }))
            base = SEGMENT_HEADER_SIZE

        for event in scan_stream(
            source,
            max_payload=args.max_payload,
            forensic=args.forensic,
            resync_limit=args.resync_limit,
            base_offset=base,
        ):
            if isinstance(event, Frame):
                print(json.dumps({
                    "kind": "frame",
                    "offset": event.offset,
                    "length": event.total_size,
                    "codec": event.codec.name,
                    "payloadLength": len(event.payload),
                    "forensicCandidate": event.recovered_candidate,
                    "crc32c": f"{event.crc32c_value:08X}",
                }))
            else:
                failures += 1
                print(json.dumps({
                    "kind": "frame-error", "offset": event.offset,
                    "status": event.status.value,
                }))
    return 1 if failures else 0
