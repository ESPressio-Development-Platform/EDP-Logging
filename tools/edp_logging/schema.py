"""Strict historical diagnostic schema loader for Localisation-generated JSON.

No language pack is required for typed CBOR argument interpretation. This
loader verifies schema identity syntax, not cryptographic origin/authentication;
a separately pinned build manifest or trusted catalogue supplies provenance.
"""

from __future__ import annotations

import json
import re
import math
from dataclasses import dataclass
from typing import Any

from .record_structure import PreservedFloat, TypedEnvelope
from .framing import FrameCodec

KINDS = frozenset({
    "bool", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64",
    "f32", "f64", "utf8", "bytes",
    "type-identifier", "field-identifier", "string-identifier",
    "device-identifier", "runtime-incarnation-id", "clock-era",
})


class SchemaError(ValueError):
    """Invalid, unsupported or mismatched diagnostic schema artefact."""


@dataclass(frozen=True)
class Parameter:
    """Index and portable scalar kind declared by the authoritative catalogue."""

    index: int
    type_name: str
    status: str


@dataclass(frozen=True)
class Message:
    """One historical String's exact governed positional diagnostic signature."""

    identity: tuple[int, int, int]
    status: str
    diagnostic_declared: bool
    parameters: tuple[Parameter, ...]


@dataclass(frozen=True)
class DiagnosticSchema:
    """Historical argument schema and its declared independent identity."""

    fingerprint: bytes
    digest: bytes
    messages: dict[tuple[int, int, int], Message]


@dataclass(frozen=True)
class DecodedArguments:
    """Validated parameter meanings, never guessed without a matching schema."""

    message: Message
    values: tuple[Any, ...]


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for name, item in pairs:
        if name in value:
            raise SchemaError(f"duplicate diagnostic schema property {name!r}")
        value[name] = item
    return value


def _reject_nonfinite(_: str) -> None:
    raise SchemaError("non-finite JSON schema token")


def _bounded_unsigned(value: object, upper: int, name: str) -> int:
    if type(value) is not int or value < 0 or value > upper:
        raise SchemaError(f"{name} must be an unsigned integer in the configured range")
    return value


def load_generated_schema(data: bytes, *, max_source_bytes: int = 2_097_152,
                          max_messages: int = 65_536) -> DiagnosticSchema:
    """Read a finite generated artefact; preserve original digest provenance externally."""
    if max_source_bytes < 0 or len(data) > max_source_bytes or max_messages < 0:
        raise SchemaError("schema source exceeds limits")
    try:
        document = json.loads(
            data.decode("utf-8", "strict"),
            object_pairs_hook=_unique_object,
            parse_constant=_reject_nonfinite,
        )
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise SchemaError("invalid diagnostic schema JSON") from exc
    if type(document) is not dict or (
        type(document.get("schemaVersion")) is not int
        or document["schemaVersion"] != 1
        or document.get("kind") != "edp-diagnostic-schema"
        or set(document) != {
            "schemaVersion", "kind", "diagnosticSchemaFingerprint",
            "diagnosticSchemaDigestSha256", "identifierWidths", "messages"
        }
    ):
        raise SchemaError("unsupported diagnostic schema source/version")

    fingerprint_hex = document.get("diagnosticSchemaFingerprint")
    digest_hex = document.get("diagnosticSchemaDigestSha256")
    if (
        type(fingerprint_hex) is not str or re.fullmatch("[0-9A-F]{32}", fingerprint_hex) is None
        or type(digest_hex) is not str or re.fullmatch("[0-9A-F]{64}", digest_hex) is None
        or digest_hex[:32] != fingerprint_hex
    ):
        raise SchemaError("invalid or inconsistent diagnostic schema fingerprint/digest")
    widths = document.get("identifierWidths")
    if type(widths) is not dict or set(widths) != {"domain", "subDomain", "string"}:
        raise SchemaError("invalid schema identity-width declaration")
    for value in widths.values():
        if type(value) is not int or value not in (1, 2, 4):
            raise SchemaError("unsupported schema identifier width")
    bounds = tuple((1 << (widths[name] * 8)) - 1 for name in ("domain", "subDomain", "string"))

    entries = document.get("messages")
    if type(entries) is not list or len(entries) > max_messages:
        raise SchemaError("invalid or unbounded governed message collection")
    messages: dict[tuple[int, int, int], Message] = {}
    previous: tuple[int, int, int] | None = None
    for entry in entries:
        if type(entry) is not dict or set(entry) != {
            "domain", "subDomain", "string", "status", "diagnosticDeclared", "parameters"
        }:
            raise SchemaError("malformed governed String declaration")
        identity = tuple(
            _bounded_unsigned(entry[name], bounds[index], name)
            for index, name in enumerate(("domain", "subDomain", "string"))
        )
        if identity[2] < 3 or (previous is not None and identity <= previous):
            raise SchemaError("diagnostic Strings must be unique, canonical sorted and non-reserved")
        previous = identity
        if (
            type(entry["status"]) is not str
            or entry["status"] not in ("active", "retired")
            or type(entry["diagnosticDeclared"]) is not bool
        ):
            raise SchemaError("invalid governed diagnostic lifecycle/declaration")
        raw = entry["parameters"]
        if type(raw) is not list or len(raw) > 255:
            raise SchemaError("invalid or oversized governed parameter signature")
        parameters: list[Parameter] = []
        for index, p in enumerate(raw):
            if type(p) is not dict or set(p) != {"index", "status", "type"}:
                raise SchemaError("malformed parameter declaration")
            if type(p["index"]) is not int or p["index"] != index:
                raise SchemaError("noncontiguous diagnostic parameter index")
            if (
                type(p["status"]) is not str
                or p["status"] not in ("active", "retired")
                or type(p["type"]) is not str
                or p["type"] not in KINDS
            ):
                raise SchemaError("unknown diagnostic parameter lifecycle or type")
            if entry["status"] == "active" and p["status"] != "active":
                raise SchemaError("active diagnostic String contains retired parameter")
            parameters.append(Parameter(index, p["type"], p["status"]))
        if parameters and not entry["diagnosticDeclared"]:
            raise SchemaError("undeclared String cannot carry diagnostic parameters")
        messages[identity] = Message(
            identity, entry["status"], entry["diagnosticDeclared"], tuple(parameters)
        )
    return DiagnosticSchema(bytes.fromhex(fingerprint_hex), bytes.fromhex(digest_hex), messages)


def _argument_matches(kind: str, value: Any) -> bool:
    if kind == "bool":
        return type(value) is bool
    if kind.startswith(("u", "i")) and kind[1:].isdigit():
        signed = kind[0] == "i"
        bits = int(kind[1:])
        if type(value) is not int:
            return False
        minimum = -(1 << (bits - 1)) if signed else 0
        maximum = (1 << (bits - 1)) - 1 if signed else (1 << bits) - 1
        return minimum <= value <= maximum
    if kind in ("f32", "f64"):
        return isinstance(value, PreservedFloat) and value.width_bits == int(kind[1:])
    if kind == "utf8":
        return type(value) is str
    if kind == "bytes":
        return type(value) is bytes
    if kind == "type-identifier":
        return type(value) is bytes and len(value) == 8 and any(value)
    if kind == "device-identifier":
        return type(value) is bytes and len(value) == 16 and any(value)
    if kind == "field-identifier":
        return type(value) is int and 0 <= value <= 255
    if kind == "string-identifier":
        return type(value) is list and len(value) == 3 and all(
            type(item) is int and item >= 0 for item in value
        )
    if kind == "runtime-incarnation-id":
        return type(value) is int and 1 <= value <= 0xFFFFFFFF
    if kind == "clock-era":
        return type(value) is int and 0 <= value <= 0xFFFFFFFFFFFFFFFF
    return False


def decode_declared_arguments(
    record: TypedEnvelope, schema: DiagnosticSchema
) -> DecodedArguments:
    """Validate portable CBOR positional arguments against an exact historical schema.

    JSON opaque-identifier byte encoding and cross-format argument semantics remain
    unfinalised; reject rather than guessing their representation.
    """
    if record.codec != FrameCodec.Cbor:
        raise SchemaError("typed JSON argument validation not yet implemented")
    identity_value = record.fields.get(0)
    schema_value = record.fields.get(12)
    values = record.fields.get(13)
    if (
        type(identity_value) is not list or len(identity_value) != 3
        or any(type(v) is not int or v < 0 for v in identity_value)
        or type(schema_value) is not bytes or len(schema_value) != 16
        or type(values) is not list
    ):
        raise SchemaError("missing or malformed StringIdentifier/fingerprint/argument array")
    if schema_value != schema.fingerprint:
        raise SchemaError("unknown or mismatched diagnostic schema fingerprint")
    identity = tuple(identity_value)
    message = schema.messages.get(identity)
    if message is None or message.status != "active" or not message.diagnostic_declared:
        raise SchemaError("message is not governed as a diagnostic String")
    if len(values) != len(message.parameters):
        raise SchemaError("diagnostic argument arity mismatch")
    if any(p.status != "active" for p in message.parameters):
        raise SchemaError("cannot decode retired diagnostic parameter as active")
    for parameter, value in zip(message.parameters, values):
        if not _argument_matches(parameter.type_name, value):
            raise SchemaError(
                f"argument {parameter.index} is not the governed {parameter.type_name} type"
            )
    return DecodedArguments(message, tuple(values))
