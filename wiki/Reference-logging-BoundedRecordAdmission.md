# logging/BoundedRecordAdmission.hpp

**PUBLIC opt-in G1 composition**, not part of the ordinary Logging umbrella. Source src/logging/BoundedRecordAdmission.hpp on feature/logging-v1. Canonical normative contract: [bounded record admission](../docs/BOUNDED-RECORD-ADMISSION.MD).

## Public result contract

RecordAdmissionStatus : uint8_t has all explicitly assigned values: Admitted=0 (queue reservation owns a lease but is not published); DeadlineElapsed=1; ShutdownRequested=2; QueueWakeRetryLimit=3; QueueWaitProviderFailure=4; QueueFailure=5; MemoryFailure=6; LateMemoryGrantReturned=7 (late grant is being returned); ReleaseUnconfirmed=8 (native Memory still owned); ReleaseNotificationFailed=9 (required wake not confirmed); OutputOccupied=10 (caller input/output owner not empty). Each failure must be distinguished from successful admission and from downstream sink/commit processing.

RecordAdmissionOutcome stores Status (composed truth), Queue (queue-owner result), Memory (admission-owner result), Release (release-owner result), MemoryAttempted and ReleaseAttempted (whether remaining fields were actually produced by an operation). Outcome has no hidden dynamic storage. Failure can leave an explicitly owned Reservation or scratch lease requiring attention; never discard an active lease.

## Public function template

AcquireRecordUntil<MaximumQueueWakes,MaximumMemoryWakes,TQueue,TAdmissionCoordinator,TWaitWakeProvider,TObjectPool,TLease,TStopPredicate,TArguments...> accepts queue, admission, waitWake, exclusive producerSlot, ObjectPool, initially empty scratchLease, inactive Reservation output, one canonical EDP-Clock deadline, noexcept stopRequested predicate, and nothrow forwarded object-construction arguments. Each maximum-wake parameter statically bounds one retry phase; TQueue selects the fixed topology and owner guard; TAdmissionCoordinator owns Memory NoWait/shutdown counters/releases; TWaitWakeProvider is Bootstrap's existing EDP-Threading latched wait; TObjectPool/TLease are actual EDP-Memory resource contracts. Every provider, queue and owner must outlive all borrowers. The function reserves queue capacity, waits for a bounded wake if full, acquires Memory with NoWait through the lifecycle, checks stop/deadline and attaches the lease. The caller must still Commit or Abort the guard.

## Internal helper and lifecycle

Detail::RemainingRecordAdmissionWait(deadline) is an INTERNAL API, returning the EDP-Platform native mutex wait budget derived from Clock::MonotonicNow and the provided canonical timestamp; it creates no independent timeline. The helper does not own user task scheduling. AdmissionLifecycleCoordinator::TrackAdmission guards the full queue wait and Memory phase; the inner AcquireUntil scope temporarily adds one more admission count. All real queue slot releases use the original producer-slot wake fanout, not an invented second Signal. Shutdown quiescence still does not imply that all records, sink callbacks or external tasks are finished.

There is no persistent state in this header beyond the owning Queue/Lifecycle/Memory/Threading objects. ISR use is unsupported, and EDP-Memory's native finite ObjectPool path is prohibited (EDP-Memory#16). Host GCC/Clang + genuine Memory/Threading tests exercise the opt-in composition, not physical ESP32 runtime, full producer semantics or flash durability.

## Extended safety qualifiers

RecordAdmissionStatus::InvalidProducerSlot=11 rejects a slot reserved for shutdown or outside the configured producer range *before* Queue reservation/wait. RecordAdmissionOutcome::WakePublication is an independently returned AdmissionWakePublicationResult (NotAttempted, Published or ProviderFailure); it is copied from the existing coordinator's Memory-grant wake fanout, so an Admitted result can still reveal a failed notification. ReleaseUnconfirmed retains highest ownership-uncertainty priority when a failing Memory release coincides with unsuccessful queue-abort rollback; the Queue field exposes the additional error. Genuine EDP-Memory integration regression exercises the reserved shutdown slot and injected failed broadcast while preserving successful exclusive record ownership.


## 2026-10-19 physical native full-queue deadline

The ESP32-D0WD real FreeRTOS HIL now checks a completely occupied
two-slot queue, requested 40 ms Clock monotonic producer deadline,
actual DeadlineElapsed result within a 60 ms measured bound, **no
EDP-Memory lease acquisition**, zero live records, and complete queue
slot reclamation. Independent on-chip flash verification and three
new UART/NVS-restart cycles PASS (31.810 / 31.808 / 31.678 ms),
while retaining 18/18 earlier HIL markers. Full GCC/Clang real
Memory+Threading owner admission tests also PASS. These are narrow
actual physical evidence, not G1 comprehensive strict-timeout
or full product release qualification.


## Real ESP32 two-core Queue mutex test — 2026-10-19

Actual EDP-Threading OrdinaryMutexProvider is held by
Core1 task while Core0 attempts AcquireRecordUntil
with 40 ms absolute Clock deadline. Fresh verified
WROOM firmware three cycles PASS at 40.035 ms each,
no Memory borrowed, no output reservation, zero
live records, mutex released by actual holder.
A predecessor test erroneously used a zero-tick
FreeRTOS poll delay; corrected to one tick and
full hardware test rerun. Actual deadline proof is
bounded to this scenario, not product worst-case.


## 2026-10-19 — real Memory object-pool exhaustion

Two live native WROOM EDP-Memory Record leases fully
occupy the dedicated pool while Queue slots remain free;
third AcquireRecordUntil reserves/rolls back its Queue slot
and times out at 36.863 ms of a requested 40 ms Clock
deadline, without owning scratch/queue after timeout.
Both pre-existing leases release through the coordinator
with successful producer wake publication, live records
return to zero. Three independently captured physical
runs and all 18/18 HIL markers PASS.


## 2026-10-19 real dual-core exhausted-Memory shutdown

With both native EDP-Memory object slots held, Core1
producer enters finite AcquireRecordUntil and is counted
as active while waiting for Memory. Core0 requests real
coordinated shutdown, the worker wakes, yields
ShutdownRequested, terminates and joins, while
ReleaseAndNotify returns original held leases. Three
physical runs PASS 292–293µs shutdown coordination,
zero active admissions and live objects, full 18/18
HIL regressions intact. Adversarial late grant and
provider failure remain open.
