"""Optional offline G5 bridge from authoritative Localisation archives.

An archived diagnostic-schema.json is a separate schema identity from its
translation-sensitive ContractFamily. The caller MUST choose the precise
historical ContractFamily; a record's diagnostic fingerprint alone cannot
reconstruct its language or distinguish translation-only family revisions.
"""

from __future__ import annotations

import hashlib
import hmac
from dataclasses import dataclass
from typing import Any
from pathlib import Path

from .schema import DiagnosticSchema, SchemaError, load_generated_schema


@dataclass(frozen=True)
class HistoricalTemplateEvidence:
    requested_language: str
    supplying_language: str
    fallback_used: bool
    template: str
    interpolated: bool = False


@dataclass(frozen=True)
class HistoricalDiagnosticEvidence:
    schema: DiagnosticSchema
    contract_family_fingerprint: bytes
    contract_family_digest: bytes
    manifest_sha256_pin_matched: bool
    _family: Any

    def resolve_template(
        self,
        language: str,
        message_identity: tuple[int, int, int],
        *,
        expected_diagnostic_fingerprint: bytes,
        max_template_bytes: int = 65_536,
    ) -> HistoricalTemplateEvidence:
        """Resolve exact historical *template*; never format record arguments.

        Caller separately validates Field12 and positional values using
        decode_declared_arguments. This cannot invent a LogRecord root ID.
        """
        from edp_localisation.common import ToolError
        from edp_localisation.diagnostic_templates import (
            DiagnosticTemplateError, parse_diagnostic_template,
        )
        if (
            type(expected_diagnostic_fingerprint) is not bytes
            or expected_diagnostic_fingerprint != self.schema.fingerprint
        ):
            raise SchemaError("record diagnostic schema fingerprint differs from historical family")
        if (
            type(message_identity) is not tuple or len(message_identity) != 3
            or any(type(part) is not int for part in message_identity)
        ):
            raise SchemaError("historical message identity must be three exact integers")
        message = self.schema.messages.get(message_identity)
        if message is None or message.status != "active" or not message.diagnostic_declared:
            raise SchemaError("historical template message is not an active declared diagnostic")
        if type(max_template_bytes) is not int or max_template_bytes <= 0:
            raise SchemaError("historical template maximum must be positive")
        try:
            resolved = self._family.resolve_string(language, *message_identity)
            if resolved.status != "Success" or resolved.value is None or resolved.supplying_language is None:
                raise SchemaError("selected historical family has no template for the message")
            parse_diagnostic_template(resolved.value, len(message.parameters))
            if len(resolved.value.encode("utf-8")) > max_template_bytes:
                raise SchemaError("historical template exceeds configured byte limit")
        except (ToolError, DiagnosticTemplateError, UnicodeError) as error:
            raise SchemaError(f"invalid historical diagnostic presentation: {error}") from error
        return HistoricalTemplateEvidence(
            requested_language=resolved.requested_language,
            supplying_language=resolved.supplying_language,
            fallback_used=resolved.fallback_used,
            template=resolved.value,
        )


def load_archived_diagnostic_schema(
    archive_root: Path,
    contract_family_fingerprint: str,
    *,
    contract_family_digest: str | None = None,
    trusted_manifest_sha256: bytes | None = None,
) -> HistoricalDiagnosticEvidence:
    """Verify the owner archive, then reuse Logging's schema decoder.

    Without an independent trust pin, all hashes show *integrity only*.
    Neither this helper nor a matching pin proves a governed LogRecord root.
    """
    try:
        from edp_localisation.common import ToolError
        from edp_localisation.historical_archive import open_historical_family
    except ImportError as error:
        raise SchemaError(
            "optional historical schema requires EDP-Localisation tooling"
        ) from error

    try:
        family = open_historical_family(
            Path(archive_root),
            contract_family_fingerprint,
            digest=contract_family_digest,
        )
    except (ToolError, OSError) as error:
        raise SchemaError(f"Localisation historical family verification failed: {error}") from error

    manifest_path = family.root / "localisation-build-manifest.json"
    manifest_bytes = manifest_path.read_bytes()
    externally_pinned = trusted_manifest_sha256 is not None
    if externally_pinned:
        if (
            type(trusted_manifest_sha256) is not bytes
            or len(trusted_manifest_sha256) != 32
        ):
            raise SchemaError("trusted historical manifest SHA-256 must be exactly 32 bytes")
        if not hmac.compare_digest(
            hashlib.sha256(manifest_bytes).digest(), trusted_manifest_sha256
        ):
            raise SchemaError("historical manifest differs from independently trusted SHA-256 pin")

    declared = [
        output for output in family.manifest["outputs"]
        if output["path"] == "diagnostic-schema.json"
        and output["kind"] == "diagnostic-schema"
    ]
    if len(declared) != 1:
        raise SchemaError("archive has no single governed diagnostic schema output")
    raw = (family.root / "diagnostic-schema.json").read_bytes()

    # Internal checks were already performed by Localisation's canonical
    # verifier. The recorded artifact hash becomes a TRUSTED pin only when
    # the parent manifest itself is independently authenticated.
    schema = load_generated_schema(
        raw,
        trusted_artifact_sha256=(
            bytes.fromhex(declared[0]["sha256"]) if externally_pinned else None
        ),
    )
    if (
        schema.fingerprint.hex().upper() !=
            family.manifest["diagnosticSchemaFingerprint"]
        or schema.digest.hex().upper() !=
            family.manifest["diagnosticSchemaDigestSha256"]
    ):
        raise SchemaError("archived diagnostic schema identity contradicts owner manifest")

    return HistoricalDiagnosticEvidence(
        schema=schema,
        contract_family_fingerprint=family.fingerprint,
        contract_family_digest=bytes.fromhex(
            family.manifest["contractFamilyDigestSha256"]
        ),
        manifest_sha256_pin_matched=externally_pinned,
        _family=family,
    )
