"""Provenance-conservative diagnostic attempt and time comparison utilities.

A gap in observed attempt tickets is not evidence of where or why an entry
was refused, discarded, corrupted, or simply not captured. Synchronized time
is an opaque same-Era coordinate, NOT civil/UTC time.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Iterable

from .record_facts import RecordFacts

MAXIMUM_UNCERTAINTY = 0xFFFFFFFF


class EvidenceError(ValueError):
    """Input exceeds configured analysis limits or contradicts strong identity facts."""


class TemporalOrder(str, Enum):
    """Strong ordering only where record provenance conclusively permits it."""

    Earlier = "Earlier"
    Later = "Later"
    Overlaps = "Overlaps"
    Incomparable = "Incomparable"


@dataclass(frozen=True)
class AttemptEvidence:
    """Observed set only; missing ticket intervals are NOT identified loss causes."""

    device_identifier: bytes
    incarnation: int
    sequence_width: int
    observed_count: int
    duplicate_observations: int
    first_ticket: int
    last_ticket: int
    unseen_intervals: tuple[tuple[int, int], ...]
    partial_capture_possible: bool = True


def _identity(facts: RecordFacts) -> tuple[bytes, int]:
    return facts.device_identifier, facts.incarnation


def analyse_attempts(
    records: Iterable[RecordFacts], *,
    max_records: int = 65_536,
    max_groups: int = 1_024,
) -> tuple[AttemptEvidence, ...]:
    """Inspect bounded observed ticket sets by DeviceIdentifier+RuntimeIncarnation.

    Unseen intervals cover only gaps between the minimum and maximum actually
    captured ticket. Their counts cannot reveal a loss stage or distinguish
    admission rejection from source/window omission. No across-incarnation
    inference is allowed. Repeated tickets are counted separately as possible
    duplicate captures/fan-out rather than silently discarded.
    """
    if type(max_records) is not int or max_records <= 0 or (
        type(max_groups) is not int or max_groups <= 0
    ):
        raise ValueError("invalid bounded analysis resource policy")

    groups: dict[tuple[bytes, int], tuple[int, set[int], int]] = {}
    count = 0
    for record in records:
        count += 1
        if count > max_records:
            raise EvidenceError("attempt evidence exceeds configured record bound")

        key = _identity(record)
        if (
            type(record.device_identifier) is not bytes
            or len(record.device_identifier) != 16
            or not any(record.device_identifier)
            or type(record.incarnation) is not int
            or not 1 <= record.incarnation <= 0xFFFFFFFF
            or type(record.sequence_width) is not int
            or record.sequence_width not in (16, 32, 64)
        ):
            raise EvidenceError("invalid device/incarnation or attempt width evidence")
        if key not in groups:
            if len(groups) >= max_groups:
                raise EvidenceError("attempt evidence exceeds configured group bound")
            groups[key] = (record.sequence_width, set(), 0)

        width, tickets, duplicates = groups[key]
        if width != record.sequence_width:
            raise EvidenceError("one device incarnation advertised multiple sequence widths")
        if type(record.attempt_sequence) is not int or not (
            1 <= record.attempt_sequence <= (1 << width) - 1
        ):
            raise EvidenceError("invalid or exhausted attempt ticket observation")
        if record.attempt_sequence in tickets:
            duplicates += 1
        else:
            tickets.add(record.attempt_sequence)
        groups[key] = (width, tickets, duplicates)

    results: list[AttemptEvidence] = []
    for (device, incarnation), (width, tickets, duplicates) in sorted(
        groups.items(), key=lambda pair: (pair[0][0], pair[0][1])
    ):
        ordered = sorted(tickets)
        unseen: list[tuple[int, int]] = []
        for before, after in zip(ordered, ordered[1:]):
            if after > before + 1:
                unseen.append((before + 1, after - 1))
        results.append(AttemptEvidence(
            device_identifier=device,
            incarnation=incarnation,
            sequence_width=width,
            observed_count=len(ordered),
            duplicate_observations=duplicates,
            first_ticket=ordered[0],
            last_ticket=ordered[-1],
            unseen_intervals=tuple(unseen),
        ))
    return tuple(results)


def compare_record_time(left: RecordFacts, right: RecordFacts) -> TemporalOrder:
    """Compare two occurrences only when their monotonic or Era facts permit it.

    Within one device incarnation, local Monotonic coordinate is authoritative.
    Across devices/incarnations, a common nonzero Era and correlated synchronized
    coordinate with a nonsaturated uncertainty estimate is required, and
    interval overlap MUST NOT be advertised as a definite ordering.

    No assumption is made that the synchronized coordinate is UTC/civil time.
    """
    if _identity(left) == _identity(right):
        if left.monotonic_nanoseconds < right.monotonic_nanoseconds:
            return TemporalOrder.Earlier
        if left.monotonic_nanoseconds > right.monotonic_nanoseconds:
            return TemporalOrder.Later
        return TemporalOrder.Overlaps

    if (
        type(left.clock_era) is not int or type(right.clock_era) is not int
        or left.clock_era == 0 or left.clock_era != right.clock_era
        or left.synchronized_nanoseconds is None
        or right.synchronized_nanoseconds is None
        or left.synchronization_state not in (1, 2)
        or right.synchronization_state not in (1, 2)
        or type(left.uncertainty_nanoseconds) is not int
        or type(right.uncertainty_nanoseconds) is not int
        or left.uncertainty_nanoseconds < 0
        or right.uncertainty_nanoseconds < 0
        or left.uncertainty_nanoseconds >= MAXIMUM_UNCERTAINTY
        or right.uncertainty_nanoseconds >= MAXIMUM_UNCERTAINTY
    ):
        return TemporalOrder.Incomparable

    left_min = left.synchronized_nanoseconds - left.uncertainty_nanoseconds
    left_max = left.synchronized_nanoseconds + left.uncertainty_nanoseconds
    right_min = right.synchronized_nanoseconds - right.uncertainty_nanoseconds
    right_max = right.synchronized_nanoseconds + right.uncertainty_nanoseconds

    if left_max < right_min:
        return TemporalOrder.Earlier
    if right_max < left_min:
        return TemporalOrder.Later
    return TemporalOrder.Overlaps
