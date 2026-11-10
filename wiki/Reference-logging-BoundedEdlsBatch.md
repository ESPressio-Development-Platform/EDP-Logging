# Reference — bounded EDLS batching

Public optional API in [source](../src/logging/BoundedEdlsBatch.hpp); [normative contract](../docs/BOUNDED-EDLS-BATCHING.MD).

Templates: TMaximumRecords (1..64), TMaximumBytes (framed body capacity), TArchive (existing strict owner), TMonotonicClockProvider (canonical noexcept Now capability), TByteOperationsProvider (stateless Memory provider consumed by Bytes), TMaximumDwellNanoseconds (Bootstrap ceiling, default100ms). Stable noncopyable/nonmovable owner borrows archive/Clock/scratch and owns bounded Bytes. Caller serializes it; existing Threading worker schedules WaitUntil/service.

ConfigureDwell validates zero or finite dwell only while empty. Offer copies one verified CBOR EDLG frame, Buffered is RAM admission only; FlushRequired consumes no input. NextFlushDeadline anchors first record and cannot be extended by later arrivals. Service flushes full/due batches; Flush explicitly drains a partial batch. Destruction does not flush. EdlsBatchOutcome separates Status, PendingRecords, AcknowledgedRecords and nested Archive evidence (only meaningful after Flush calls owner).

Configuration outcomes: Configured valid; InvalidDwell negative/over-limit; PendingBatch refuses deadline extension; RecoveryRequired sticky failure; InsufficientScratch rejects invalid resource budget. Operation outcomes: Buffered admitted; FlushRequired unconsumed capacity/deadline refusal; Waiting not due; Empty no work; NotConfigured no Bootstrap policy; InvalidFrame malformed/non-CBOR; FrameTooLarge over-capacity; ClockRangeExhausted prevents deadline wrap; ClockRegressed fails closed; DurablyAcknowledged reports complete segment+NVS ACK; DurablyAcknowledgedPendingRetirement preserves ACK despite cleanup lag; RecoveryRequired prohibits blind retries after uncertainty.

Private archive_, clock_ and scratch_ are lifetime-stable borrows. bytes_ owns copied frames, records_ authoritative count, first_ current deadline origin, dwell_/configured_ Bootstrap policy, recoveryRequired_ sticky terminal guard. Observe builds non-ACK progress; IsDue is a pure predicate. Flush clears frames/count only after confirmed durable ACK. Unknown outcome retains bytes, returns zero ACK and blocks retries until Bootstrap reconciles archive and reconstructs the accumulator. No allocator/task/timer/filesystem is created. Host real-VFS/fake-NVS tests and exact negative contracts live in BoundedEdlsBatchTests.cpp; physical/scheduled-worker evidence is recorded separately.

Native synchronous 16-record profile: 256 durable record completions, 16 strict
NVS segment ACKs, initially 5.189 records/s. Independent physical raw LittleFS/NVS audit
passed on 30 October. Full measurements and qualification limits are in the
normative contract. Scheduled multi-producer use and 100 records/s remain
unqualified.

Corrected native GPIO17 cut (8.221 s OFF) and independent raw media audit PASS:
NVS ACK2 and both protected 16-record batches survived a complete unACKed third
segment; recovery then retained segments 3/4, records 33..64, NVS ACK4. Shared
electrical qualification rules are unchanged. Detailed run IDs and the earlier
failed fixture protocol attempt remain in the normative contract and handoff.

Existing whole-segment inspection now measures 51.680 durable records/s on the
same synchronous 16-record workload (p50 300615 us, p99 479627 us); all CRC and
NVS boundaries remain intact. Independent profile media audit passes. This is
not scheduled multi-producer or 100 records/s qualification. See the normative
contract for the separate power-cut outcome of this inspection change.

The complete segment read change also passed its fresh GPIO17 8.222 s cut and
independent post-cut raw-media audit in 20261030T141936-ad0f5730, retaining both
complete sealed segments 3/4, original records 33..64 and NVS ACK4. See the
normative contract for honest reset classification and remaining gates.

Existing upper-bound 64-record native configuration also passes compile, host
UBSan, independent media and a fresh 8.220 s POR-confirmed cut. The synchronous
256-record sample measures 132.828 durable records/s, p50 512326 us and p99
590018 us, sampled min heap 241848 B. It retains up to 128 records in two
segments, versus the 16-record fixture's 32. Dwell remains explicitly 100 ms.
These resource/latency/retention trade-offs and exact hashes are in the contract;
scheduled sustained ingestion and complete production remain unqualified.

The separately selected O2/500ms/160MHz managed native fixture passes its
finite2000-record two-producer workload, idle partial, bounded pressure,
complete durable drain/teardown and actual LittleFS/NVS audit in
20261030T180038-622f5b3b. This does not establish indefinite sustained rate.
See the normative contract for measured latency, resources and retained
earlier failures.

[Buildable container demo](../demos/ConfigurableBoundedEdlsBatch/README.MD)
now provides all three required targets. RPI400183532-8bfb4a6b builds and
184352-e1b78bb7 two-boot physical tests PASS for Arduino IDE, PlatformIO Arduino
and PlatformIO ESP-IDF. It uses small CBOR values, not full generated
LogRecords; the typed managed-worker fixture is separate. Full image and
filesystem verification precede application startup.

Extended O2/500ms/160MHz6000-record scheduled workload and independent media
audit232528-9e980d90 also PASS. All6129 phase records durably drain in the
experiment; scheduled complete-drain61.096605s, ACKp50812.873ms/p991257.849ms.
This qualifies a finite60-second offered100records/s sample, not indefinite
sustained rate. See the [qualification ledger](../docs/LOGGING-QUALIFICATION-STATUS.MD).


## Immutable application byte budget

The archive accepts a trailing byteCapacity constructor argument before initialisation. BoundedEdlsBatch respects the resulting smaller per-segment share during admission and deadline service. The bound includes EDLS files, the temporary successor and the 40-byte ACK value; provider-internal physical allocation overhead needs a separate application reserve. See [consumer and library-expansion contract](../docs/IMMUTABLE-PERSISTENCE-BYTE-CAPACITY.MD).

## Complete current source declaration and configuration audit (10 November 2026)

**Exact source at reviewed baseline:** [`BoundedEdlsBatch.hpp`](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/8d2b307a8a1db1aa5596abc904cd0da1f3c032cd/src/logging/BoundedEdlsBatch.hpp). This optional API imports existing EDP-BoundedTypes/Memory ByteOperations, EDP-Clock monotonic contracts and the established strict EDP-Persistence-backed NVS-ACK `EdlsPowerLossAcknowledgement` owner. It does not create its own timer, task, clock, filesystem or write serialization.

**Public `EdlsBatchConfigurationStatus` values:** **Configured (0)**, Bootstrap supplied valid dwell/capacity/scratch; **InvalidDwell (1)**, dwell negative or above compile-time ceiling; **PendingBatch (2)**, cannot reconfigure while records are retained; **RecoveryRequired (3)**, sticky archive uncertainty forbids resumption; **InsufficientScratch (4)**, no complete-segment workspace for the encoded frame/seal and provider profile; and **InvalidByteCapacity (5)**, actual Bootstrapped archive segment byte share cannot fit even one minimum 21-byte framed record body. InvalidByteCapacity is distinct from a missing scratch span: the application must enlarge its initial immutable logical archive allocation and reinitalize safely, not silently increase it after archive initialization.

**Public `EdlsBatchStatus` values:** **Buffered (0)** means a complete checked CBOR frame was copied into bounded volatile Bytes, **not durable**; **FlushRequired (1)** means pending batch must be serviced first and supplied new input **was not consumed**; **Waiting (2)** means pending batch is not due; **Empty (3)** means no retained frame; **NotConfigured (4)** means initial dwell/scratch are not approved; **InvalidFrame (5)** malformed/trailing/non-CBOR EDLG; **FrameTooLarge (6)** one frame or accumulated bytes would exceed the effective body limit; **ClockRangeExhausted (7)** dwell would overflow the monotonic range; **ClockRegressed (8)** monotonic clock moved backward relative to first record (sticky recovery required); **DurablyAcknowledged (9)** a complete segment and NVS watermark were independently verified by existing strict owner; **DurablyAcknowledgedPendingRetirement (10)** durable successor ACK is confirmed although safe prior-generation physical retirement is still incomplete; **RecoveryRequired (11)** storage/provider or generation uncertainty now requires explicit Bootstrap reconciliation rather than retry.

**Public `EdlsBatchOutcome` fields:** **Status** is one of the above; **PendingRecords : size_t** is current volatile uncommitted record count; **AcknowledgedRecords : size_t** is nonzero only for the currently completed fully ACKed batch (zero for buffering, waiting and failure); **Archive : EdlsAcknowledgedObservation** carries the actual strict owner operation status when Flush called it, and defaults to no transaction on other outcomes. These fields must not confuse source retirement, RAM acceptance, sink acceptance and physical durable ACK.

**Template `BoundedEdlsBatch<TMaximumRecords,TMaximumBytes,TArchive,TMonotonicClockProvider,TByteOperationsProvider,TMaximumDwellNanoseconds=100000000>`:** TMaximumRecords is exact finite records/segment (1–64), TMaximumBytes is compile-time maximum total framed body, TArchive is existing external strict acknowledged publisher whose maximum segment/frame size and configured segment byte share qualify this batch, TMonotonicClockProvider is the application's immutable source of `Clock::MonotonicTimestamp` via noexcept `Now()`, TByteOperationsProvider is a qualifying EDP-Memory byte-copy provider selected through `Bounded::Bytes`, and the final int64 parameter is the maximum permitted dwell in nanoseconds (default 100 ms, may include zero immediate dwell). All hard sizes, profile fit, positive count and Clock/byte-provider contracts are constrained at compilation.

**Private authoritative state:** `archive_` is a borrowed exclusive stable ACK publisher; `clock_` is the borrowed canonical Clock; `scratch_` is borrowed, nonaliased caller-owned full-segment write/scan workspace; `bytes_` owns the entire finite encoded frame batch, not borrowed producer spans; `records_` counts actual complete buffered frames; `first_` is the authoritative first admission timestamp while nonempty; `dwell_` is the Bootstrap-selected window unchanged while pending; `configured_` confirms valid initial configuration; `recoveryRequired_` latches any uncertain archive/Clock state until a separate Bootstrap reconciliation creates a safe new lifecycle. The class is deliberately noncopyable/nonmovable to keep all pointers and pending archival ownership stable. These are finite retained resources; no heap-backed unbounded growth occurs.

**Private helper `EffectiveBodyBytes()`** subtracts EDLS header and any sealed EDLC completion from the immutable application-selected per-segment byte share, clamps to TMaximumBytes, returns zero when insufficient; no additional storage allocation is made. `Observe(status)` returns current non-durable count/result with no invented archive ACK. `IsDue(now)` becomes true when exactly full by record count/byte share or elapsed canonical dwell since `first_`; later arrivals do not reset the clock.

**Public lifecycle:** constructor borrows stable archive, Clock and scratch without initialization. `ConfigureDwell(dwell)` checks failure/pending state, valid 0..maximum dwell, minimum actual byte capacity **and scratch for a full sealed segment**; only then sets `configured_`. `Offer(completeFrame)` validates one complete CBOR EDLG frame, payload length and configured effective byte bound, copies the original borrowed frame into owned Bytes, sets `first_` only on first admission and returns Buffered; a full/due batch returns FlushRequired **without consuming new input**. A regressing Clock latches RecoveryRequired, and a near-maximum timestamp cannot wrap a future deadline. `NextFlushDeadline()` yields `nullopt` for empty/failed, original first-coordinate when already full, otherwise the initial first + selected dwell; never extends due time. `Service()` checks canonical Clock, reports Waiting until due, and delegates only due batches to Flush. `Flush()` is an explicit partial/full batch transaction: calls the existing archive's `PublishBatch<TMaximumRecords>` with exact owned bytes and external scratch, accepts **only** typed AcknowledgedWithinCapacity/AndRetired/PendingRetirement, then increments ACK count and clears buffered Bytes/record count. Any failed/uncertain archive outcome retains bytes, returns zero ACK and latches RecoveryRequired, forbidding blind retry or overwrite. Destruction does not commit.

**Concurrency / ISR and retention:** Bootstrap serializes Offer, Service, Flush and ConfigureDwell in one existing worker context; none is ISR-safe or a second worker. Retention N is a **finite number of EDLS segments**, not N records, each holding at most TMaximumRecords. Larger configured logical archive byte capacity generally reduces Flash wear but offers no specific lifespan guarantee. The older synchronous 16/64 record experiments and newer named finite managed 100-rps WROOM profiles must be reported separately; source acceptance is not generic FileStorage PowerLoss atomicity. See [strict ACK architecture](../docs/EDLS-ACKNOWLEDGED-RECOVERY.MD), [bounded batching contract](../docs/BOUNDED-EDLS-BATCHING.MD) and [G1–G15 acceptance](../docs/ACCEPTANCE-G1-G15.MD).
