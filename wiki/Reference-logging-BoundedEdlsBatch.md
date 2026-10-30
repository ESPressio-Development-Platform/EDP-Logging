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
