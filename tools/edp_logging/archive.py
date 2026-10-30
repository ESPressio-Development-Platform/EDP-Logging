"""Fixed 40-byte EDLS v1 segment header validation; no filesystem mutations."""

from __future__ import annotations

from dataclasses import dataclass
from io import BytesIO

from .framing import Frame, FrameIssue, crc32c, scan_stream

SEGMENT_HEADER_SIZE = 40
SEGMENT_MAGIC = b"EDLS"
SEGMENT_VERSION = 1


class SegmentHeaderError(ValueError):
    """Invalid or incompatible segment header; do not infer valid stored records."""


@dataclass(frozen=True)
class SegmentHeader:
    """Identifies a bounded archive slot and its nonzero unique generation."""

    slot: int
    generation: int
    device_identifier: bytes
    incarnation: int
    profile: int = 0


def parse_segment_header(data: bytes) -> SegmentHeader:
    """Validate the exact versioned header, including its independent checksum."""
    if len(data) != SEGMENT_HEADER_SIZE:
        raise SegmentHeaderError("EDLS header must be exactly 40 bytes")
    if data[:4] != SEGMENT_MAGIC or data[4] != SEGMENT_VERSION or data[5] not in (0, 1):
        raise SegmentHeaderError("invalid EDLS magic/version/reserved flags")
    if int.from_bytes(data[36:40], "little") != crc32c(data[:36]):
        raise SegmentHeaderError("EDLS header CRC32C mismatch")
    generation = int.from_bytes(data[8:16], "little")
    incarnation = int.from_bytes(data[32:36], "little")
    if generation == 0 or incarnation == 0:
        raise SegmentHeaderError("EDLS generation and incarnation must be nonzero")
    device = bytes(data[16:32])
    if len(device) != 16 or not any(device):
        raise SegmentHeaderError("invalid device identity width")
    return SegmentHeader(
        slot=int.from_bytes(data[6:8], "little"),
        generation=generation,
        device_identifier=device,
        incarnation=incarnation,
        profile=data[5],
    )


def encode_segment_header(header: SegmentHeader) -> bytes:
    """Reference fixture writer, exactly matching the device-side EDLS V1 header.

    Slot generation must already be chosen durably by the future archive owner;
    this utility does not allocate generations, touch storage or assert durability.
    """
    if (
        type(header.profile) is not int or header.profile not in (0, 1)
        or type(header.slot) is not int or not 0 <= header.slot <= 0xFFFF
        or type(header.generation) is not int or not 1 <= header.generation <= 0xFFFFFFFFFFFFFFFF
        or type(header.device_identifier) is not bytes or len(header.device_identifier) != 16
        or not any(header.device_identifier)
        or type(header.incarnation) is not int or not 1 <= header.incarnation <= 0xFFFFFFFF
    ):
        raise SegmentHeaderError("invalid EDLS header identity or generation")
    data = bytearray(SEGMENT_HEADER_SIZE)
    data[:4] = SEGMENT_MAGIC
    data[4] = SEGMENT_VERSION
    data[5] = header.profile
    data[6:8] = header.slot.to_bytes(2, "little")
    data[8:16] = header.generation.to_bytes(8, "little")
    data[16:32] = header.device_identifier
    data[32:36] = header.incarnation.to_bytes(4, "little")
    data[36:40] = crc32c(data[:36]).to_bytes(4, "little")
    return bytes(data)


def parse_sealed_segment(
    data: bytes, *, max_payload: int = 65_536,
    max_records: int = 64, max_segment_bytes: int = 1_048_576,
) -> tuple[SegmentHeader, tuple[Frame, ...]]:
    """Validate an entire bounded sealed batch, never accept a clean prefix.

    EDLC's count and framed-byte length must describe the exact complete body.
    Its CRC32C covers the EDLS header, every frame and the first 12 seal bytes.
    Framing/CRC are accidental-corruption evidence, not authentication or ACK.
    An independent durable NVS watermark is still required to establish ACK.
    """
    if type(max_records) is not int or not 1 <= max_records <= 64:
        raise SegmentHeaderError("invalid sealed record-count bound")
    if type(max_segment_bytes) is not int or max_segment_bytes < 76:
        raise SegmentHeaderError("invalid sealed segment-byte bound")
    if len(data) > max_segment_bytes or len(data) < 76:
        raise SegmentHeaderError("sealed segment size outside bound")
    header = parse_segment_header(data[:SEGMENT_HEADER_SIZE])
    if header.profile != 1:
        raise SegmentHeaderError("segment does not advertise the sealed profile")
    seal = data[-16:]
    count = int.from_bytes(seal[4:8], "little")
    length = int.from_bytes(seal[8:12], "little")
    if seal[:4] != b"EDLC" or not 1 <= count <= max_records:
        raise SegmentHeaderError("invalid sealed record count or completion")
    if length != len(data) - SEGMENT_HEADER_SIZE - 16:
        raise SegmentHeaderError("sealed body length mismatch")
    if int.from_bytes(seal[12:16], "little") != crc32c(data[:-4]):
        raise SegmentHeaderError("whole-segment CRC32C mismatch")
    frames = []
    for frame in scan_stream(
        BytesIO(data[SEGMENT_HEADER_SIZE:-16]), max_payload=max_payload,
        forensic=False, base_offset=SEGMENT_HEADER_SIZE,
    ):
        if isinstance(frame, FrameIssue):
            raise SegmentHeaderError("sealed batch contains an invalid EDLG frame")
        frames.append(frame)
        if len(frames) > max_records:
            raise SegmentHeaderError("sealed record-count bound exceeded")
    if len(frames) != count:
        raise SegmentHeaderError("sealed record count mismatch")
    return header, tuple(frames)
