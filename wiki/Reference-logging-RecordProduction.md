# RecordProduction — Mesh-independent bounded Logging

Status: partial G1/G10, version 0.1.0.

Source: logging/RecordProduction.hpp. Bootstrap composes an ordinary
EDP-Threading sequencer Mutex, monotonic/synchronized EDP-Clock capture
provider, EDP-Memory topology, EDP-BoundedTopology FIFO and sink callback.
No EDP-Mesh import, dependency or implementation exists in Logging.
Monotonic-only Clock is valid. Optional Mesh-referenced synchronized
Clock coherence belongs to the application's Clock/Mesh integration.

ProduceRecordUntil issues a non-wrapping ticket and captures time ONCE
before bounded queue/Memory waits, then commits an exclusive FIFO record.
DrainOneRecord borrows an EDP-Memory lease to one bounded sink callback and
attempts explicit native release. Sink Accepted, Rejected, Partial and
ProviderFailure never imply flushed, physically received or durable.
Failed native release retains exclusive source ownership for Bootstrap.

GCC and Clang host tests, 3x physical ESP32-D0WD test app runs with
real FreeRTOS/Memory, EDP-Serialisation CBOR and CRC-verified EDLG UART
output passed in the original G1 application fixture. Its source
payload was a **test-only positional tuple**, not a permanent wire
record. Since those early tests, the separately governed 15-field
LogRecord identity and generated wire projection have been implemented
in the Logging source; `ProduceRecordUntil` here remains a **generic
scalar/enum producer envelope** and never claims to construct that
canonical root representation. See [Record production ownership](../docs/RECORD-PRODUCTION-OWNERSHIP.MD),
[generated wire projection](Reference-logging-GeneratedLogRecordWireProjection)
and [G1 fixture validation](../tests/hil/g1-espidf/VALIDATION.MD).

## Bounded fanout — DrainOneRecordToSinks

Drains one native Memory-owned source to 1..8 application-supplied
noexcept synchronous sinks in argument order and returns
RecordFanoutOutcome<TResultCount>. Source is the underlying
RecordDrainOutcome; Submissions is a fixed array of native sink
statuses; Attempted, Accepted, Unaccepted are exact counts.
IsFullySubmittedAndReclaimed() requires all sinks accept and native
Memory reclamation succeeds. Each sink is called exactly once,
including after another sink rejects; no automatic retry. Borrowed
source contents become invalid after the final sink and reclamation.
This is not an asynchronous retained-buffer fanout, nor proof of
durability, physical wire receipt or full LogRecord root identity.


## Entry snapshot safety (2026-10-17)

`ProduceRecordUntil` now captures one qualified timestamp and an
owned finite tuple of scalar/enum source values at instrumentation
entry, **before** even the sequencer mutex can block. A failure after
this source timestamp reports `TimeCaptured=true` even without an
issued ticket; it does not imply any accepted or emitted record.
Use the generated schema-owned LogRecord assembler for bounded text,
bytes and complex strong parameter Types rather than borrowed spans or
pointer-bearing aggregates in the generic constructor. This partial
G1 producer still requires full application Bootstrap composition.


## Bounded coordinator quantum (2026-10-17)

`logging/CoordinatorQuantum.hpp` provides
`DrainCoordinatorQuantum<N>(queue, callerOwnedClaim, sinks...)` for
1..8 statically composed synchronous sinks and at most `N` records.
Typed `CoordinatorQuantumStatus` discriminates empty, consumed
budget, claim failure, uncertain reclaim and an unresolved prior claim.
It preserves per-sink accepted/rejected counts and release-notification
anomalies. The executor is intentionally **not** a worker, wake
provider or detached Thread: Bootstrap must supply the actual
EDP-Threading Dedicated Thread and stop/join lifecycle. The current
scope is finite synchronous fanout, not persistence durability.

## Topology-owned dedicated worker (2026-10-17)

The optional `logging/DedicatedCoordinator.hpp` is an
EDP-Threading `ThreadContext&` callable suitable for binding using
`BindDedicatedThread<Identity>`. It composes bounded source
quanta, native ThreadContext finite wake, per-sink report accounting
and a distinct cooperative exit. Bootstrap owns the actual
`StaticThreadingRuntime`, stable Queue, Memory provider, sinks, native
execution and target wake. No secondary scheduler or worker pool is
created. The callback does NOT prove end-to-end production
LogRecord serialization, task stack limits, power-loss durability,
or async encoded-buffer lifetime. See
[Dedicated Coordinator](../docs/DEDICATED-COORDINATOR.MD).

## Encoded representation buffer ownership (2026-10-17)

`logging/OwnedRepresentationFanout.hpp` provides an optional
EDP-Memory-pool-owned, representation-selected CBOR/JSON buffer
whose source bytes live until all synchronous copy-on-accept sinks
have been offered the same one-time-encoded bytes. One sink's
rejection never retracts another sink's accepted enqueue;
unknown EDP-Memory release leaves the caller's lease explicit.
This is not a shared asynchronous borrowed buffer or a final
15-field TypedEnvelope writer. See
[Owned Representation Fanout](../docs/OWNED-REPRESENTATION-FANOUT.MD).

## Single-sink coordinator contract (2026-10-22, #10)

The record fanout, bounded quantum and EDP-Threading dedicated coordinator
accept **1–8** statically selected synchronous copying/consuming sink
callbacks, consistently with the existing G10 representation fanout.
Zero sinks remain a compile-time error. Exactly one sink requires no
dummy second sink, introduces no additional worker, queue, memory owner
or asynchronous borrowed lifetime, and retains the same per-sink
Accepted/Rejected/Partial/ProviderFailure evidence and source-lease
reclamation contract. Host coverage uses one sole sink across direct
drain, finite quantum and DedicatedCoordinator stop/wake. No durability
or physical-transport claim follows from sink acceptance.

## Declaration-by-declaration source contract (9 November 2026)

This section follows the *current* `src/logging/RecordProduction.hpp`, including the two internal `Detail` helpers. The earlier dated sections above record how the facility evolved; the declarations below describe the currently selected source. All functions use caller-supplied existing owners and introduce no scheduler, allocator, additional queue or runtime sink.

### Producer entry, result and non-wrapping attempt

- `RecordProductionStatus` is a fixed `uint8_t` enum: `Published=0` means only the queue commit succeeded; `OutputOccupied=1` refuses a nonempty scratch lease or active caller reservation; `InvalidProducerSlot=2` rejects an unassigned producer; `StopRequested=3` prevents new admission; `SequencerTimedOut=4` and `SequencerProviderFailure=5` distinguish existing mutex acquisition outcomes; `SequencerReleaseUnconfirmed=6` preserves a failed/unknown mutex release; `AttemptExhausted=7` refuses counter wrap; `AdmissionFailed=8` reports a bounded queue/Memory failure; and `CommitFailed=9` reports an unsuccessful publication commit. None asserts sink or flash durability.
- `RecordProductionOutcome<TWidth>` exposes `Status`, the issued/non-issued `Attempt`, the separate typed `Admission` outcome, the native queue `Commit` result, and `TimeCaptured`. The latter records that time was captured even when the attempt later fails; it is not a published-record indicator.
- `ProduceRecordUntil<TWidth,TMaximumQueueWakes,TMaximumMemoryWakes,...>` captures exactly one `CaptureLogTime(temporal)` sample and a finite owned tuple of caller scalar/enum arguments **before** acquiring the sequencer or waiting for capacity. It refuses active output handles, invalid slots and shutdown. It invokes the existing EDP-Threading ordinary mutex with the canonical remaining deadline; under that lock it issues the non-wrapping attempt and releases the lock with explicit release-failure status. It delegates bounded Memory/queue admission to `AcquireRecordUntil`, then invokes `Reservation::CommitUntil(deadline)` when present or the already-owned `Commit()` contract otherwise. On commit failure, caller-held scratch/Reservation ownership is **not** silently discarded. Pointer/spanned/borrowed complex arguments are compile-time excluded; the generated LogRecord assembly owns those representations.

### Native consumer claim and one synchronous sink

- `RecordSinkSubmission` is an independent fixed `uint8_t` enum: `Accepted=0`, `Rejected=1`, `Partial=2`, `ProviderFailure=3`. Accepted means that one synchronous copying sink accepted submission, **not** that its bytes were written durably or physically received.
- `RecordDrainStatus` distinguishes `Empty=0`, `AcceptedAndReclaimed=1`, `FailedAndReclaimed=2`, `ClaimFailure=3`, and `ReclaimUnconfirmed=4`. Failed sink submission is not mislabelled as a failed native Memory release.
- `RecordDrainOutcome` has `Status`, original queue `Pop`, actual queue `Reclaim`, sink `Submission`, and `SubmissionAttempted`; a caller can separately inspect whether the callback ran and whether the source's ownership returned to Memory.
- `DrainOneRecord<TQueue,TSink>` is the public original no-wait entry point. It forwards to the same internal state machine with the no-wait policy; only the caller-supplied native claim and lease are consumed, never duplicated or implicitly retained.
- `Detail::DrainOneRecordUnderQueuePolicy<TBoundedQueueWait,TQueue,TSink>` is the internal shared implementation. It rejects a previously active caller claim, selects canonical `queue.PopUntil` vs original `queue.Pop`, checks both result and active ownership, borrows the source through the claim's EDP-Memory lease and calls the synchronous `noexcept` sink at most once. A sink must return `RecordSinkSubmission`; the lease remains borrowed only for that call. It then executes `CompleteUntil` or `Complete`, with a **separate new retirement deadline** for bounded mode. `ReleasedNotificationFailed` still represents a completed native release with failed notification, whereas any genuinely unconfirmed reclaim returns `ReclaimUnconfirmed` and leaves ownership visible to Bootstrap. No callback result upgrades storage durability.

### Bounded fanout and internal queue-locking policy

- `RecordFanoutOutcome<TResultCount>` is bounded at compile time to **1–8** sinks. `Source` is the one native `RecordDrainOutcome`; `Submissions` is a fixed array of per-sink outcomes; `Attempted`, `Accepted`, `Unaccepted` are the exact invocation/acceptance counts. `IsFullySubmittedAndReclaimed()` returns true only for an accepted native source reclamation with every configured sink attempted and accepted; it does not claim durable delivery.
- `DrainOneRecordToSinks<TQueue,TSinks...>` is the public original no-wait fanout wrapper; it dispatches the same shared source-claim state machine with the no-wait policy and a fixed sink pack.
- `Detail::DrainOneRecordToSinksUnderQueuePolicy<TBoundedQueueWait,TQueue,TSinks...>` is the internal bounded/no-wait fanout implementation. It initializes each per-sink result to `ProviderFailure` before dispatch, calls each supplied `noexcept` callback exactly once in argument order under one source claim, counts each returned result, and aggregates full acceptance, partial acceptance and provider failure without hiding any individual result. It delegates claim/reclaim to `DrainOneRecordUnderQueuePolicy`; a failed sink does not retract prior acceptances or trigger automatic retry. The bounded policy passes its canonical queue-lock wait to source claim and retirement. Source borrows expire at return; asynchronous consumers must copy/own payloads themselves.

### Ownership boundaries and verification

`CaptureLogTime`, the sequencer's `Issue/Release`, queue claim `Borrow` and Memory lease `Get` are calls to separately owned Clock/AttemptSequence/Threading/queue/Memory contracts, **not additional methods declared by RecordProduction.hpp**. This separation prevents duplicate-owner APIs in Logging. Source validation includes [record production ownership](../docs/RECORD-PRODUCTION-OWNERSHIP.MD) and maintained [RecordProduction host tests](../tests/RecordProductionTests.cpp). The 9 November 2026 exact-source/current-main 68-case GCC+Clang integration suite `20261109T090446-792155c2` **passed 68/68**; the selected production source has not changed as part of this Wiki-only clarification. All package versions remain 0.1.0.
