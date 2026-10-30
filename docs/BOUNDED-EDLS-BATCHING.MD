# Configurable bounded EDLS record batching

Status: feature/logging-v1, version 0.1.0. Host-qualified implementation plus
a native synchronous ESP32 benchmark and independent raw-media audit. Real
relay-cut recovery and scheduled multi-producer qualification remain separate gates.

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
The independent 40-byte NVS watermark always keeps profile 0; a sealed-file
profile in the ledger is InvalidWatermark even with a valid header CRC.

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

Remaining gates include real VDD cuts at unACKed and retirement boundaries,
finite scheduled multi-producer worker/backpressure, physical capacity/GC and
flash endurance. The following native result is a bounded measurement, not
complete production qualification.

## Initial native 16-record measurement, 30 October 2026

Native ESP32-WROOM ESP-IDF 5.5.5 build 20261030T132959-fb355990 passed after the
explicit NVS watermark profile guard and complete original-record comparison.
Factory SHA256: 04543343d5e30f3fa70b994f7eedbb4c0e160a435c4e701f739aeebd6e639909.
The actual main-task stack configuration was 8192 B.

Physical run 20261030T133316-98a7a8dc passed with verified firmware and LittleFS
seed after a fresh 2 MiB backup. One synchronous task generated 256 individual
typed CBOR records, admitted 16 per batch and completed 16 strict NVS segment
commits.

| Measurement | Initial sealed-batch result |
|---|---:|
| Durable record completions | 256 |
| Strict NVS-ACKed segments | 16 |
| Total workload time | 49.339880 s |
| Durable record throughput | 5.189 records/s |
| Admission-to-ACK p50 | 3383535 us |
| Admission-to-ACK p99 | 4581894 us |
| Sampled minimum free heap | 278896 B |
| Sampled minimum main-task stack headroom | 3040 B |
| Explicit accumulation dwell | 100000000 ns |
| Retained segment window | 15,16 |
| Retained original record tickets | 225..256 |

RAM admission did not ACK. Every final retained physical frame was compared
against its original generated record encoding. The existing per-record
instrumented benchmark measured 1.970 records/s; these different workload and
instrumentation profiles are not an isolated measurement of one algorithmic
change. The initial batch profile still makes per-frame filesystem reads during
each mandatory inspection and does not meet a 100 records/s aspiration.
The reported p50 uses the upper median; p99 uses sorted sample index 253.

First physical attempt 20261030T133157-043959a1 failed during read-only full-chip
backup: the UART data stream stopped at approximately 220 KiB. It never flashed.
The exit trap powered OFF. The fresh-directory retry passed without changing
baud or weakening backup/image verification. The root cause of that transient
UART failure is not established; failed evidence remains private and intact.

Host requalification after the NVS guard: GCC/Clang UBSan batching, existing
Memory/Threading strict typed Task integration and 70 HIL/audit tests,
20261030T132958-b210831f PASS. Existing strict NVS/EDLS GCC/Clang UBSan regression,
20261030T133047-b20f9efd PASS. Native cut-fixture build 20261030T133018-f49c5dd7
PASS; the physical cut and independent media outcomes are recorded separately
after they complete.

Independent actual-media audit 20261030T133832-06d04ea6 PASS: whole sealed
segments 15/16, every retained original record 225..256 with all permanent typed
fields, and official Espressif NVS page/entry/blob CRC ACK16 in namespace
batchack, incarnation 7. Physical LittleFS SHA256:
eca628a62c54d3dc8b71c60caeb4fb60f29f24182b2b3975fe8808597a4210ac.
Physical NVS SHA256:
d252f368493957bb31bf938f67865a53f6cf92eff20204c6d265cec518873297.
Private evidence is retained on RPI400; no raw media or UART is published.

### First cut attempt and corrected terminal protocol

Physical run 20261030T134033-1f8ae8cf is **FAIL**, not a qualified electrical
acceptance result. GPIO17 OFF/ON was recorded for 8.263 s; firmware booted with
native reset reason 7, recovered NVS ACK2 and protected segments 1/2, removed only
the complete unACKed candidate, then verified new segments 3/4. However, this
fixture emitted a new terminal label SEALED_TYPED_BATCH_NVS_ACK_AFTER_CUT.
The unchanged shared PostCutEvidence owner recognises the established label
STRICT_TYPED15_NVS_ACK_AFTER_CUT, so the controller correctly timed out with
exit 6 and the runner failed. Its acceptance rules were not relaxed.

Independent actual raw-media audit 20261030T134734-1adc2de5 PASS nevertheless
confirmed every retained record 33..64, complete sealed segments 3/4 and official
NVS page/entry/blob CRC ACK4. This proves the resulting media, not a retroactive
PASS for the failed controller run. LittleFS SHA256:
9f27a0628fd6163db51ffb87d65f1af76a6c1747a233912505a53e736526501e.
NVS SHA256:
733eb3e7f94cbbd3f9673c19d8b3d2bce6403b21a3a91112c2d8307a383b98ac.

The test-only firmware now reuses the established strict typed completion
label; the separate EDP_BATCH replay receipt still identifies 16 records per
segment. No production path or host qualification predicate changed.
Corrected native build 20261030T134734-937d4516 PASS, factory SHA256:
61c26e23821b28b0de902634649eb7392544b3c3e077dfb65fc52c52a7d6a0cf.
A fresh physical repeat and independent audit are required before that gate
can pass.
