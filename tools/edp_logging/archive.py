"""Fixed 40-byte EDLS v1 segment header validation; no filesystem mutations."""

from __future__ import annotations

from dataclasses import dataclass

from .framing import crc32c

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


def parse_segment_header(data: bytes) -> SegmentHeader:
    """Validate the exact versioned header, including its independent checksum."""
    if len(data) != SEGMENT_HEADER_SIZE:
        raise SegmentHeaderError("EDLS header must be exactly 40 bytes")
    if data[:4] != SEGMENT_MAGIC or data[4] != SEGMENT_VERSION or data[5] != 0:
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
    )


def encode_segment_header(header: SegmentHeader) -> bytes:
    """Reference fixture writer, exactly matching the device-side EDLS V1 header.

    Slot generation must already be chosen durably by the future archive owner;
    this utility does not allocate generations, touch storage or assert durability.
    """
    if (
        type(header.slot) is not int or not 0 <= header.slot <= 0xFFFF
        or type(header.generation) is not int or not 1 <= header.generation <= 0xFFFFFFFFFFFFFFFF
        or type(header.device_identifier) is not bytes or len(header.device_identifier) != 16
        or not any(header.device_identifier)
        or type(header.incarnation) is not int or not 1 <= header.incarnation <= 0xFFFFFFFF
    ):
        raise SegmentHeaderError("invalid EDLS header identity or generation")
    data = bytearray(SEGMENT_HEADER_SIZE)
    data[:4] = SEGMENT_MAGIC
    data[4] = SEGMENT_VERSION
    data[6:8] = header.slot.to_bytes(2, "little")
    data[8:16] = header.generation.to_bytes(8, "little")
    data[16:32] = header.device_identifier
    data[32:36] = header.incarnation.to_bytes(4, "little")
    data[36:40] = crc32c(data[:36]).to_bytes(4, "little")
    return bytes(data)
