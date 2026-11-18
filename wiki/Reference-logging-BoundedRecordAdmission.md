# logging/BoundedRecordAdmission.hpp

**Classification:** PUBLIC opt-in G1 composition. Import [BoundedRecordAdmission.hpp](../src/logging/BoundedRecordAdmission.hpp) explicitly to use this contract. Canonical normative contract: [bounded record admission](../docs/BOUNDED-RECORD-ADMISSION.MD).

## Public result contract

RecordAdmissionStatus : uint8_t has all explicitly assigned values: Admitted=0 (queue reservation owns a lease but is not published); DeadlineElapsed=1; ShutdownRequested=2; QueueWakeRetryLimit=3; QueueWaitProviderFailure=4; QueueFailure=5; MemoryFailure=6; LateMemoryGrantReturned=7 (late grant was reclaimed through the release path); ReleaseUnconfirmed=8 (native Memory still owned); ReleaseNotificationFailed=9 (required wake not confirmed); OutputOccupied=10 (caller input/output owner already active); InvalidProducerSlot=11 (slot is outside the producer range). Each failure must be distinguished from successful admission and from downstream sink/commit processing.

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


## 2026-10-19 real two-core Memory capacity arrival

New opt-in native test runs Core1 producer under a bounded Clock
deadline while Core0 controls release from actual EDP-Memory.
A grant well before the deadline is admitted then released; capacity
that arrives only *after* a timed-out producer has exited cannot
resurrect its output reservation or retain an object. Full
18/18 prior hardware markers and 3/3 independent real-device
UART/NVS restart captures PASS. The exact race where the
Memory provider grants after deadline but before the consumer
observes it remains a separate qualification gap.

## G1 2026-10-20: late real Memory grant ownership rollback

Test-only `g1_injected_late_grant.hpp` wraps actual EDP-Memory
ObjectPool NoWait admission, delays a genuinely successful return
beyond the Clock deadline, and checks production admission ownership
rollback. Actual native ESP32 build, flash+independent verification
and 3/3 real UART/NVS-restart runs PASS, with
`LateMemoryGrantReturned` / `GrantedAfterDeadline`,
`ReleasedAndNotified`, successful wake publication, empty lease,
no reservation, no live records. All earlier 18 markers PASS.
The delay is deliberate HIL fault injection, not native-pool
runtime behavior. No new production dependency or task is introduced.

## G1 2026-10-20: final queue attachment deadline

A successful native Memory grant does not satisfy an admission
deadline unless the final queue attachment completes on time.
`AcquireRecordUntil` now checks EDP-Clock after
`Queue::AttachLease` and rolls back an attached, real Memory lease
through Queue::Abort on deadline or shutdown; failure to clean up
remains an explicit error. A test-only real EDP-Threading mutex
shim delays the second lock acquisition beyond a 40ms deadline.
GCC+Clang host tests, ESP32 firmware build, independent flash
digest and 3/3 real UART runs PASS, each observed 89.623ms,
no live records, no active reservation, 18/18 prior markers.
This is controlled late-completion injection, not a product
latency benchmark.


## 2026-10-20 08:38–08:42 CEST — native post-attachment cancellation

After the G1 post-AttachLease canonical deadline fix, a second test
exercises **cancellation becoming visible during** the final native
Queue::AttachLease. The HIL-only provider delegates to a *real*
EDP-Threading mutex and raises a test stop predicate on its second
successful timed Acquire, then introduces an 80ms + FreeRTOS tick
post-acquire delay. The actual EDP-Memory lease transfers to the
Queue and MUST be released by the normal Reservation abort path.
The expected result is `ShutdownRequested` (2), rather than
`Admitted`, even while the queue attachment returns Succeeded.
This is an injected stop predicate, not a claim that real lifecycle
ShutdownUntil or all inter-core stop races were exhausted.

Host real owner-composed GCC and Clang admission tests detached
`20261020T083848-50c62356` **PASS exit0**. Actual complete
PioArduino/ESP-IDF WROOM firmware build detached
`20261020T083937-a40a64b6` **PASS exit0**, factory SHA256
`360ef7ad8b732b32510e2f089318950a7a6df4e412b5b265a18da0cb0c7742c3`.
Real write at 0x0 followed by separately verified on-chip flash
digest `20261020T084050-323a7b0b` **PASS exit0**.
Three independent host-triggered native reset/NVS-restart UART cycles
`20261020T084135-8228e918` **PASS exit0**, each 18/18
original markers, full EDLG CRC/resource/G11 instrumentation.
Every cycle emitted `G1_CANCEL_DURING_ATTACH|locks=2|status=2|
memory=0|queue=0|live=0|elapsed_us=89563–89564|valid=1`;
no owner lease or admission was retained. Private capture files
at `/srv/espressio/scratch/gpt6-logging-20261019-g11/g1-final-attach-cancel-captures-20261020`,
checksum-manifest SHA256
`1bd6c5bde1d9de5a12e9d7aa72fe8296aa786d120982292d3e626fd183396f24`.
No arbitrary full-product timing, natural race probability,
unexpected external power cut or complete Logging release implied.

