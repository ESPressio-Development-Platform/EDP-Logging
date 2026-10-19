"""Owner-issued EDP-Serialisation JSON positional value compatibility."""
import base64
import json
import unittest

from edp_logging.framing import FrameCodec
from edp_logging.record_structure import inspect_typed_envelope, PreservedFloat, EnvelopeError
from edp_logging.schema import load_generated_schema, decode_declared_arguments, SchemaError


class JsonOwnedSchemaTests(unittest.TestCase):
    def setUp(self):
        self.kinds = (
            "bool", "i16", "u8", "f32", "f64", "utf8", "bytes",
            "device-identifier", "type-identifier", "field-identifier",
            "runtime-incarnation-id", "clock-era", "string-identifier"
        )
        self.raw = [
            True, -32768, 255, 1.5, 3.25, "Hello",
            base64.b64encode(b"\x01\x02\xfe").decode("ascii"),
            base64.b64encode(bytes(range(16))).decode("ascii"),
            [0, 0, 1, 0, 0, 0, 0, 42], 255, 7, 17, [1, 0, 3]
        ]
        self.declaration = {
            "schemaVersion": 1,
            "kind": "edp-diagnostic-schema",
            "diagnosticSchemaFingerprint": "AB" * 16,
            "diagnosticSchemaDigestSha256": "AB" * 16 + "CD" * 16,
            "identifierWidths": {"domain": 1, "subDomain": 1, "string": 2},
            "messages": [{
                "domain": 1, "subDomain": 0, "string": 3,
                "status": "active", "diagnosticDeclared": True,
                "parameters": [
                    {"index": i, "status": "active", "type": k}
                    for i, k in enumerate(self.kinds)
                ]
            }]
        }
        self.schema = load_generated_schema(json.dumps(self.declaration).encode())

    def envelope(self, values=None, fingerprint=None):
        root = {"$edp": {"v": 1, "type": "0000010000000001"},
                "value": {"0": [1, 0, 3],
                          "12": list(bytes.fromhex("AB" * 16))
                                if fingerprint is None else fingerprint,
                          "13": self.raw if values is None else values}}
        return inspect_typed_envelope(
            json.dumps(root, ensure_ascii=False, separators=(",", ":")).encode(),
            FrameCodec.Json,
        )

    def test_all_owner_json_shapes_and_typed_values(self):
        result = decode_declared_arguments(self.envelope(), self.schema)
        self.assertEqual(result.message.identity, (1, 0, 3))
        self.assertEqual(result.values[0:3], (True, -32768, 255))
        self.assertEqual(result.values[3], PreservedFloat(1.5, 32))
        self.assertEqual(result.values[4], PreservedFloat(3.25, 64))
        self.assertEqual(result.values[5], "Hello")
        self.assertEqual(result.values[6], b"\x01\x02\xfe")
        self.assertEqual(result.values[7], bytes(range(16)))
        self.assertEqual(result.values[8], bytes([0, 0, 1, 0, 0, 0, 0, 42]))
        self.assertEqual(result.values[9:12], (255, 7, 17))
        self.assertEqual(result.values[12], [1, 0, 3])

    def test_strict_no_coercion_or_unknown_surrogates(self):
        invalid = (
            (0, 1), (1, True), (2, -1), (3, "1.5"),
            (3, 1e100), (4, "3.25"),
            (6, "AQL-"), (6, "AQJ="), (6, "AQL+===="),
            (7, base64.b64encode(bytes(15)).decode()),
            (8, [0] * 8), (8, [256] + [0] * 7),
            (9, False), (11, -1), (12, [1, True, 3]),
        )
        for index, replacement in invalid:
            with self.subTest(index=index, invalid=replacement):
                values = list(self.raw)
                values[index] = replacement
                with self.assertRaises(SchemaError):
                    decode_declared_arguments(self.envelope(values), self.schema)

    def test_float_lexical_underflow_and_overflow_rejected(self):
        root = {"$edp": {"v": 1, "type": "0000010000000001"},
                "value": {"0": [1, 0, 3], "12": [171] * 16,
                          "13": self.raw}}
        fixture = json.dumps(root, separators=(",", ":"))
        for original, replacement in (
            ("1.5", "1e-5000"), ("3.25", "1e-5000"),
            ("1.5", "1e5000"), ("3.25", "1e5000"),
        ):
            with self.subTest(original=original, replacement=replacement):
                with self.assertRaises((SchemaError, EnvelopeError)):
                    candidate = inspect_typed_envelope(
                        fixture.replace(original, replacement).encode(),
                        FrameCodec.Json,
                    )
                    decode_declared_arguments(candidate, self.schema)

    def test_no_implicit_schema_fingerprint_or_arity(self):
        with self.assertRaises(SchemaError):
            decode_declared_arguments(self.envelope(fingerprint=[0] * 16), self.schema)
        with self.assertRaises(SchemaError):
            decode_declared_arguments(self.envelope(fingerprint=[True] + [0] * 15), self.schema)
        with self.assertRaises(SchemaError):
            decode_declared_arguments(self.envelope(self.raw[:-1]), self.schema)


if __name__ == "__main__":
    unittest.main()
