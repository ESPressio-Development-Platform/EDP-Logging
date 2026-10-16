from __future__ import annotations

import io
import unittest

from edp_logging.archive import SegmentHeader, SegmentHeaderError, parse_segment_header, encode_segment_header
from edp_logging.framing import (
    Frame, FrameCodec, FrameIssue, FrameStatus, crc32c, encode_frame, scan_stream,
)


class FramingTests(unittest.TestCase):

    def test_crc32c_known_vector(self) -> None:
        self.assertEqual(crc32c(b"123456789"), 0xE3069283)

    def test_valid_frame_and_exact_offset(self) -> None:
        framed = encode_frame(FrameCodec.Cbor, b"\x83\x01\x48")
        self.assertEqual(len(framed), 23)
        events = list(scan_stream(io.BytesIO(framed), base_offset=40))
        self.assertEqual(len(events), 1)
        self.assertIsInstance(events[0], Frame)
        self.assertEqual(events[0].offset, 40)
        self.assertFalse(events[0].recovered_candidate)
        self.assertEqual(events[0].payload, b"\x83\x01\x48")

    def test_truncated_and_corrupt_candidate_rejected(self) -> None:
        framed = bytearray(encode_frame(FrameCodec.Cbor, b"test"))
        self.assertEqual(
            list(scan_stream(io.BytesIO(framed[:-1])))[0].status,
            FrameStatus.TruncatedFrame
        )
        framed[16] ^= 1
        self.assertEqual(
            list(scan_stream(io.BytesIO(framed)))[0].status,
            FrameStatus.CrcMismatch
        )

    def test_invalid_flags_and_bounded_length(self) -> None:
        framed = bytearray(encode_frame(FrameCodec.Json, b"{}"))
        framed[6] = 1
        self.assertEqual(
            list(scan_stream(io.BytesIO(framed)))[0].status,
            FrameStatus.UnsupportedFlags
        )
        framed[6] = 0
        framed[8:12] = (0xFFFFFFFF).to_bytes(4, "little")
        self.assertEqual(
            list(scan_stream(io.BytesIO(framed), max_payload=256))[0].status,
            FrameStatus.PayloadTooLarge
        )

    def test_forensic_resync_preserves_damage(self) -> None:
        first = encode_frame(FrameCodec.Cbor, b"first")
        second = encode_frame(FrameCodec.Cbor, b"second")
        damaged = first[:18] + bytes([first[18] ^ 1]) + first[19:]
        data = damaged + b"garbage" + second
        result = list(scan_stream(io.BytesIO(data), forensic=True, resync_limit=100))
        accepted = [x for x in result if isinstance(x, Frame)]
        self.assertEqual(len(accepted), 1)
        self.assertEqual(accepted[0].payload, b"second")
        self.assertTrue(accepted[0].recovered_candidate)
        self.assertEqual(accepted[0].offset, len(damaged) + 7)
        self.assertTrue(any(isinstance(x, FrameIssue) for x in result))

    def test_forensic_classification_stays_sticky_after_recovery(self) -> None:
        first = encode_frame(FrameCodec.Cbor, b"intact")
        second = encode_frame(FrameCodec.Cbor, b"salvaged")
        third = encode_frame(FrameCodec.Cbor, b"still-forensic")
        stream = first + b"corrupt" + second + third
        items = list(scan_stream(io.BytesIO(stream), forensic=True, resync_limit=128))
        frames = [item for item in items if isinstance(item, Frame)]
        self.assertEqual(len(frames), 3)
        self.assertFalse(frames[0].recovered_candidate)
        self.assertTrue(frames[1].recovered_candidate)
        self.assertTrue(frames[2].recovered_candidate)

    def test_resync_budget_enforced(self) -> None:
        output = list(scan_stream(
            io.BytesIO(b"q" * 100),
            forensic=True,
            resync_limit=4,
        ))
        self.assertEqual(output[-1].status, FrameStatus.ResynchronizationLimit)

    def test_edls_header_crc_and_identity(self) -> None:
        header = bytearray(40)
        header[:4] = b"EDLS"
        header[4] = 1
        header[6:8] = (2).to_bytes(2, "little")
        header[8:16] = (7).to_bytes(8, "little")
        header[16:32] = bytes(range(16))
        header[32:36] = (9).to_bytes(4, "little")
        header[36:40] = crc32c(header[:36]).to_bytes(4, "little")
        parsed = parse_segment_header(bytes(header))
        self.assertEqual((parsed.slot, parsed.generation, parsed.incarnation), (2, 7, 9))
        header[16] ^= 0x40
        with self.assertRaises(SegmentHeaderError):
            parse_segment_header(bytes(header))


    def test_edls_cpp_python_golden_parity(self) -> None:
        segment = SegmentHeader(
            slot=2, generation=7, device_identifier=bytes(range(1, 17)), incarnation=9
        )
        encoded = encode_segment_header(segment)
        self.assertEqual(
            encoded.hex(),
            "45444c530100020007000000000000000102030405060708"
            "090a0b0c0d0e0f1009000000ae4e0c51"
        )
        self.assertEqual(parse_segment_header(encoded), segment)
        for invalid in (
            SegmentHeader(2, 0, segment.device_identifier, 9),
            SegmentHeader(2, 7, bytes(16), 9),
            SegmentHeader(2, 7, segment.device_identifier, 0),
        ):
            with self.assertRaises(SegmentHeaderError):
                encode_segment_header(invalid)


    def test_cbor_typed_envelope_structural_inspection(self) -> None:
        from edp_logging.record_structure import inspect_typed_envelope
        root = bytes([0x83, 0x01, 0x48]) + bytes(range(1, 9)) + bytes([0xA1, 0x01, 0x02])
        result = inspect_typed_envelope(root, FrameCodec.Cbor)
        self.assertEqual(result.type_identifier, bytes(range(1, 9)))
        self.assertEqual(result.fields, {1: 2})

    def test_cbor_rejects_noncanonical_integer_and_field_order(self) -> None:
        from edp_logging.record_structure import EnvelopeError, inspect_typed_envelope
        prefix = bytes([0x83, 0x01, 0x48]) + bytes(range(1, 9))
        for suffix in (
            bytes([0xA1, 0x01, 0x18, 0x02]),
            bytes([0xA2, 0x02, 0x01, 0x01, 0x01]),
            bytes([0xA1, 0x01, 0xF9, 0, 0]),
        ):
            with self.assertRaises(EnvelopeError):
                inspect_typed_envelope(prefix + suffix, FrameCodec.Cbor)

    def test_json_envelope_exact_int_and_duplicate_rejected(self) -> None:
        from edp_logging.record_structure import EnvelopeError, inspect_typed_envelope
        valid = b'{"$edp":{"v":1,"type":"0102030405060708"},"value":{"4":18446744073709551615}}'
        result = inspect_typed_envelope(valid, FrameCodec.Json)
        self.assertEqual(result.fields[4], 0xFFFFFFFFFFFFFFFF)
        for invalid in (
            b'{"$edp":{"v":1,"type":"0102030405060708"},"value":{"4":1,"4":2}}',
            b'{"$edp":{"v":1,"type":"0102030405060708"},"value":{"04":1}}',
            b'{"$edp":{"v":1,"type":"0102030405060708"},"value":{"4":1e400}}',
        ):
            with self.assertRaises(EnvelopeError):
                inspect_typed_envelope(invalid, FrameCodec.Json)


    def test_governed_historical_argument_validation(self) -> None:
        import json
        from edp_logging.schema import (
            SchemaError, decode_declared_arguments, load_generated_schema,
        )
        from edp_logging.record_structure import inspect_typed_envelope
        document = {
            "schemaVersion": 1,
            "kind": "edp-diagnostic-schema",
            "diagnosticSchemaFingerprint": "AB" * 16,
            "diagnosticSchemaDigestSha256": "AB" * 16 + "CD" * 16,
            "identifierWidths": {"domain": 1, "subDomain": 1, "string": 2},
            "messages": [{
                "domain": 1, "subDomain": 0, "string": 3,
                "status": "active", "diagnosticDeclared": True,
                "parameters": [{"index": 0, "status": "active", "type": "u16"}],
            }],
        }
        schema = load_generated_schema(json.dumps(document).encode("utf-8"))
        typed_envelope = (
            bytes([0x83, 0x01, 0x48]) + bytes(range(1, 9))
            + bytes([0xA3, 0x00, 0x83, 0x01, 0x00, 0x03, 0x0C, 0x50])
            + bytes.fromhex("AB" * 16)
            + bytes([0x0D, 0x81, 0x18, 0x2A])
        )
        record = inspect_typed_envelope(typed_envelope, FrameCodec.Cbor)
        resolved = decode_declared_arguments(record, schema)
        self.assertEqual(resolved.message.identity, (1, 0, 3))
        self.assertEqual(resolved.values, (42,))
        with self.assertRaises(SchemaError):
            decode_declared_arguments(record, load_generated_schema(
                json.dumps({**document, "diagnosticSchemaFingerprint": "CD" * 16,
                            "diagnosticSchemaDigestSha256": "CD" * 16 + "AB" * 16}).encode()
            ))
        with self.assertRaises(SchemaError):
            load_generated_schema(
                json.dumps({**document, "messages": [
                    {**document["messages"][0], "parameters": [
                        {"index": 1, "status": "active", "type": "u16"}
                    ]}
                ]}).encode()
            )


    def test_malformed_historical_schema_is_typed_failure(self) -> None:
        import json
        from edp_logging.schema import SchemaError, load_generated_schema

        valid = {
            "schemaVersion": 1, "kind": "edp-diagnostic-schema",
            "diagnosticSchemaFingerprint": "AB" * 16,
            "diagnosticSchemaDigestSha256": "AB" * 16 + "CD" * 16,
            "identifierWidths": {"domain": 1, "subDomain": 1, "string": 1},
            "messages": [
                {"domain": 1, "subDomain": 0, "string": 3, "status": "active",
                 "diagnosticDeclared": True, "parameters": [
                     {"index": 0, "status": "active", "type": "u16"},
                 ]},
            ],
        }
        for broken in (
            {**valid, "schemaVersion": True},
            {**valid, "unexpected": 1},
            {**valid, "messages": [
                {**valid["messages"][0], "parameters": [
                    {"index": 0, "status": "active", "type": []}
                ]}
            ]},
            {**valid, "messages": [
                {**valid["messages"][0], "parameters": [
                    {"index": 0, "status": [], "type": "u16"}
                ]}
            ]},
            {**valid, "messages": [
                {**valid["messages"][0], "status": "retired"}
            ]},
        ):
            # Only the retired String is a valid historical declaration,
            # while arbitrary unhashable/type-mismatched values must fail.
            if broken["messages"][0]["status"] == "retired":
                continue
            with self.assertRaises(SchemaError):
                load_generated_schema(json.dumps(broken).encode("utf-8"))
        with self.assertRaises(SchemaError):
            load_generated_schema(
                json.dumps(valid).replace('"schemaVersion": 1', '"schemaVersion": NaN').encode()
            )


    def test_log_record_outer_sequence_and_zero_time(self) -> None:
        from edp_logging.record_facts import RecordFactError, inspect_log_record_facts
        from edp_logging.record_structure import TypedEnvelope

        base = {
            0: [1, 0, 3], 1: 4, 2: bytes(range(1, 17)), 3: 10,
            4: 65535, 5: 16, 6: 1_000, 7: 0, 8: 1, 9: 17,
            10: 999, 11: 1, 12: bytes(range(16)), 13: [],
            14: 0,
        }
        sample = TypedEnvelope(bytes(range(1, 9)), base, FrameCodec.Cbor)
        facts = inspect_log_record_facts(sample)
        self.assertEqual((facts.sequence_width, facts.attempt_sequence), (16, 65535))
        self.assertEqual(facts.synchronized_nanoseconds, 0)
        for invalid in (
            {**base, 4: 65536},
            {**base, 5: 8},
            {**base, 1: 6},
            {**base, 7: 100, 9: 0},
            {**base, 8: 0},
            {**base, 2: bytes(16)},
            {**base, 13: {}},
            {**base, 15: 0},
        ):
            with self.assertRaises(RecordFactError):
                inspect_log_record_facts(TypedEnvelope(sample.type_identifier, invalid, FrameCodec.Cbor))
        without_sync = {key: value for key, value in base.items() if key != 7}
        without_sync[8] = 0
        without_sync[9] = 0
        self.assertIsNone(inspect_log_record_facts(
            TypedEnvelope(sample.type_identifier, without_sync, FrameCodec.Cbor)
        ).synchronized_nanoseconds)


    def test_archive_selects_largest_unique_generation(self) -> None:
        import tempfile
        from pathlib import Path
        from edp_logging.archive_recovery import inspect_archive_slots, SlotState

        with tempfile.TemporaryDirectory() as root:
            paths = [Path(root) / "slot0.edls", Path(root) / "slot1.edls"]
            identity = bytes(range(1, 17))
            paths[0].write_bytes(
                encode_segment_header(SegmentHeader(0, 7, identity, 9))
                + encode_frame(FrameCodec.Cbor, b"ok")
            )
            paths[1].write_bytes(
                encode_segment_header(SegmentHeader(1, 8, identity, 10))
                + encode_frame(FrameCodec.Cbor, b"new")
            )
            result = inspect_archive_slots(paths)
            self.assertTrue(result.order_unambiguous)
            self.assertEqual(result.newest_known_slot, 1)
            self.assertEqual(result.next_generation, 9)
            self.assertEqual(result.slots[0].verified_frames, 1)
            self.assertEqual(result.slots[1].state, SlotState.Clean)

            paths[1].write_bytes(
                encode_segment_header(SegmentHeader(1, 8, identity, 10))
                + encode_frame(FrameCodec.Cbor, b"good") + b"torn-tail"
            )
            torn = inspect_archive_slots(paths)
            self.assertEqual(torn.slots[1].state, SlotState.DamagedTail)
            self.assertEqual(torn.slots[1].verified_frames, 1)
            self.assertTrue(torn.order_unambiguous)

    def test_archive_ambiguity_does_not_issue_successor(self) -> None:
        import tempfile
        from pathlib import Path
        from edp_logging.archive_recovery import inspect_archive_slots, SlotState

        with tempfile.TemporaryDirectory() as root:
            paths = [Path(root) / "0", Path(root) / "1"]
            identity = bytes(range(1, 17))
            paths[0].write_bytes(
                encode_segment_header(SegmentHeader(0, 2, identity, 9))
            )
            paths[1].write_bytes(b"partial header")
            uncertain = inspect_archive_slots(paths)
            self.assertFalse(uncertain.order_unambiguous)
            self.assertEqual(uncertain.newest_known_slot, 0)
            self.assertIsNone(uncertain.next_generation)
            self.assertEqual(uncertain.slots[1].state, SlotState.InvalidHeader)

            paths[1].write_bytes(
                encode_segment_header(SegmentHeader(1, 2, identity, 9))
            )
            duplicate = inspect_archive_slots(paths)
            self.assertFalse(duplicate.order_unambiguous)
            self.assertIsNone(duplicate.newest_known_slot)
            self.assertIsNone(duplicate.next_generation)

            paths[1].write_bytes(
                encode_segment_header(SegmentHeader(1, 3, bytes(range(2, 18)), 10))
            )
            mismatch = inspect_archive_slots(paths)
            self.assertFalse(mismatch.order_unambiguous)
            self.assertIsNone(mismatch.next_generation)


    def test_attempt_observations_not_loss_stage(self) -> None:
        from edp_logging.analysis import analyse_attempts, EvidenceError
        from edp_logging.record_facts import RecordFacts

        def record(ticket: int, incarnation: int = 2, width: int = 16) -> RecordFacts:
            return RecordFacts(
                message_identifier=(1, 0, 3), severity=2,
                device_identifier=bytes(range(1, 17)), incarnation=incarnation,
                attempt_sequence=ticket, sequence_width=width,
                monotonic_nanoseconds=ticket * 100,
                synchronized_nanoseconds=None, synchronization_state=0,
                clock_era=0, uncertainty_nanoseconds=0xFFFFFFFF,
                diagnostic_fingerprint=bytes(range(16)), positional_arguments=[],
                temporal_quality=0, record_flags=0, type_identifier=bytes(range(1, 9)),
            )

        evidence = analyse_attempts([record(1), record(3), record(3), record(6), record(2, 4)])
        self.assertEqual(len(evidence), 2)
        first = evidence[0]
        self.assertEqual(first.observed_count, 3)
        self.assertEqual(first.duplicate_observations, 1)
        self.assertEqual(first.unseen_intervals, ((2, 2), (4, 5)))
        self.assertTrue(first.partial_capture_possible)

        with self.assertRaises(EvidenceError):
            analyse_attempts([record(1), record(2, width=32)])
        with self.assertRaises(EvidenceError):
            analyse_attempts([record(1), record(2)], max_records=1)

    def test_temporal_order_respects_era_and_uncertainty(self) -> None:
        from dataclasses import replace
        from edp_logging.analysis import TemporalOrder, compare_record_time
        from edp_logging.record_facts import RecordFacts

        left = RecordFacts(
            message_identifier=(1, 0, 3), severity=2,
            device_identifier=bytes(range(1, 17)), incarnation=4,
            attempt_sequence=1, sequence_width=32,
            monotonic_nanoseconds=1000, synchronized_nanoseconds=5000,
            synchronization_state=1, clock_era=17, uncertainty_nanoseconds=100,
            diagnostic_fingerprint=bytes(range(16)), positional_arguments=[],
            temporal_quality=2, record_flags=0, type_identifier=bytes(range(1, 9)),
        )
        same_device = replace(left, monotonic_nanoseconds=1100, attempt_sequence=2)
        self.assertEqual(compare_record_time(left, same_device), TemporalOrder.Earlier)
        other = replace(
            left, device_identifier=bytes(range(2, 18)),
            synchronized_nanoseconds=5400, uncertainty_nanoseconds=100,
        )
        self.assertEqual(compare_record_time(left, other), TemporalOrder.Earlier)
        self.assertEqual(
            compare_record_time(left, replace(other, synchronized_nanoseconds=5150)),
            TemporalOrder.Overlaps
        )
        self.assertEqual(
            compare_record_time(left, replace(other, clock_era=18)),
            TemporalOrder.Incomparable
        )
        self.assertEqual(
            compare_record_time(left, replace(other, uncertainty_nanoseconds=0xFFFFFFFF)),
            TemporalOrder.Incomparable
        )


    def test_externally_pinned_historical_schema_exact_bytes(self) -> None:
        import contextlib
        import hashlib
        import json
        import tempfile
        from pathlib import Path

        from edp_logging.cli import main
        from edp_logging.schema import SchemaError, load_generated_schema

        # Claimed diagnostic fingerprint is not reconstructible from the
        # flattened messages; a separately trusted SHA-256 pin authenticates
        # exact received bytes to that independently distributed pin only.
        document = {
            "schemaVersion": 1, "kind": "edp-diagnostic-schema",
            "diagnosticSchemaFingerprint": "AB" * 16,
            "diagnosticSchemaDigestSha256": "AB" * 16 + "CD" * 16,
            "identifierWidths": {"domain": 1, "subDomain": 1, "string": 2},
            "messages": [{
                "domain": 1, "subDomain": 0, "string": 3,
                "status": "active", "diagnosticDeclared": True,
                "parameters": [{"index": 0, "status": "active", "type": "u16"}],
            }],
        }
        source = json.dumps(document, sort_keys=True).encode("utf-8")
        pin = hashlib.sha256(source).digest()
        self.assertFalse(load_generated_schema(source).artifact_sha256_pin_matched)
        self.assertTrue(load_generated_schema(
            source, trusted_artifact_sha256=pin
        ).artifact_sha256_pin_matched)
        for invalid_pin in (b"", b"x" * 31, b"x" * 33, "0" * 64, 1):
            with self.subTest(invalid_pin=invalid_pin), self.assertRaises(SchemaError):
                load_generated_schema(source, trusted_artifact_sha256=invalid_pin)
        with self.assertRaisesRegex(SchemaError, "do not match"):
            load_generated_schema(source + b" ", trusted_artifact_sha256=pin)

        # Full CLI integration: matching independent content pin is reported,
        # and mismatched pin rejects before the captured frame is decoded.
        typed_envelope = (
            bytes([0x83, 0x01, 0x48]) + bytes(range(1, 9))
            + bytes([0xA3, 0x00, 0x83, 0x01, 0x00, 0x03, 0x0C, 0x50])
            + bytes.fromhex("AB" * 16)
            + bytes([0x0D, 0x81, 0x18, 0x2A])
        )
        with tempfile.TemporaryDirectory() as directory:
            schema_file = Path(directory, "diagnostic-schema.json")
            capture_file = Path(directory, "capture.edlg")
            schema_file.write_bytes(source)
            capture_file.write_bytes(encode_frame(FrameCodec.Cbor, typed_envelope))

            def call(*extra: str) -> tuple[int, dict]:
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    code = main([str(capture_file), "--schema", str(schema_file), *extra])
                return code, json.loads(output.getvalue().splitlines()[0])

            code, event = call("--schema-sha256", pin.hex())
            self.assertEqual(code, 0)
            self.assertTrue(event["schemaArtifactSha256PinMatched"])
            self.assertTrue(event["parameterSchemaInterpreted"])
            code, event = call()
            self.assertEqual(code, 0)
            self.assertFalse(event["schemaArtifactSha256PinMatched"])
            code, event = call("--schema-sha256", "0" * 64)
            self.assertEqual(code, 2)
            self.assertEqual(event["kind"], "schema-error")
            code, event = call("--schema-sha256", "bogus")
            self.assertEqual(code, 2)
            self.assertEqual(event["kind"], "schema-error")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                code = main([str(capture_file), "--schema-sha256", pin.hex()])
            self.assertEqual(code, 2)
            self.assertEqual(json.loads(output.getvalue())["kind"], "schema-error")


if __name__ == "__main__":
    unittest.main()
