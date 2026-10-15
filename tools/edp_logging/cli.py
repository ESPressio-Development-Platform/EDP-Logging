"""Offline structural frame/segment inspection; typed LogRecord decoding pending."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Sequence

from .archive import SEGMENT_HEADER_SIZE, SegmentHeaderError, parse_segment_header
from .framing import Frame, scan_stream
from .record_structure import EnvelopeError, PreservedFloat, inspect_typed_envelope
from .schema import SchemaError, decode_declared_arguments, load_generated_schema


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Scan EDLG v1 frames without interpreting typed LogRecord arguments")
    parser.add_argument("path", type=Path, help="existing captured binary framing or archive slot")
    parser.add_argument("--archive", action="store_true", help="validate EDLS segment header before framing")
    parser.add_argument("--inspect-record", action="store_true", help="inspect typed envelope without guessing parameter semantics")
    parser.add_argument("--schema", type=Path, help="generated historical diagnostic-schema.json for validated CBOR argument typing")
    parser.add_argument("--forensic", action="store_true", help="bounded resync; mark recovered candidate frames")
    parser.add_argument("--max-payload", type=int, default=65_536)
    parser.add_argument("--resync-limit", type=int, default=1_048_576)
    args = parser.parse_args(argv)

    failures = 0
    historical_schema = None
    if args.schema is not None:
        try:
            historical_schema = load_generated_schema(args.schema.read_bytes())
        except (SchemaError, OSError) as error:
            print(json.dumps({"kind": "schema-error", "message": str(error)}))
            return 2

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
                report = {
                    "kind": "frame",
                    "offset": event.offset,
                    "length": event.total_size,
                    "codec": event.codec.name,
                    "payloadLength": len(event.payload),
                    "forensicCandidate": event.recovered_candidate,
                    "crc32c": f"{event.crc32c_value:08X}",
                }
                if args.inspect_record or historical_schema is not None:
                    try:
                        envelope = inspect_typed_envelope(event.payload, event.codec)
                    except EnvelopeError as error:
                        failures += 1
                        report["recordError"] = str(error)
                    else:
                        report["typeIdentifierHex"] = envelope.type_identifier.hex().upper()
                        report["fieldIdentifiers"] = sorted(envelope.fields)
                        report["parameterSchemaInterpreted"] = False
                        if historical_schema is not None:
                            try:
                                decoded = decode_declared_arguments(envelope, historical_schema)
                            except SchemaError as error:
                                failures += 1
                                report["schemaError"] = str(error)
                            else:
                                report["messageIdentifier"] = list(decoded.message.identity)
                                report["parameterKinds"] = [
                                    parameter.type_name for parameter in decoded.message.parameters
                                ]
                                report["argumentCount"] = len(decoded.values)
                                report["parameterSchemaInterpreted"] = True
                print(json.dumps(report))
            else:
                failures += 1
                print(json.dumps({
                    "kind": "frame-error", "offset": event.offset,
                    "status": event.status.value,
                }))
    return 1 if failures else 0
