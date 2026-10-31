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
