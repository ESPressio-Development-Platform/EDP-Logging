"""Read-only, bounded conservative inspection of fixed EDLS archive slots.

This module NEVER appends behind a torn tail, chooses a storage write
destination, claims Flash atomicity, or supplies durability evidence. It
recovers validated *prefixes* only. The production EDP-Persistence-backed
archive coordinator remains a separate, not-yet-implemented component.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Sequence

from .archive import SEGMENT_HEADER_SIZE, SegmentHeader, SegmentHeaderError, parse_segment_header, parse_sealed_segment
from .framing import Frame, FrameIssue, FrameStatus, scan_stream


class SlotState(str, Enum):
    """Finite source-file observations for one configured physical archive slot."""

    Empty = "Empty"
    Clean = "Clean"
    DamagedTail = "DamagedTail"
    InvalidHeader = "InvalidHeader"
    SlotMismatch = "SlotMismatch"
    ReadFailure = "ReadFailure"


@dataclass(frozen=True)
class SlotInspection:
    """One read-only slot result; good_end is the end of verified contiguous bytes."""

    slot: int
    path: Path
    state: SlotState
    header: SegmentHeader | None
    verified_frames: int
    good_end: int
    first_issue: FrameStatus | None


@dataclass(frozen=True)
class ArchiveInspection:
    """A qualified, not necessarily complete view of an archive's generations."""

    slots: tuple[SlotInspection, ...]
    newest_known_slot: int | None
    order_unambiguous: bool
    next_generation: int | None
    explanations: tuple[str, ...]


def inspect_slot(
    path: Path,
    expected_slot: int,
    *,
    max_payload: int = 65_536,
    max_segment_bytes: int = 1_048_576,
) -> SlotInspection:
    """Inspect read-only; sealed bodies use explicitly bounded whole-file validation."""
    if type(expected_slot) is not int or not 0 <= expected_slot <= 0xFFFF:
        raise ValueError("physical slot index outside u16 range")
    if not 0 <= max_payload <= 0xFFFFFFFF:
        raise ValueError("invalid archive frame payload bound")

    try:
        with path.open("rb") as source:
            raw = source.read(SEGMENT_HEADER_SIZE)
            if len(raw) == 0:
                return SlotInspection(
                    expected_slot, path, SlotState.Empty, None, 0, 0, None
                )
            try:
                header = parse_segment_header(raw)
            except SegmentHeaderError:
                return SlotInspection(
                    expected_slot, path, SlotState.InvalidHeader, None, 0, 0, None
                )
            if header.slot != expected_slot:
                return SlotInspection(
                    expected_slot, path, SlotState.SlotMismatch, header, 0, SEGMENT_HEADER_SIZE, None
                )

            if header.profile == 1:
                # A sealed batch has no independently acceptable partial prefix.
                if type(max_segment_bytes) is not int or max_segment_bytes < 76:
                    raise ValueError("invalid sealed segment byte bound")
                body = source.read(max_segment_bytes + 1 - SEGMENT_HEADER_SIZE)
                try:
                    _, frames = parse_sealed_segment(
                        raw + body, max_payload=max_payload,
                        max_segment_bytes=max_segment_bytes,
                    )
                except SegmentHeaderError:
                    return SlotInspection(
                        expected_slot, path, SlotState.DamagedTail,
                        header, 0, SEGMENT_HEADER_SIZE, None,
                    )
                return SlotInspection(
                    expected_slot, path, SlotState.Clean, header,
                    len(frames), SEGMENT_HEADER_SIZE + len(body), None,
                )

            count = 0
            good_end = SEGMENT_HEADER_SIZE
            for item in scan_stream(
                source, max_payload=max_payload, forensic=False,
                base_offset=SEGMENT_HEADER_SIZE,
            ):
                if isinstance(item, FrameIssue):
                    return SlotInspection(
                        expected_slot, path, SlotState.DamagedTail,
                        header, count, good_end, item.status
                    )
                count += 1
                good_end = item.offset + item.total_size

            return SlotInspection(
                expected_slot, path, SlotState.Clean, header, count, good_end, None
            )
    except FileNotFoundError:
        return SlotInspection(expected_slot, path, SlotState.Empty, None, 0, 0, None)
    except OSError:
        return SlotInspection(expected_slot, path, SlotState.ReadFailure, None, 0, 0, None)


def inspect_archive_slots(
    paths: Sequence[Path],
    *,
    max_slots: int = 64,
    max_payload: int = 65_536,
) -> ArchiveInspection:
    """Inspect a declared finite slot topology in physical index order.

    A corrupt/unreadable header could conceal a newer generation, so the
    largest observed generation is only a *candidate*, never silently
    promoted to a conclusive latest generation. Any duplicate generation or
    mixed device identity makes order ambiguous. A damaged tail does not
    invalidate the verified prefix but means that slot cannot be appended
    without first creating a new clean successor.
    """
    if type(max_slots) is not int or not 2 <= max_slots <= 65536:
        raise ValueError("invalid configured maximum archive slot count")
    if not 2 <= len(paths) <= max_slots:
        raise ValueError("archive needs at least two declared bounded physical slots")

    slots = tuple(
        inspect_slot(Path(path), i, max_payload=max_payload)
        for i, path in enumerate(paths)
    )
    reasons: list[str] = []
    if any(
        slot.state in (
            SlotState.InvalidHeader, SlotState.SlotMismatch, SlotState.ReadFailure
        )
        for slot in slots
    ):
        reasons.append("one or more slots cannot certify their generation")

    headers = [slot.header for slot in slots if slot.header is not None and slot.state in (
        SlotState.Clean, SlotState.DamagedTail
    )]
    if len({header.device_identifier for header in headers}) > 1:
        reasons.append("mixed DeviceIdentifier values in a single configured archive")
    generations = [header.generation for header in headers]
    if len(generations) != len(set(generations)):
        reasons.append("duplicate generation values make archive ordering ambiguous")

    chosen: int | None = None
    next_generation: int | None = None
    if generations:
        largest = max(generations)
        matching = [
            slot.slot for slot in slots
            if slot.header is not None and slot.state in (
                SlotState.Clean, SlotState.DamagedTail
            ) and slot.header.generation == largest
        ]
        if len(matching) == 1:
            chosen = matching[0]
        if largest == 0xFFFFFFFFFFFFFFFF:
            reasons.append("generation exhausted; no wrap or implicit reuse permitted")
        elif chosen is not None and not reasons:
            next_generation = largest + 1
    else:
        if not reasons:
            next_generation = 1

    return ArchiveInspection(
        slots=slots,
        newest_known_slot=chosen,
        order_unambiguous=not reasons,
        next_generation=next_generation,
        explanations=tuple(reasons),
    )
