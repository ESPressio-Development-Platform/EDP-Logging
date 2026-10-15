"""Pure-stdlib, bounded offline EDP diagnostic framing tools (partial implementation)."""

from .framing import (
    Frame, FrameIssue, FrameCodec, FrameStatus, crc32c, encode_frame, scan_stream,
)
from .archive import SegmentHeader, parse_segment_header, encode_segment_header
from .archive_recovery import (
    ArchiveInspection, SlotInspection, SlotState, inspect_slot, inspect_archive_slots,
)
from .record_structure import EnvelopeError, PreservedFloat, TypedEnvelope, inspect_typed_envelope
from .schema import (
    SchemaError, DiagnosticSchema, Message, Parameter,
    DecodedArguments, load_generated_schema, decode_declared_arguments,
)
from .record_facts import RecordFactError, RecordFacts, inspect_log_record_facts
from .analysis import (
    AttemptEvidence, EvidenceError, TemporalOrder,
    analyse_attempts, compare_record_time,
)

__all__ = [
    "Frame", "FrameIssue", "FrameCodec", "FrameStatus",
    "crc32c", "encode_frame", "scan_stream",
    "SegmentHeader", "parse_segment_header", "encode_segment_header",
    "ArchiveInspection", "SlotInspection", "SlotState", "inspect_slot", "inspect_archive_slots",
    "EnvelopeError", "PreservedFloat", "TypedEnvelope", "inspect_typed_envelope",
    "SchemaError", "DiagnosticSchema", "Message", "Parameter",
    "DecodedArguments", "load_generated_schema", "decode_declared_arguments",
    "RecordFactError", "RecordFacts", "inspect_log_record_facts",
    "AttemptEvidence", "EvidenceError", "TemporalOrder",
    "analyse_attempts", "compare_record_time",
]
