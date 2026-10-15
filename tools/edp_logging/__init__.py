"""Pure-stdlib, bounded offline EDP diagnostic framing tools (partial implementation)."""

from .framing import (
    Frame, FrameIssue, FrameCodec, FrameStatus, crc32c, encode_frame, scan_stream,
)
from .archive import SegmentHeader, parse_segment_header
from .record_structure import EnvelopeError, PreservedFloat, TypedEnvelope, inspect_typed_envelope
from .schema import (
    SchemaError, DiagnosticSchema, Message, Parameter,
    DecodedArguments, load_generated_schema, decode_declared_arguments,
)
from .record_facts import RecordFactError, RecordFacts, inspect_log_record_facts

__all__ = [
    "Frame", "FrameIssue", "FrameCodec", "FrameStatus",
    "crc32c", "encode_frame", "scan_stream",
    "SegmentHeader", "parse_segment_header",
    "EnvelopeError", "PreservedFloat", "TypedEnvelope", "inspect_typed_envelope",
    "SchemaError", "DiagnosticSchema", "Message", "Parameter",
    "DecodedArguments", "load_generated_schema", "decode_declared_arguments",
    "RecordFactError", "RecordFacts", "inspect_log_record_facts",
]
