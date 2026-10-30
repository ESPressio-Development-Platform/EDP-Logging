"""Offline structural frame/segment inspection; typed LogRecord decoding pending."""

from __future__ import annotations

import argparse
import contextlib
import json
import sys
import re
from pathlib import Path
from typing import Sequence

from .archive import SEGMENT_HEADER_SIZE, SegmentHeaderError, parse_segment_header, parse_sealed_segment
from .framing import Frame, scan_stream
from .record_structure import EnvelopeError, PreservedFloat, inspect_typed_envelope
from .schema import SchemaError, decode_declared_arguments, load_generated_schema
from .historical_schema import load_archived_diagnostic_schema
from .record_facts import RecordFactError, inspect_log_record_facts
from .analysis import EvidenceError, analyse_attempts, compare_record_time


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Scan EDLG v1 frames without interpreting typed LogRecord arguments")
    parser.add_argument("path", type=Path,
                        help="existing captured binary framing/archive slot; '-' reads live binary stdin until EOF")
    parser.add_argument("--archive", action="store_true", help="validate EDLS segment header before framing")
    parser.add_argument("--inspect-record", action="store_true", help="inspect typed envelope without guessing parameter semantics")
    parser.add_argument("--schema", type=Path, help="generated historical diagnostic-schema.json for validated CBOR argument typing")
    parser.add_argument("--schema-sha256", help="64-hex SHA-256 of exact schema bytes from an independently trusted pinned manifest")
    parser.add_argument("--historical-archive", type=Path, help="EDP-Localisation offline family archive root")
    parser.add_argument("--contract-family-fingerprint", help="explicit 32-uppercase-hex historic ContractFamily fingerprint; NOT the diagnostic fingerprint")
    parser.add_argument("--contract-family-digest", help="optional full 64-uppercase-hex historic ContractFamily digest")
    parser.add_argument("--trusted-archive-manifest-sha256", help="independently trusted full 64-hex SHA256 of selected historical family manifest")
    parser.add_argument("--historical-language", help="resolve template from explicitly selected historical ContractFamily; no argument interpolation")
    parser.add_argument("--max-historical-template-bytes", type=int, default=65536, help="maximum decoded historical template UTF-8 bytes")
    parser.add_argument("--validate-record", action="store_true", help="validate the known outer LogRecord V1 field structure")
    parser.add_argument("--expected-root-type-hex",
        help="16 hex digits of LogRecord's independently verified EDP-System TypeIdentifier; requires --validate-record")
    parser.add_argument("--analyse-attempts", action="store_true",
                        help="emit bounded per-device/incarnation ticket-evidence summaries; requires strict pinned-root record validation")
    parser.add_argument("--compare-record-times", action="store_true",
                        help="emit explicit uncertainty-aware pairwise ordering evidence; requires validated pinned root")
    parser.add_argument("--max-comparison-pairs", type=int, default=4096,
                        help="finite ceiling on pairwise record comparisons; refuse the entire summary if exceeded")
    parser.add_argument("--max-analysis-records", type=int, default=65536,
                        help="maximum validated LogRecords retained for attempt evidence")
    parser.add_argument("--max-analysis-groups", type=int, default=1024,
                        help="maximum distinct device/incarnation evidence groups")
    parser.add_argument("--forensic", action="store_true", help="bounded resync; mark recovered candidate frames")
    parser.add_argument("--max-payload", type=int, default=65_536)
    parser.add_argument("--resync-limit", type=int, default=1_048_576)
    args = parser.parse_args(argv)

    failures = 0
    if (args.analyse_attempts or args.compare_record_times) and (
        not args.validate_record or args.expected_root_type_hex is None
    ):
        print(json.dumps({"kind": "schema-error",
                          "message": "attempt/time evidence requires --validate-record and --expected-root-type-hex"}))
        return 2
    if (args.max_analysis_records < 1 or args.max_analysis_groups < 1
            or args.max_comparison_pairs < 1):
        print(json.dumps({"kind": "schema-error",
                          "message": "analysis resource bounds must both be positive"}))
        return 2
    analysis_records = []
    analysis_offsets = []
    analysis_overflow = False
    expected_root = None
    if args.expected_root_type_hex is not None:
        if not args.validate_record or re.fullmatch(
            r"[0-9a-fA-F]{16}", args.expected_root_type_hex
        ) is None:
            print(json.dumps({
                "kind": "schema-error",
                "message": "--expected-root-type-hex requires --validate-record and exactly 16 hexadecimal digits",
            }))
            return 2
        expected_root = bytes.fromhex(args.expected_root_type_hex)
    historical_schema = None
    historical_evidence = None
    uses_archive = args.historical_archive is not None
    if (
        (args.contract_family_fingerprint is not None
         or args.contract_family_digest is not None
         or args.trusted_archive_manifest_sha256 is not None)
        and not uses_archive
    ) or (uses_archive and args.contract_family_fingerprint is None):
        print(json.dumps({
            "kind": "schema-error",
            "message": "historical archive requires --historical-archive and --contract-family-fingerprint",
        }))
        return 2
    if args.historical_language is not None and not uses_archive:
        print(json.dumps({
            "kind": "schema-error",
            "message": "--historical-language requires --historical-archive",
        }))
        return 2
    if args.max_historical_template_bytes <= 0:
        print(json.dumps({
            "kind": "schema-error",
            "message": "historical template byte limit must be positive",
        }))
        return 2
    if uses_archive and (args.schema is not None or args.schema_sha256 is not None):
        print(json.dumps({
            "kind": "schema-error",
            "message": "historical archive cannot be combined with --schema or --schema-sha256",
        }))
        return 2
    if uses_archive:
        try:
            pin = None
            if args.trusted_archive_manifest_sha256 is not None:
                if re.fullmatch(r"[0-9a-fA-F]{64}", args.trusted_archive_manifest_sha256) is None:
                    raise SchemaError("trusted archive manifest SHA-256 must contain 64 hex digits")
                pin = bytes.fromhex(args.trusted_archive_manifest_sha256)
            historical_evidence = load_archived_diagnostic_schema(
                args.historical_archive,
                args.contract_family_fingerprint,
                contract_family_digest=args.contract_family_digest,
                trusted_manifest_sha256=pin,
            )
            historical_schema = historical_evidence.schema
        except (SchemaError, OSError) as error:
            print(json.dumps({"kind": "schema-error", "message": str(error)}))
            return 2
    if args.schema_sha256 is not None and args.schema is None:
        print(json.dumps({"kind": "schema-error", "message": "--schema-sha256 requires --schema"}))
        return 2
    if args.schema is not None:
        try:
            pin = None
            if args.schema_sha256 is not None:
                if re.fullmatch(r"[0-9a-fA-F]{64}", args.schema_sha256) is None:
                    raise SchemaError("--schema-sha256 must contain exactly 64 hexadecimal characters")
                pin = bytes.fromhex(args.schema_sha256)
            historical_schema = load_generated_schema(
                args.schema.read_bytes(), trusted_artifact_sha256=pin
            )
        except (SchemaError, OSError) as error:
            print(json.dumps({"kind": "schema-error", "message": str(error)}))
            return 2

    # A pipe/FIFO serial capture must not need seek, pre-read the input,
    # or close the host's own standard input on CLI return.
    source_owner = (
        contextlib.nullcontext(sys.stdin.buffer)
        if args.path == Path("-")
        else args.path.open("rb")
    )
    with source_owner as source:
        base = 0
        sealed_frames = None
        if args.archive:
            try:
                raw_header = source.read(SEGMENT_HEADER_SIZE)
                header = parse_segment_header(raw_header)
                if header.profile == 1:
                    _, sealed_frames = parse_sealed_segment(
                        raw_header + source.read(1_048_576 + 1 - SEGMENT_HEADER_SIZE),
                        max_payload=args.max_payload,
                    )
            except SegmentHeaderError as error:
                print(json.dumps({"kind": "segment-error", "message": str(error)}))
                return 2
            print(json.dumps({
                "kind": "segment",
                "sealed": header.profile == 1,
                "slot": header.slot,
                "generation": header.generation,
                "incarnation": header.incarnation,
                "deviceIdentifierHex": header.device_identifier.hex().upper(),
            }))
            base = SEGMENT_HEADER_SIZE

        for event in (sealed_frames if sealed_frames is not None else scan_stream(
            source,
            max_payload=args.max_payload,
            forensic=args.forensic,
            resync_limit=args.resync_limit,
            base_offset=base,
        )):
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
                if args.inspect_record or args.validate_record or historical_schema is not None:
                    try:
                        envelope = inspect_typed_envelope(event.payload, event.codec)
                    except EnvelopeError as error:
                        failures += 1
                        report["recordError"] = str(error)
                    else:
                        report["typeIdentifierHex"] = envelope.type_identifier.hex().upper()
                        report["fieldIdentifiers"] = sorted(envelope.fields)
                        report["parameterSchemaInterpreted"] = False
                        if args.validate_record:
                            try:
                                facts = inspect_log_record_facts(
                                    envelope, expected_root_type_identifier=expected_root
                                )
                            except RecordFactError as error:
                                failures += 1
                                report["recordFactError"] = str(error)
                            else:
                                report["logRecordCandidate"] = True
                                report["messageIdentifier"] = list(facts.message_identifier)
                                report["deviceIdentifierHex"] = facts.device_identifier.hex().upper()
                                report["incarnation"] = facts.incarnation
                                report["attemptSequence"] = facts.attempt_sequence
                                report["attemptSequenceWidth"] = facts.sequence_width
                                report["monotonicNanoseconds"] = facts.monotonic_nanoseconds
                                report["synchronizedNanoseconds"] = facts.synchronized_nanoseconds
                                report["clockEra"] = facts.clock_era
                                report["uncertaintyNanoseconds"] = facts.uncertainty_nanoseconds
                                report["clockCorrelationProjectionStatus"] = facts.temporal_quality[0]
                                report["temporalQualityAssessment"] = facts.temporal_quality[1]
                                report["recordValidityFacts"] = facts.record_flags
                                # A caller supplied digest/type pin is a strict equality
                                # check, not proof of who issued or signed that identity.
                                report["rootTypeMatchedSuppliedIdentity"] = expected_root is not None
                                report["rootTypeGoverned"] = False
                                if args.analyse_attempts or args.compare_record_times:
                                    if len(analysis_records) >= args.max_analysis_records:
                                        analysis_overflow = True
                                    else:
                                        analysis_records.append(facts)
                                        analysis_offsets.append(event.offset)
                        if historical_schema is not None:
                            report["schemaArtifactSha256PinMatched"] = (
                                historical_schema.artifact_sha256_pin_matched
                            )
                            if historical_evidence is not None:
                                report["historicalContractFamilyFingerprint"] = (
                                    historical_evidence.contract_family_fingerprint.hex().upper()
                                )
                                report["historicalManifestTrustedPinMatched"] = (
                                    historical_evidence.manifest_sha256_pin_matched
                                )
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
                                if historical_evidence is not None and args.historical_language is not None:
                                    try:
                                        template = historical_evidence.resolve_template(
                                            args.historical_language,
                                            decoded.message.identity,
                                            expected_diagnostic_fingerprint=historical_schema.fingerprint,
                                            max_template_bytes=args.max_historical_template_bytes,
                                        )
                                    except SchemaError as error:
                                        failures += 1
                                        report["historicalTemplateError"] = str(error)
                                    else:
                                        report["historicalTemplate"] = template.template
                                        report["historicalTemplateSupplyingLanguage"] = template.supplying_language
                                        report["historicalTemplateFallbackUsed"] = template.fallback_used
                                        report["historicalTemplateInterpolated"] = template.interpolated
                print(json.dumps(report))
            else:
                failures += 1
                print(json.dumps({
                    "kind": "frame-error", "offset": event.offset,
                    "status": event.status.value,
                }))
    if args.analyse_attempts:
        if analysis_overflow:
            failures += 1
            print(json.dumps({
                "kind": "attempt-evidence-error",
                "message": "validated record observations exceeded bounded evidence capacity",
            }))
        else:
            try:
                evidence = analyse_attempts(
                    analysis_records,
                    max_records=args.max_analysis_records,
                    max_groups=args.max_analysis_groups,
                )
            except (ValueError, EvidenceError) as error:
                failures += 1
                print(json.dumps({
                    "kind": "attempt-evidence-error",
                    "message": str(error),
                }))
            else:
                for group in evidence:
                    print(json.dumps({
                        "kind": "attempt-evidence",
                        "deviceIdentifierHex": group.device_identifier.hex().upper(),
                        "incarnation": group.incarnation,
                        "sequenceWidth": group.sequence_width,
                        "observedDistinctTickets": group.observed_count,
                        "duplicateObservations": group.duplicate_observations,
                        "firstObservedTicket": group.first_ticket,
                        "lastObservedTicket": group.last_ticket,
                        "unseenTicketIntervals": list(group.unseen_intervals),
                        "partialCapturePossible": group.partial_capture_possible,
                        "rootTypeMatchedSuppliedIdentity": True,
                        "lossCauseInferred": False,
                    }))
    if args.compare_record_times:
        pairs = len(analysis_records) * (len(analysis_records) - 1) // 2
        if analysis_overflow or pairs > args.max_comparison_pairs:
            failures += 1
            print(json.dumps({
                "kind": "temporal-evidence-error",
                "message": (
                    "bounded record observation capacity exceeded"
                    if analysis_overflow
                    else "pairwise comparison count exceeds configured finite ceiling"
                ),
            }))
        else:
            for left in range(len(analysis_records)):
                for right in range(left + 1, len(analysis_records)):
                    relation = compare_record_time(
                        analysis_records[left], analysis_records[right]
                    )
                    print(json.dumps({
                        "kind": "temporal-comparison",
                        "leftOffset": analysis_offsets[left],
                        "rightOffset": analysis_offsets[right],
                        "relationship": relation.value,
                        "certainTotalOrder": relation.value in ("Earlier", "Later"),
                        "civilUtcInferred": False,
                        "unobservedGapLossInferred": False,
                        "rootTypeMatchedSuppliedIdentity": True,
                    }))
    return 1 if failures else 0
