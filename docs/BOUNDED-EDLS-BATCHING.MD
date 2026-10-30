# Configurable bounded EDLS record batching

Status: feature/logging-v1, version 0.1.0. Host-qualified implementation plus
a native synchronous ESP32 benchmark, complete-unACKed-segment relay cut and
independent raw-media audits. Scheduled multi-producer qualification remains open.

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

Remaining gates include VDD cuts during retirement and physical write/GC activity,
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

### Corrected electrical cut and independent media qualification

Corrected physical run **20261030T135327-96bdbd46 PASS** used the unchanged
shared controller and acceptance rules, a fresh 2 MiB backup, verified firmware
61c26e23821b28b0de902634649eb7392544b3c3e077dfb65fc52c52a7d6a0cf and verified
LittleFS seed. The controller confirmed GPIO17 OFF for **8.221 s**, ON and a new
boot epoch. Native reset was 10, ROM reset 6 SDIO_RESET; the honest classification
is **EXTERNALLY_VERIFIED_POWER_CUT**, not POR_CONFIRMED.

Before cut, segments 1/2 each contained 16 original records with NVS ACK2; the
complete sealed candidate 3 was still unACKed. Recovery verified the protected
batches, removed only that unACKed candidate, then strictly committed segments 3/4.
Final retained source tickets are 33..64, with all 16 original frames in each
segment. Only the test fixture completion string changed after the first run.

Independent actual raw-media run **20261030T135804-8e1a4cfe PASS** revalidated
whole seals, every EDLG CRC and all fifteen typed-field bindings for records 33..64,
plus official NVS page/entry/blob CRC ACK4, namespace batchack, incarnation 7.
LittleFS SHA256: 9f27a0628fd6163db51ffb87d65f1af76a6c1747a233912505a53e736526501e.
NVS SHA256: 733eb3e7f94cbbd3f9673c19d8b3d2bce6403b21a3a91112c2d8307a383b98ac.
These match the first attempt's independently audited final media; its failed
controller verdict remains unchanged. ESP32 exited powered OFF.

This qualifies the complete-unACKed-segment gap and subsequent recovery/rolling
for this native profile. It does not establish arbitrary in-write or GC-phase
durability, scheduled multi-producer performance, flash endurance, or full
production readiness.

## Whole-segment inspection using existing caller scratch

The same EdlsArchiveInspection owner now uses one complete physical ReadFileAt
when the file fits its existing caller-owned scratch. Header/frame/seal parsing
uses readonly spans into that buffer. Every CRC, count, length and protected
history check remains; the existing streaming path handles larger files.
No allocation, provider, persistent cache, format or transaction-order change
is introduced. No file bytes or integrity evidence are reused across calls.

A failed or short whole-file read cannot be promoted into verified frames.
The inspector independently reads only the 40-byte header to retain available
valid identity/generation/slot provenance before returning failure. In
particular, strict Recover must continue refusing a foreign segment even when
its larger read fails. Header errors keep their prior classifications.

GCC/Clang compare whole-read and streaming outcomes for every single-bit
corruption of a complete sealed fixture. Negative tests cover failed/short
bulk reads and an actual strict recovery attempt that must preserve a foreign
candidate and both acknowledged predecessors without changing NVS. Existing
GCC/Clang batching, strict ACK, actual VFS successor/rolling and typed Task
regressions also pass. Native performance and physical outcomes for this
inspection change are recorded below after measurement; the initial 5.189
records/s and earlier cut receipts above remain the unchanged baseline.

### Same native workload after complete segment reads

Both native environments rebuilt in 20261030T140831-ce3715f5 PASS. Profile
factory SHA256: be8609f7217c6688013079627731a334e74b73bd9fa2b74f12227314c540e3b2.
Cut factory SHA256: 26d2ca2e256d979a222c7fc30d5243495d95566701bd3cfa3177f7967ccebea5.

Actual ESP32 run **20261030T141118-9faa651c PASS** used the same generated
records, 16-record batches, explicit 100 ms dwell, N=2 retained segments, 8192 B
stack, SDK/provider bindings and physical provisioning procedure.

| Measurement | Per-frame inspection baseline | Complete segment reads |
|---|---:|---:|
| Durable original records | 256 | 256 |
| NVS-ACKed segments | 16 | 16 |
| Total workload time | 49.339880 s | 4.953522 s |
| Durable record throughput | 5.189 records/s | 51.680 records/s |
| Admission-to-ACK p50 | 3383535 us | 300615 us |
| Admission-to-ACK p99 | 4581894 us | 479627 us |
| Sampled minimum free heap | 278896 B | 278896 B |
| Sampled minimum main stack headroom | 3040 B | 3056 B |
| Retained original tickets | 225..256 | 225..256 |

The measured workload is approximately 9.96 times faster. All records still
share their segment's strict file-verification/NVS acknowledgment boundary.
This is a synchronous full-batch sample, not sustained scheduled ingestion or
a 100 records/s qualification.

Host initial owner regressions 20261030T140341-47624421 PASS; new whole-read and
streaming corruption/foreign-header GCC/Clang negatives 20261030T140713-e753bfdc
PASS; GCC/Clang UBSan inspection plus actual VFS successor/rolling and genuine
Memory/Threading typed strict Task regressions 20261030T140934-9e311a15 PASS.
The existing host workflow now enables the same inspection UBSan option; no
hosted execution is relied upon.

Independent physical profile audit in detached sequence
20261030T141936-ad0f5730 passed: every retained original record 225..256,
whole seals/frame CRCs and official NVS CRC ACK16. LittleFS raw SHA256:
7935bbfe6d50c2ae972105ec55cca9f212f8d559c93a605510a0a6256c8bdf0b.
NVS raw SHA256:
d252f368493957bb31bf938f67865a53f6cf92eff20204c6d265cec518873297.
The two extracted EDLS files are byte-for-byte equal to the baseline files:
slot0 SHA256 ffcb495b7cd6b9919765965a32d11e455b986f16dae30d02d837a47e9483c7fe;
slot2 SHA256 c76ea4ff8511d1487835aa2a9203f0ed6ffda8e4bba08a8e3eed5302b1df6adf.
Whole partition hashes need not match logical file hashes.

The same detached sequence proceeds through a fresh GPIO17 cut and separate
post-cut physical audit. Their exact outcome is recorded after completion;
the earlier cut qualifies the preceding source, not this inspection change.

The complete requalification sequence **20261030T141936-ad0f5730 PASS** also
performed the fresh GPIO17 cut and separate post-cut raw audit. OFF duration was
**8.222 s**, with confirmed OFF/ON and a new app boot epoch. Native reset 7 and
ROM EXT_SYS_RESET/RTCWDT_RTC_RESET were observed; classification remains
EXTERNALLY_VERIFIED_POWER_CUT, not POR_CONFIRMED. The unchanged controller
accepted the protected-batch recovery and strict next commits.

Independent post-cut physical parsing verified sealed segments 3/4, every
original record 33..64 and official NVS CRC ACK4/incarnation 7. LittleFS SHA256:
002042d78c081e1a2d06c224c041c68764ebbec7a5e67e6e9e3e8d2e8ff6cd4c.
NVS SHA256:
733eb3e7f94cbbd3f9673c19d8b3d2bce6403b21a3a91112c2d8307a383b98ac.
Both physical evidence roots and the prior baseline remain private and intact.
The device exited powered OFF. These are synchronous fixture results; scheduled
worker/backpressure, retirement/write/GC cuts, endurance and full product gates
remain separate.

## Explicit 16-record and 64-record native configurations

The production batching API already supports a maximum count from 1 through
64. The native fixture now selects two explicit configurations without changing
production code. Its default remains 16 records. Both select the same explicit
100 ms dwell and retain two acknowledged SEGMENTS plus one physical spare.

| Native configuration | Body byte cap | Complete segment/scratch cap | Maximum retained records |
|---|---:|---:|---:|
| 16 records | 4096 B | 4152 B | 32 |
| 64 records | 16384 B | 16440 B | 128 |

These are fixture budgets for the fixed generated schema. A different source
schema may hit the byte cap before the count cap. The application still chooses
both production bounds independently, and partial deadline flushes may retain
fewer records. Native scratch/readback/batch buffers remain static outside the
8192 B main task stack; actual heap/stack figures require measurement.

Build the larger fixture with EDP_LFS_BUILD_ENV=esp32dev-sealed-batch-64 using
the existing typed build helper. Its cut variant is
esp32dev-sealed-batch-64-cut. The same physical profile runner selects
EDP_PROFILE_BUILD_ENV=esp32dev-sealed-batch-64. The same relay runner uses
typed_ack_gap with the explicit 64-cut environment; electrical acceptance rules
are unchanged.

The 256-record larger workload has four strict segment ACKs, retaining
segments 3/4 with original tickets 129..256. The cut first protects segments 1/2
(tickets 1..128), leaves complete candidate 3 (129..192) unACKed, then recovers
and strictly commits 3/4 (129..256). Segment generation is never a record counter.
Independent raw audit selects EDP_BATCH_AUDIT_KIND=profile64 or power-cut64 and
requires exactly 64 complete CRC-valid original typed records per retained
segment. The default 16-record audit remains separately selectable.

Host large-bound coverage additionally commits full 64-record segments through
actual POSIX VFS and the explicit atomic-NVS fake, rejects the next unconsumed
input on fullness, and verifies a partial two-record flush at a newly configured
50 ms first-admission deadline. Independent Python negatives check every record
in a 64-record segment and reject count/ticket reinterpretation. Actual native
build/runtime/resource/cut results follow after execution.

Large-configuration host validation 20261030T144419-2b28a733 PASS: GCC/Clang
UBSan full 64 and partial-deadline real-VFS cases plus 74 controller/audit tests.
Native build 20261030T143752-db3e7c35 PASS compiled all four environments:
default 16 benchmark/cut and explicit 64 benchmark/cut. PlatformIO rebuilt shared
SDK dependencies; this was build time, not a physical runtime result. Detached
sequence 20261030T145734-fce800f4 waits for that PASS before provisioning, then
runs the 64-record benchmark, independent raw profile audit, fresh electrical
cut and independent post-cut raw audit. Physical outcomes remain pending at
this source checkpoint; no 64-record throughput/resource result is inferred.

### 64-record actual native measurement and electrical qualification

Detached sequence **20261030T145734-fce800f4 PASS** waited for all four native
builds, then completed verified provisioning, the 256-record profile, independent
profile media audit, a fresh electrical cut and independent post-cut media audit.

| Measurement | 16-record bound | 64-record bound |
|---|---:|---:|
| Durable original records | 256 | 256 |
| Strict NVS segment ACKs | 16 | 4 |
| Total workload time | 4.953522 s | 1.927310 s |
| Durable record throughput | 51.680 records/s | 132.828 records/s |
| Admission-to-ACK p50 | 300615 us | 512326 us |
| Admission-to-ACK p99 | 479627 us | 590018 us |
| Sampled minimum free heap | 278896 B | 241848 B |
| Sampled main-task stack headroom | 3056 B | 3076 B |
| Explicit accumulation dwell | 100 ms | 100 ms |
| Retained original tickets | 225..256 | 129..256 |

The 64-record bound uses 37048 B less sampled free heap and retains up to 128
records instead of 32, while increasing full-batch acknowledgment latency.
The measured burst workload exceeds 100 durable records/s; it is not scheduled
sustained ingestion, a full product memory budget or an endurance result.

64-profile factory SHA256:
7dff9366b547176b0e6d529ae954f0bad476921b61027d515a5811b100422763.
Independent actual profile LittleFS SHA256:
41b8560c7510e580355806258b9919dba604fe9721f3af5d9703c83e2d858a38.
Actual profile NVS SHA256:
51757d6b012b1ae4687ae43556abfc15ea58e1747684ca7869ea9bea738f723f.
Both retained segments 3/4 contain 64 complete original typed frames each,
tickets 129..256, with whole-seal/EDLG CRC and official NVS CRC ACK4.

64-cut factory SHA256:
1cc760f62bc5ccbc906ba2f44d1ba757b89450a275c29ecbaa7e61b5a4a50652.
The fresh controller-confirmed OFF interval was **8.220 s**. Native reset 1
and ROM POWERON_RESET 1 agree, so this run is honestly **POR_CONFIRMED**.
NVS ACK2 and both complete protected 64-record segments survived the complete
unACKed third candidate; recovery then strictly committed 3/4.
Separate post-cut actual LittleFS SHA256:
6a36d579f661e86eb7161dca5b59a975897e809fc42aecbd27571d574b5c3141.
Actual NVS SHA256:
733eb3e7f94cbbd3f9673c19d8b3d2bce6403b21a3a91112c2d8307a383b98ac.
Every original retained record 129..256/all 15 fields, full seals/frame CRCs
and official NVS page/entry/blob CRC ACK4 passed independent parsing.

Raw media, original failed attempts and earlier configurations remain private
and intact. ESP32 exited powered OFF. There is no automatic in-place history
migration: Bootstrap must establish compatible archive provenance and bounds
when choosing a configuration. Remaining scheduled-worker, physical write/GC,
capacity/endurance and three-target demo gates stay open.

## Existing dedicated worker integration (2026-10-30)

[Coordinator sink service](COORDINATOR-SINK-SERVICE.MD) describes the
host-qualified optional service path in the existing DedicatedCoordinator
and typed delivery. Idle partial batches expose canonical WaitUntil
deadlines even without new input. Native worker, sustained producer
and backpressure qualification remains pending.

## Managed worker capacity boundary (30 October)

The separately selected native worker500ms profile uses the existing
compile-time dwell ceiling/runtime configuration and independent existing
source/representation Memory topologies. It passes finite queue pressure
and idle partial flushing, but the target100records/s scheduled workload
reaches queue-capacity admission deadlines before Memory acquisition.
The100ms profile also remains unqualified for that sustained target.
These are retained failed workloads, not throughput claims. Native fixture
README and the living handoff contain exact runs and forthcoming per-stage
profiling. Generic typed validation, ACK order and all versions0.1.0 remain.

## Measured native worker cost

The failed500ms/160MHz worker sample now has same-owner wall profiling:
encoder~778.5us/call, typed delivery without publication~1760us/call and
flush-bearing delivery~641128us/call. It retains all typed validation.
The actual application compile command uses -Og; an explicit -O2 build
with assertions retained is the next controlled configuration experiment.
See native fixture README for full totals, quiescence and limits.

## Explicit optimised native configuration

The native fixture's separately selected O2/500ms/160MHz environment passed
a finite2000-record scheduled workload, idle partial flush, queue pressure,
finite teardown and independent actual-media/NVS audit in RPI400 detached
20261030T180038-622f5b3b. All2129 accepted phase records were durably completed.
The final two retained segments contain92 independently verified original
15-field records; ACK35. Two producers offered100records/s; complete drain
took21.002193s, so full-run average including drain is95.23records/s.
ACK latency p50809.995ms/p991215.160ms; configured accumulation dwell500ms
is not a bound on storage/service/scheduler time.

The build independently verifies actual -O2, enabled level2 assertions,
unchanged160MHz CPU and exact500ms selection. Standard100ms and earlier
Og profiles remain selectable; their prior failures are preserved.
No production default, typed validation or durability condition changed.
See the native README for exact hashes, stage/resource observations and
remaining power/GC/endurance/demo gates. This finite result is not general
production readiness or default100ms scheduling qualification.

## Buildable three-target container demonstration

[demos/ConfigurableBoundedEdlsBatch](../demos/ConfigurableBoundedEdlsBatch/README.MD)
shares one application header across Arduino IDE, PlatformIO Arduino and
PlatformIO ESP-IDF. It demonstrates count-triggered full batching, explicitly
configured idle partial dwell, RAM-versus-durable ACK, finite two-segment
retention and durable incarnation advancement across normal boots.
The small CBOR values demonstrate this container; the full generated15-field
managed path remains the separate native fixture.

All three builds183532-8bfb4a6b and actual two-boot physical executions
184352-e1b78bb7 PASS on RPI400;95 controller/media tests182456-7134fda1 PASS.
The demo README preserves earlier build and image-verification failures,
their measured cause, exact image hashes and corrected verify-before-boot
ordering. No hosted Actions coverage is relied upon. Overall production,
extended scheduled load and remaining write/GC/endurance gates remain open.
