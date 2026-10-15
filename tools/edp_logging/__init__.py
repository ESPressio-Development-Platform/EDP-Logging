"""Pure-stdlib, bounded offline EDP diagnostic framing tools (partial implementation)."""

from .framing import (
    Frame, FrameIssue, FrameCodec, FrameStatus, crc32c, encode_frame, scan_stream,
)
from .archive import SegmentHeader, parse_segment_header
from .record_structure import EnvelopeError, PreservedFloat, TypedEnvelope, inspect_typed_envelope

__all__ = [
    "Frame", "FrameIssue", "FrameCodec", "FrameStatus",
    "crc32c", "encode_frame", "scan_stream",
    "SegmentHeader", "parse_segment_header",
    "EnvelopeError", "PreservedFloat", "TypedEnvelope", "inspect_typed_envelope",
]
