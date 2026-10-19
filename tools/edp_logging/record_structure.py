"""Strict structural EDP Typed Envelope inspection without historical schema assumptions.

Uses only stdlib; no arbitrary CBOR tags, indefinite containers, float coercion,
unknown runtime registries or implicit translation. Record arguments remain untyped.
"""

from __future__ import annotations

import json
import math
import re
import struct
from decimal import Decimal
from dataclasses import dataclass
from enum import Enum
from typing import Any

from .framing import FrameCodec


class EnvelopeError(ValueError):
    """Malformed canonical Typed Envelope or profile-limit violation."""


@dataclass(frozen=True)
class PreservedFloat:
    """Exact floating source width; no implicit binary32/binary64 equivalence."""

    value: float
    width_bits: int


@dataclass(frozen=True)
class TypedEnvelope:
    """Structurally validated Type identity and ordered numeric fields."""

    type_identifier: bytes
    fields: dict[int, Any]
    codec: FrameCodec


class _CborReader:
    """Deterministic definite-length CBOR subset with one global node budget."""

    def __init__(self, payload: bytes, maximum_depth: int, maximum_items: int) -> None:
        self.data = memoryview(payload)
        self.index = 0
        self.maximum_depth = maximum_depth
        self.remaining_items = maximum_items

    def _take(self, count: int) -> bytes:
        if count < 0 or count > len(self.data) - self.index:
            raise EnvelopeError("truncated CBOR item")
        value = bytes(self.data[self.index:self.index + count])
        self.index += count
        return value

    def _argument(self, additional: int) -> int:
        if additional < 24:
            return additional
        widths = {24: 1, 25: 2, 26: 4, 27: 8}
        if additional not in widths:
            raise EnvelopeError("indefinite or reserved CBOR additional information")
        width = widths[additional]
        value = int.from_bytes(self._take(width), "big")
        if (width == 1 and value < 24) or (
            width == 2 and value <= 0xFF
        ) or (width == 4 and value <= 0xFFFF) or (
            width == 8 and value <= 0xFFFFFFFF
        ):
            raise EnvelopeError("non-canonical CBOR integer/length")
        return value

    def parse(self, depth: int = 0) -> Any:
        if depth > self.maximum_depth or self.remaining_items <= 0:
            raise EnvelopeError("CBOR parser depth/node budget exceeded")
        self.remaining_items -= 1
        first = self._take(1)[0]
        major, ai = first >> 5, first & 31

        if major == 7:
            if ai == 20:
                return False
            if ai == 21:
                return True
            if ai == 22:
                return None
            if ai in (26, 27):
                width = 4 if ai == 26 else 8
                number = struct.unpack(">f" if width == 4 else ">d", self._take(width))[0]
                if not math.isfinite(number):
                    raise EnvelopeError("non-finite CBOR float")
                return PreservedFloat(number, width * 8)
            raise EnvelopeError("unsupported CBOR simple or float type")

        argument = self._argument(ai)
        if major == 0:
            return argument
        if major == 1:
            return -1 - argument
        if major == 2:
            return self._take(argument)
        if major == 3:
            try:
                value = self._take(argument).decode("utf-8", "strict")
            except UnicodeDecodeError as exc:
                raise EnvelopeError("invalid CBOR UTF-8 text") from exc
            if "\0" in value:
                raise EnvelopeError("CBOR UTF-8 text contains NUL")
            return value
        if major == 4:
            if argument > self.remaining_items:
                raise EnvelopeError("CBOR array exceeds item budget")
            return [self.parse(depth + 1) for _ in range(argument)]
        if major == 5:
            if argument * 2 > self.remaining_items:
                raise EnvelopeError("CBOR map exceeds item budget")
            items: dict[int, Any] = {}
            last_key = -1
            for _ in range(argument):
                key = self.parse(depth + 1)
                if type(key) is not int or key < 0 or key <= last_key:
                    raise EnvelopeError("CBOR map fields must be strictly ascending unsigned keys")
                items[key] = self.parse(depth + 1)
                last_key = key
            return items
        raise EnvelopeError("unsupported CBOR major type")

    def completed(self) -> bool:
        return self.index == len(self.data)


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise EnvelopeError(f"duplicate JSON member {key!r}")
        result[key] = value
    return result


def _reject_nonfinite(_: str) -> None:
    raise EnvelopeError("non-finite JSON numeric token")


def _validate_json(value: Any, depth: int, budget: list[int], max_depth: int) -> None:
    if depth > max_depth or budget[0] <= 0:
        raise EnvelopeError("JSON depth/node budget exceeded")
    budget[0] -= 1
    if type(value) is Decimal:
        if not value.is_finite():
            raise EnvelopeError("non-finite JSON floating value")
        native = float(value)
        if not math.isfinite(native) or (value != 0 and native == 0):
            raise EnvelopeError("JSON numeric overflow or lexical underflow")
    if type(value) is float and not math.isfinite(value):
        raise EnvelopeError("non-finite JSON floating value")
    if isinstance(value, dict):
        if len(value) * 2 > budget[0]:
            raise EnvelopeError("JSON map exceeds item budget")
        for key, item in value.items():
            if "\0" in key:
                raise EnvelopeError("JSON object key contains NUL")
            _validate_json(item, depth + 1, budget, max_depth)
    elif isinstance(value, list):
        if len(value) > budget[0]:
            raise EnvelopeError("JSON array exceeds item budget")
        for item in value:
            _validate_json(item, depth + 1, budget, max_depth)
    elif type(value) is str and "\0" in value:
        raise EnvelopeError("JSON text contains NUL")


def inspect_typed_envelope(
    payload: bytes,
    codec: FrameCodec,
    *,
    max_depth: int = 16,
    max_items: int = 1024,
) -> TypedEnvelope:
    """Decode only canonical Typed Envelope structure, never guessed parameter types."""
    if max_depth <= 0 or max_items <= 0:
        raise ValueError("invalid structural parser limits")

    if codec == FrameCodec.Cbor:
        reader = _CborReader(payload, max_depth, max_items)
        root = reader.parse()
        if not reader.completed():
            raise EnvelopeError("trailing CBOR input")
        if (
            type(root) is not list or len(root) != 3 or
            type(root[0]) is not int or root[0] != 1 or
            type(root[1]) is not bytes or len(root[1]) != 8 or
            type(root[2]) is not dict
        ):
            raise EnvelopeError("expected CBOR Typed Envelope [1, bytes8, numeric-map]")
        if not any(root[1]):
            raise EnvelopeError("zero TypeIdentifier is invalid")
        for key in root[2]:
            if type(key) is not int or key > 255:
                raise EnvelopeError("FieldIdentifier outside 0..255")
        return TypedEnvelope(root[1], root[2], codec)

    if codec == FrameCodec.Json:
        try:
            root = json.loads(
                payload.decode("utf-8", "strict"),
                object_pairs_hook=_unique_object,
                parse_float=Decimal,
                parse_constant=_reject_nonfinite,
            )
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise EnvelopeError("invalid JSON Typed Envelope") from exc
        _validate_json(root, 0, [max_items], max_depth)
        if type(root) is not dict or set(root) != {"$edp", "value"}:
            raise EnvelopeError("expected JSON Typed Envelope metadata and value")
        metadata = root["$edp"]
        if type(metadata) is not dict or set(metadata) != {"v", "type"}:
            raise EnvelopeError("malformed JSON envelope metadata")
        identifier = metadata["type"]
        if type(metadata["v"]) is not int or metadata["v"] != 1 or (
            type(identifier) is not str or re.fullmatch("[0-9a-f]{16}", identifier) is None
        ):
            raise EnvelopeError("unsupported JSON envelope version or TypeIdentifier")
        type_bytes = bytes.fromhex(identifier)
        if not any(type_bytes):
            raise EnvelopeError("zero TypeIdentifier is invalid")
        fields_json = root["value"]
        if type(fields_json) is not dict:
            raise EnvelopeError("JSON Typed Envelope body must be a numeric field object")
        fields: dict[int, Any] = {}
        for name, value in fields_json.items():
            if re.fullmatch(r"(?:0|[1-9][0-9]*)", name) is None:
                raise EnvelopeError("noncanonical JSON FieldIdentifier")
            identifier_value = int(name)
            if identifier_value > 255:
                raise EnvelopeError("JSON FieldIdentifier outside 0..255")
            fields[identifier_value] = value
        return TypedEnvelope(type_bytes, dict(sorted(fields.items())), codec)

    raise EnvelopeError("unsupported Typed Envelope codec")
