# Configurable bounded EDLS record batching

Status: feature/logging-v1, version 0.1.0. Initial host-qualified implementation.
Native ESP32 performance, real relay-cut recovery and scheduled multi-producer
qualification are separate gates and are not claimed by host compilation.

## Ownership and finite capacity

BoundedEdlsBatch in logging/BoundedEdlsBatch.hpp accumulates complete CBOR EDLG
frames in the existing Bounded::Bytes container. Its stateless byte provider is
the existing EDP-Memory ByteOperations implementation selected by Bootstrap.
It borrows one PowerLossAcknowledgedEdls archive, the canonical MonotonicClock,
and separate caller-owned scratch. No timer, task, queue, allocator, filesystem,
native SDK dependency or independent Clock is created.

TMaximumRecords is a compile-time bound from 1 through 64. TMaximumBytes bounds
all framed bytes in one batch, excluding the EDLS header and completion seal.
The archive's MaximumSegmentBytes must fit that body plus 40 header bytes and
16 seal bytes when the maximum frame count exceeds one. Scratch must fit both
the whole segment and the existing per-frame inspection requirement.

The archive retains N latest acknowledged SEGMENTS plus one vacant successor
name. Each segment has one through TMaximumRecords records. Retained history is
finite: at most N*TMaximumRecords records and N*MaximumSegmentBytes file bytes,
plus the temporary successor and provider metadata/GC reserve. Partial segments
contain fewer records. This profile does not promise an exact N-record window.
Existing record TypeIdentifier, source incarnation, attempt ticket and fields
are unchanged; segment generation is never reinterpreted as a record counter.

## Configuration and ordinary worker use

Construction borrows the stable archive, Clock and non-aliased scratch; it does
not initialize storage or admit records. Bootstrap initializes the existing
archive explicitly, then calls ConfigureDwell(Duration). There is no implicit
runtime dwell default. Zero means immediate flush. Positive values must not
exceed the compile-time TMaximumDwellNanoseconds (100,000,000 ns by default;
applications may choose another bound explicitly).

Configuration returns Configured, InvalidDwell, PendingBatch, RecoveryRequired
or InsufficientScratch. A pending batch's dwell cannot be changed, so later
admission cannot extend the original first-record deadline. Invalid
reconfiguration preserves the previous valid setting.

Offer validates exactly one complete CBOR EDLG frame and copies it into owned
Bytes. Buffered means RAM admission only, with zero AcknowledgedRecords.
FlushRequired rejects this input without consuming it when the existing batch
has expired, is full by record count, or lacks byte capacity. The worker flushes
the prior batch, handles its completion, then may offer the unconsumed input
again. InvalidFrame and FrameTooLarge also preserve prior admitted bytes.

NextFlushDeadline returns an optional canonical timestamp. A full batch returns
its already-reached first-admission time. Otherwise the deadline is first
admission plus configured dwell. Service commits a full or due batch, returns
Waiting before its deadline, or Empty when no batch exists. Flush explicitly
drains a nonempty partial batch for shutdown or byte pressure. The existing
application EDP-Threading worker owns wake, WaitUntil and service scheduling;
this class does not create that worker. Dwell bounds intentional accumulation,
not scheduler lateness, flash operation time or overall acknowledgement latency.

Use one exclusive ordinary-context owner for configuration, admission, service
and archive mutation. Do not invoke these operations from an ISR, call them
concurrently or alias scratch with live frame bytes. Provider instances and
scratch outlive the noncopyable, nonmovable accumulator.

## Durability, uncertainty and source ownership

PowerLossAcknowledgedEdls::Publish remains exactly-one-frame. The explicit
PublishBatch<MaximumFrames> entry point uses the SAME publication state machine:
read/recover watermark, inspect protected history, prove physical vacancy,
write immutable successor, validate all frame CRCs/count/length/seal and exact
attempted frame bytes, verify protected predecessors, StoreValue and reread the
PowerLoss-atomic NVS watermark, then retire only the eligible oldest segment.

All frames in a segment share its ACK boundary. A successful Flush reports the
exact AcknowledgedRecords for that call and the nested archive observation.
DurablyAcknowledgedPendingRetirement also means NVS ACK is committed; delayed
physical reclamation is separate and the existing archive reconciles it before
a later publication. Task/queue/RAM acceptance is never durable completion.

Unknown/failed archive publication latches RecoveryRequired, retains pending
bytes and reports zero acknowledged records. No repeated Flush or Offer retries
the transaction. Bootstrap must reconcile the existing archive/watermark before
replacing this accumulator and deciding which source operations to retry.
An atomic NVS write can commit even when its response is lost. Blind retry
could duplicate an already committed batch; tests explicitly cover that case.
Destruction does not flush, ACK or retry pending RAM.

Clock regression while a batch is pending also fails closed. Admission near
the canonical coordinate limit refuses an unrepresentable deadline rather
than wrapping or silently extending dwell.

## Sealed EDLS v1 content profile

The existing 40-byte header layout and its CRC are unchanged. Header byte 5,
formerly required zero, now explicitly names EdlsContentProfile:

| Value | Meaning |
|---|---|
| 0 / Unsealed | Existing unsealed EDLS frame stream |
| 1 / SealedFrames | Immutable frame sequence with required final completion seal |

ReadEdlsHeader rejects unknown profiles. Ordinary single-frame callers still
write profile 0. PublishBatch with MaximumFrames greater than one writes profile
1 even when a partial batch contains only one record. No automatic migration or
retroactive ACK provenance is introduced. Old decoders reject the new profile.

The last 16 bytes of a sealed segment are:

| Offset in seal | Width | Meaning |
|---|---:|---|
| 0 | 4 | ASCII EDLC |
| 4 | 4 | Exact positive record count, little-endian |
| 8 | 4 | Exact total EDLG frame bytes, little-endian |
| 12 | 4 | CRC32C of the complete EDLS header, frame body and first 12 seal bytes |

The existing EDLS inspection owner validates every complete EDLG frame and
then validates the seal's count, body length and whole-segment checksum.
This detects loss of a suffix ending exactly between complete EDLG records:
a checksum-valid prefix cannot masquerade as the complete acknowledged batch.
Strict protected plans additionally reject header-only segments with no frames.

CRC is accidental-corruption detection, not authentication. This protocol does
not upgrade generic VFS FileStorage retention/interruption properties or prove
arbitrary internal flash-phase durability. The independent NVS ACK and actual
provider/physical qualification remain necessary.

## Source and host tools

The shared successor template adds a trailing MaximumFrames parameter defaulted
to one; no parallel file writer is created. The strict archive adds PublishBatch,
MaximumFramePayloadBytes and MaximumSegmentBytes. Frame inspection and
predecessor comparison understand the explicit sealed profile.

Python archive.py extends SegmentHeader with profile=0, preserves existing
golden bytes, and exposes parse_sealed_segment with explicit payload, record and
total-byte bounds. archive_recovery.inspect_slot validates a sealed batch
completely before reporting any verified records. The CLI --archive validates
the entire seal before emitting record events, including in forensic mode;
its sealed whole-segment buffering is capped at 1 MiB. A valid seal alone is not
proof that NVS acknowledged the segment.

## Validation and remaining gates

tests/run-bounded-edls-batch-tests.sh exercises the actual POSIX VfsFileStorage
and an explicit fake of the independently atomic NVS contract. It verifies
zero/configured dwell, first-record anchoring, capacity refusal, finite rolls,
single-frame API rejection of multi-frame input, CRC-clean suffix truncation,
wrong count with recomputed CRC, rejected watermark, commit-then-error
uncertainty, byte pressure, scratch bounds and Clock overflow/regression.

Initial RPI400 GCC UBSan run 20261030T130515-b3a7d5a0 PASS.
Clang UBSan plus existing GCC/Clang foundations, actual VFS rolling,
strict ACK and generated typed Task regressions: 20261030T130658-6c103347 PASS.
Python sealed-owner/tooling and 64 existing HIL controller negatives:
20261030T130912-034c600a PASS. The subsequent CLI-specific run is recorded in
the living handoff after completion. No hosted Actions execution is relied on.

Next gates are native compile/runtime with real NVS/LittleFS, independent
typed CBOR and raw media seal/NVS audit, real VDD cuts at unACKed and retirement
boundaries, finite scheduled multi-producer worker/backpressure, physical
capacity/GC and flash endurance. No throughput or complete production claim
follows from this initial implementation.
