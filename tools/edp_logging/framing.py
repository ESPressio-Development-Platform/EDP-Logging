"""Canonical EDLG v1 framing, strictly bounded even for damaged streams.

This module validates transport framing, NOT the semantic CBOR/JSON LogRecord.
A valid CRC32C is accidental-corruption evidence, never authentication or durability.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum, Enum
from typing import BinaryIO, Iterator

MAGIC = b"EDLG"
COMPLETION = b"GLDE"
FRAME_VERSION = 1
HEADER_SIZE = 16
OVERHEAD = 20
DEFAULT_MAX_PAYLOAD = 65_536
DEFAULT_RESYNC_LIMIT = 1_048_576


class FrameCodec(IntEnum):
    """Defined portable EDLG payload codecs."""

    Cbor = 1
    Json = 2


class FrameStatus(str, Enum):
    """Distinct outcomes for an unavailable or corrupt frame candidate."""

    InvalidMagic = "InvalidMagic"
    TruncatedHeader = "TruncatedHeader"
    UnsupportedVersion = "UnsupportedVersion"
    UnsupportedCodec = "UnsupportedCodec"
    UnsupportedFlags = "UnsupportedFlags"
    PayloadTooLarge = "PayloadTooLarge"
    TruncatedFrame = "TruncatedFrame"
    InvalidCompletion = "InvalidCompletion"
    CrcMismatch = "CrcMismatch"
    ResynchronizationLimit = "ResynchronizationLimit"


@dataclass(frozen=True)
class Frame:
    """Fully validated outer frame; the payload remains semantically uninterpreted."""

    offset: int
    codec: FrameCodec
    payload: bytes
    crc32c_value: int
    recovered_candidate: bool

    @property
    def total_size(self) -> int:
        return OVERHEAD + len(self.payload)


@dataclass(frozen=True)
class FrameIssue:
    """One integrity/format failure, preserving byte location and explicit status."""

    offset: int
    status: FrameStatus


def crc32c(data: bytes | bytearray | memoryview, initial: int = 0) -> int:
    """Compute CRC32C (reflected Castagnoli; standard 123456789 -> E3069283)."""
    crc = (initial ^ 0xFFFFFFFF) & 0xFFFFFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc >> 1) ^ 0x82F63B78) & 0xFFFFFFFF if crc & 1 else crc >> 1
    return crc ^ 0xFFFFFFFF


def encode_frame(codec: FrameCodec, payload: bytes, *, max_payload: int = DEFAULT_MAX_PAYLOAD) -> bytes:
    """Construct deterministic fixture/reference bytes; target encoders own their buffers."""
    if codec not in (FrameCodec.Cbor, FrameCodec.Json):
        raise ValueError("unknown EDLG codec")
    if max_payload < 0 or len(payload) > max_payload or len(payload) > 0xFFFFFFFF:
        raise ValueError("payload exceeds bounded EDLG profile")
    head = bytearray(HEADER_SIZE)
    head[:4] = MAGIC
    head[4] = FRAME_VERSION
    head[5] = int(codec)
    head[8:12] = len(payload).to_bytes(4, "little")
    checksum = crc32c(head[:12])
    checksum = crc32c(payload, checksum)
    checksum = crc32c(COMPLETION, checksum)
    head[12:16] = checksum.to_bytes(4, "little")
    return bytes(head) + payload + COMPLETION


def _candidate_problem(header: bytearray, maximum: int) -> FrameStatus | None:
    if header[:4] != MAGIC:
        return FrameStatus.InvalidMagic
    if header[4] != FRAME_VERSION:
        return FrameStatus.UnsupportedVersion
    if header[5] not in (FrameCodec.Cbor, FrameCodec.Json):
        return FrameStatus.UnsupportedCodec
    if header[6] != 0 or header[7] != 0:
        return FrameStatus.UnsupportedFlags
    if int.from_bytes(header[8:12], "little") > maximum:
        return FrameStatus.PayloadTooLarge
    return None


def scan_stream(
    source: BinaryIO,
    *,
    max_payload: int = DEFAULT_MAX_PAYLOAD,
    forensic: bool = False,
    resync_limit: int = DEFAULT_RESYNC_LIMIT,
    base_offset: int = 0,
) -> Iterator[Frame | FrameIssue]:
    """Incrementally validate frames without seeking or unbounded input buffering.

    Conservative mode stops after the first invalid candidate. Forensic mode scans
    at most resync_limit discarded bytes between intact frames; every recovered
    frame is tagged as a candidate, not evidence of intact archive continuity.
    This scan does not establish persistence or source authenticity.
    """
    if not (0 <= max_payload <= 0xFFFFFFFF) or resync_limit < 0 or base_offset < 0:
        raise ValueError("invalid frame scanner bounds")

    buffer = bytearray()
    offset = base_offset
    eof = False
    # Remains sticky after the first damaged region: even a sequence of later
    # individually valid frames is forensically recovered, not continuous history.
    invalid_preceded = False
    discarded = 0

    def fill(minimum: int) -> None:
        nonlocal eof
        while not eof and len(buffer) < minimum:
            fragment = source.read(min(4096, minimum - len(buffer)))
            if not fragment:
                eof = True
            else:
                buffer.extend(fragment)

    def reject(status: FrameStatus) -> FrameIssue:
        return FrameIssue(offset, status)

    while True:
        fill(HEADER_SIZE)
        if not buffer:
            return
        if len(buffer) < HEADER_SIZE:
            yield reject(FrameStatus.TruncatedHeader)
            return

        problem = _candidate_problem(buffer, max_payload)
        if problem is None:
            length = int.from_bytes(buffer[8:12], "little")
            needed = OVERHEAD + length
            fill(needed)
            if len(buffer) < needed:
                problem = FrameStatus.TruncatedFrame
            elif bytes(buffer[HEADER_SIZE + length:needed]) != COMPLETION:
                problem = FrameStatus.InvalidCompletion
            else:
                checksum = crc32c(memoryview(buffer)[:12])
                checksum = crc32c(memoryview(buffer)[HEADER_SIZE:HEADER_SIZE + length], checksum)
                checksum = crc32c(COMPLETION, checksum)
                if checksum != int.from_bytes(buffer[12:16], "little"):
                    problem = FrameStatus.CrcMismatch
            if problem is None:
                yield Frame(
                    offset=offset,
                    codec=FrameCodec(buffer[5]),
                    payload=bytes(buffer[HEADER_SIZE:HEADER_SIZE + length]),
                    crc32c_value=int.from_bytes(buffer[12:16], "little"),
                    recovered_candidate=invalid_preceded,
                )
                del buffer[:needed]
                offset += needed
                # Reset only the inter-frame search budget. Once any damage has
                # occurred in this source, no later candidate regains verified
                # pre-damage continuity from CRC alone.
                discarded = 0
                continue

        yield reject(problem)
        if not forensic:
            return
        invalid_preceded = True
        del buffer[0]
        offset += 1
        discarded += 1
        if discarded > resync_limit:
            yield reject(FrameStatus.ResynchronizationLimit)
            return
