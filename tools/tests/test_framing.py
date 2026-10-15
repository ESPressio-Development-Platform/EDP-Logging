from __future__ import annotations

import io
import unittest

from edp_logging.archive import SegmentHeaderError, parse_segment_header
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


if __name__ == "__main__":
    unittest.main()
