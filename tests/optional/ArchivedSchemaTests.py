from __future__ import annotations

import contextlib
import hashlib
import io
import json
import tempfile
import unittest
from pathlib import Path

from edp_localisation.compiler import compile_generated_set
from edp_localisation.historical_archive import archive_generated_family
from edp_localisation.common import ToolError
import test_toolchain

from edp_logging.cli import main
from edp_logging.framing import FrameCodec, encode_frame
from edp_logging.historical_schema import load_archived_diagnostic_schema
from edp_logging.schema import SchemaError


class ArchivedSchemaBridgeTests(unittest.TestCase):
    @staticmethod
    def fixture(root: Path) -> tuple[Path, dict]:
        source, platform, schema = test_toolchain.ToolchainTests().make_fixture(root)
        catalogue_path = source / "strings_catalogue.json"
        catalogue = json.loads(catalogue_path.read_text("utf-8"))
        catalogue["domains"][0]["subDomains"][0]["strings"][0]["parameters"] = [
            {"index": 0, "status": "active", "type": "u16", "symbol": "Count"}
        ]
        test_toolchain.write_json(catalogue_path, catalogue)
        output = root / "generated"
        compile_generated_set(source, platform, [schema], output, "Fixture::Historical")
        archive = root / "archive"
        pinned = archive_generated_family(output, archive)
        return source, {"archive":archive, **pinned}

    def test_integrity_only_and_independent_manifest_pin(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            _, info = self.fixture(root)
            archive = info["archive"]
            evidence = load_archived_diagnostic_schema(
                archive, info["fingerprint"], contract_family_digest=info["digest"]
            )
            self.assertFalse(evidence.schema.artifact_sha256_pin_matched)
            self.assertFalse(evidence.manifest_sha256_pin_matched)
            message = evidence.schema.messages[(1,0,3)]
            self.assertEqual(message.parameters[0].type_name, "u16")
            historical_text = evidence.resolve_template(
                "de", (1,0,3),
                expected_diagnostic_fingerprint=evidence.schema.fingerprint
            )
            self.assertEqual(historical_text.template, "Hello")
            self.assertEqual(historical_text.supplying_language, "en-GB")
            self.assertTrue(historical_text.fallback_used)
            self.assertFalse(historical_text.interpolated)
            with self.assertRaises(SchemaError):
                evidence.resolve_template(
                    "de", (1,0,3),
                    expected_diagnostic_fingerprint=bytes(16)
                )
            with self.assertRaises(SchemaError):
                evidence.resolve_template(
                    "de", (1,0,3),
                    expected_diagnostic_fingerprint=evidence.schema.fingerprint,
                    max_template_bytes=1
                )
            manifest = (archive / "families" / info["digest"] /
                        "localisation-build-manifest.json").read_bytes()
            pin = hashlib.sha256(manifest).digest()
            trusted = load_archived_diagnostic_schema(
                archive, info["fingerprint"],
                contract_family_digest=info["digest"],
                trusted_manifest_sha256=pin
            )
            self.assertTrue(trusted.schema.artifact_sha256_pin_matched)
            self.assertTrue(trusted.manifest_sha256_pin_matched)
            with self.assertRaises(SchemaError):
                load_archived_diagnostic_schema(
                    archive, info["fingerprint"],
                    trusted_manifest_sha256=bytes(32)
                )
            with self.assertRaises(SchemaError):
                load_archived_diagnostic_schema(
                    archive, info["fingerprint"],
                    trusted_manifest_sha256=b"short"
                )
            with self.assertRaises(SchemaError):
                load_archived_diagnostic_schema(
                    archive, "0"*32
                )

    def test_same_diagnostic_identity_distinct_translation_family_requires_choice(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            source, info = self.fixture(root)
            old = load_archived_diagnostic_schema(
                info["archive"], info["fingerprint"]
            )
            strings_path = source / "en-GB" / "strings.json"
            text = json.loads(strings_path.read_text("utf-8"))
            text["domains"]["1"]["subDomains"]["0"]["strings"]["3"] = "Changed translation"
            test_toolchain.write_json(strings_path,text)
            # Use original source inputs; only translation text has changed.
            generated = root / "second"
            compile_generated_set(
                source,
                root / "platform",
                [root / "schema.json"],
                generated,
                "Fixture::Historical"
            )
            second = archive_generated_family(generated,info["archive"])
            self.assertNotEqual(info["fingerprint"],second["fingerprint"])
            newer = load_archived_diagnostic_schema(info["archive"],second["fingerprint"])
            self.assertEqual(old.schema.fingerprint,newer.schema.fingerprint)
            self.assertNotEqual(
                old.contract_family_fingerprint,
                newer.contract_family_fingerprint
            )
            self.assertEqual(
                old.resolve_template(
                    "en-GB",(1,0,3),
                    expected_diagnostic_fingerprint=old.schema.fingerprint
                ).template,
                "Hello"
            )
            self.assertEqual(
                newer.resolve_template(
                    "en-GB",(1,0,3),
                    expected_diagnostic_fingerprint=newer.schema.fingerprint
                ).template,
                "Changed translation"
            )
            self.assertEqual(
                old.schema.messages[(1,0,3)].parameters,
                newer.schema.messages[(1,0,3)].parameters
            )
            with self.assertRaises(SchemaError):
                load_archived_diagnostic_schema(
                    info["archive"], info["fingerprint"],
                    contract_family_digest=second["digest"]
                )

    def test_cli_archive_lookup_is_explicit_and_never_claims_governed_root(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            _, info = self.fixture(root)
            evidence = load_archived_diagnostic_schema(
                info["archive"],info["fingerprint"]
            )
            # Synthetic nonzero root TypeIdentifier is TEST DATA ONLY.
            # Three definite map entries: Field0 StringId, Field12 schema, Field13 arguments.
            framed_payload = (
                bytes([0x83,0x01,0x48]) +
                bytes(range(1,9)) +
                bytes([0xA3,0x00,0x83,0x01,0x00,0x03,0x0C,0x50]) +
                evidence.schema.fingerprint +
                bytes([0x0D,0x81,0x07])
            )
            captured = root / "captured.edlg"
            captured.write_bytes(encode_frame(FrameCodec.Cbor, framed_payload))
            cmd = [
                str(captured),
                "--historical-archive", str(info["archive"]),
                "--contract-family-fingerprint", info["fingerprint"],
                "--contract-family-digest", info["digest"],
            ]
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                code=main(cmd)
            self.assertEqual(code,0,output.getvalue())
            report=json.loads(output.getvalue())
            self.assertEqual(report["parameterKinds"],["u16"])
            self.assertTrue(report["parameterSchemaInterpreted"])
            self.assertFalse(report["schemaArtifactSha256PinMatched"])
            self.assertFalse(report["historicalManifestTrustedPinMatched"])
            self.assertEqual(report["historicalContractFamilyFingerprint"],info["fingerprint"])
            self.assertNotIn("rootTypeGoverned",report)

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    main(cmd+["--historical-language","de"]),0
                )
            localized_report=json.loads(output.getvalue())
            self.assertEqual(localized_report["historicalTemplate"],"Hello")
            self.assertTrue(localized_report["historicalTemplateFallbackUsed"])
            self.assertEqual(
                localized_report["historicalTemplateSupplyingLanguage"],"en-GB"
            )
            self.assertFalse(localized_report["historicalTemplateInterpolated"])

            output=io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    main(cmd+["--historical-language","de",
                        "--max-historical-template-bytes","1"]),1
                )
            self.assertIn("historicalTemplateError",json.loads(output.getvalue()))

            manifest_bytes = (
                info["archive"] / "families" / info["digest"] /
                "localisation-build-manifest.json"
            ).read_bytes()
            pin = hashlib.sha256(manifest_bytes).hexdigest().upper()
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    main(cmd + ["--trusted-archive-manifest-sha256", pin]), 0
                )
            trusted_report = json.loads(output.getvalue())
            self.assertTrue(trusted_report["schemaArtifactSha256PinMatched"])
            self.assertTrue(trusted_report["historicalManifestTrustedPinMatched"])

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    main(cmd + ["--trusted-archive-manifest-sha256", "00" * 32]), 2
                )
            self.assertEqual(
                json.loads(output.getvalue())["kind"], "schema-error"
            )

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(main(cmd+["--schema",str(captured)]),2)
            self.assertEqual(json.loads(output.getvalue())["kind"],"schema-error")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(main([
                    str(captured),"--historical-archive",str(info["archive"])
                ]),2)
            self.assertEqual(json.loads(output.getvalue())["kind"],"schema-error")

if __name__ == "__main__":
    unittest.main()
