"""Validated portable LogRecord V1 outer facts.

No unknown root TypeIdentifier or opaque flags/provenance semantics are invented.
Validation here establishes well-formed *candidate* record facts, not authenticity,
durability, translation, or a complete decoded chronological assessment.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from .record_structure import TypedEnvelope


class RecordFactError(ValueError):
    """Invalid frozen LogRecord field mapping or contradictory identity/clock facts."""


@dataclass(frozen=True)
class RecordFacts:
    """Known outer LogRecord V1 facts, with unknown provider-owned facts preserved."""

    message_identifier: tuple[int, int, int]
    severity: int
    device_identifier: bytes
    incarnation: int
    attempt_sequence: int
    sequence_width: int
    monotonic_nanoseconds: int
    synchronized_nanoseconds: int | None
    synchronization_state: int
    clock_era: int
    uncertainty_nanoseconds: int
    diagnostic_fingerprint: bytes
    positional_arguments: list[Any]
    temporal_quality: tuple[int, int]  # Owner Clock projection, then assessment
    record_flags: int  # Field14 zero-only v1
    type_identifier: bytes


_REQUIRED = frozenset((0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14))
_KNOWN = _REQUIRED | {7}


def _unsigned(value: Any, maximum: int, label: str, minimum: int = 0) -> int:
    if type(value) is not int or not minimum <= value <= maximum:
        raise RecordFactError(f"invalid or out-of-range {label}")
    return value


def inspect_log_record_facts(
    envelope: TypedEnvelope,
    *,
    expected_root_type_identifier: bytes | None = None,
) -> RecordFacts:
    """Check D13 FieldIds 0..14 and approved Clock/validity child semantics.

    EDP-System formally issued the permanent LogRecord root
    00 00 01 00 00 00 00 01 on 2026-10-17. This parser intentionally accepts
    an explicit expected_root_type_identifier from an independently governed
    source. Without that expected ID it validates only a structural forensic
    candidate and must NOT label any arbitrary Type as LogRecord.
    A matching 8-byte ID alone does not authenticate the producer, schema
    manifest, or historical record payload.
    """
    fields = envelope.fields
    if not _REQUIRED <= fields.keys() or not fields.keys() <= _KNOWN:
        raise RecordFactError("LogRecord missing required or containing unregistered FieldId")
    if expected_root_type_identifier is not None and (
        type(expected_root_type_identifier) is not bytes
        or len(expected_root_type_identifier) != 8
        or envelope.type_identifier != expected_root_type_identifier
    ):
        raise RecordFactError("governed LogRecord root TypeIdentifier mismatch")
    message = fields[0]
    if type(message) is not list or len(message) != 3:
        raise RecordFactError("StringIdentifier must be a three-element canonical tuple")
    # Domain/SubDomain/String widths are taken from the *matching historical schema*;
    # this outer-only inspection conservatively permits up to 32-bit members.
    identifiers = tuple(
        _unsigned(value, 0xFFFFFFFF, "StringIdentifier component")
        for value in message
    )
    if identifiers[2] < 3:
        raise RecordFactError("ordinary logging StringId is in reserved metadata range")

    severity = _unsigned(fields[1], 5, "emitted severity")
    device = fields[2]
    if type(device) is not bytes or len(device) != 16 or not any(device):
        raise RecordFactError("invalid 16-byte DeviceIdentifier")
    incarnation = _unsigned(fields[3], 0xFFFFFFFF, "RuntimeIncarnationId", 1)
    width = fields[5]
    if type(width) is not int or width not in (16, 32, 64):
        raise RecordFactError("unknown selected attempt sequence width")
    sequence = _unsigned(fields[4], (1 << width) - 1, "per-incarnation sequence", 1)
    monotonic = _unsigned(fields[6], 0xFFFFFFFFFFFFFFFF, "MonotonicTimestamp")
    synchronized = fields.get(7)
    if synchronized is not None:
        synchronized = _unsigned(synchronized, 0xFFFFFFFFFFFFFFFF, "SynchronizedTimestamp")
    state = _unsigned(fields[8], 4, "Clock SynchronizationState")
    era = _unsigned(fields[9], 0xFFFFFFFFFFFFFFFF, "ClockEra")
    uncertainty = _unsigned(fields[10], 0xFFFFFFFF, "SynchronizationUncertainty")

    # A synchronized coordinate of *zero* is valid; availability is encoded by
    # field presence, not a numeric sentinel. Known correlation requires Era.
    if synchronized is not None and era == 0:
        raise RecordFactError("synchronized coordinate present without valid ClockEra")
    if synchronized is not None and state == 0:
        raise RecordFactError("NeverSynchronized state cannot claim a synchronized coordinate")

    fingerprint = fields[12]
    if type(fingerprint) is not bytes or len(fingerprint) != 16:
        raise RecordFactError("DiagnosticSchemaFingerprint must be exactly 16 bytes")
    arguments = fields[13]
    if type(arguments) is not list:
        raise RecordFactError("positional arguments must be a definite array")

    # These two v1 child domains are explicitly approved; never reinterpret
    # unknown Clock codes or treat reserved validity facts as valid history.
    provenance = fields[11]
    if type(provenance) is not list or len(provenance) != 2:
        raise RecordFactError("Field11 must be two ordered Clock owner statuses")
    projection = _unsigned(provenance[0], 3, "ClockCorrelationProjectionStatus")
    assessment = _unsigned(provenance[1], 2, "TemporalQualityAssessment")
    facts = _unsigned(fields[14], 0, "Field14 reserved v1 validity facts")
    return RecordFacts(
        identifiers, severity, device, incarnation, sequence, width,
        monotonic, synchronized, state, era, uncertainty,
        fingerprint, arguments, (projection, assessment), facts,
        envelope.type_identifier,
    )
