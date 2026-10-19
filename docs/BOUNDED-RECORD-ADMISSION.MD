# Composed bounded record admission — opt-in G1

**Branch:** feature/logging-v1. **Package:** 0.1.0. **Status:** tested partial admission composition, not a complete Logging producer or runtime.

## Purpose and ownership

The public, optional header src/logging/BoundedRecordAdmission.hpp composes the existing TransactionalRecordQueue, AdmissionLifecycleCoordinator, EDP-Memory ObjectPool NoWait acquisitions, EDP-Threading fixed-slot BoundedWaitWakeProvider and the one canonical EDP-Clock MonotonicTimestamp. There is no new waiter registry, memory pool, clock timeline, platform concrete or new dependency direction. Application Bootstrap selects and retains all concrete providers.

Each producer owns an exclusive EDP-Threading wake slot, an initially empty caller-owned Memory scratch lease and an inactive move-only queue Reservation. AcquireRecordUntil<MaximumQueueWakes,MaximumMemoryWakes> first reserves the queue entitlement before trying to acquire a potentially scarce Memory lease, then uses the existing lifecycle's NoWait Memory admission, attaches the acquired lease to that reserved queue slot, and returns the active exclusive Reservation. The caller fills the record and explicitly commits or aborts; an uncommitted guard aborts on destruction. Successful admission does not certify commit, sink processing, transport or durability.

## Deadline, shutdown and rollback

The caller supplies a single absolute canonical EDP-Clock monotonic deadline. Each queue mutex operation receives the remaining relative native EDP-Platform WaitTimeout measured from that same deadline. EDP-Threading WaitUntil consumes that original absolute deadline. Queue and Memory retry limits are compile-time constants; wake does not reset the deadline. These constraints apply to the admission owner operation, not arbitrary application computation or beyond the timing guarantee of the chosen native provider.

TrackAdmission counts the entire queue wait, Memory admission and rollback against the existing lifecycle shutdown drain. The inner Memory AcquireUntil is also counted in the same coordinator for its short lifetime. ShutdownRequest wakes all existing producer slots. ShutdownUntil cannot report quiescence while an active queue-stage producer has yet to exit. This does NOT drain accepted source records, outstanding leases, sink callbacks, buffers or application Tasks: Bootstrap must stop new calls, join all workers and return every outstanding owner before teardown.

RecordAdmissionOutcome contains Status, Queue, Memory, Release, MemoryAttempted, ReleaseAttempted. Its strong RecordAdmissionStatus values are Admitted, DeadlineElapsed, ShutdownRequested, QueueWakeRetryLimit, QueueWaitProviderFailure, QueueFailure, MemoryFailure, LateMemoryGrantReturned, ReleaseUnconfirmed, ReleaseNotificationFailed and OutputOccupied. Failed Memory release leaves the scratch lease owned; a failed queue abort leaves the RAII reservation active and reports the problem. The success status never hides notification failure. Late grants are explicitly returned through the existing coordinator or remain caller-owned when release cannot be confirmed.

## Resource and safety limits

No dynamic allocation is introduced by this optional composition. The queue, producers, Threading wake slots and Memory pool remain statically bounded by Bootstrap profiles. Native finite EDP-Memory ObjectPool waiter safety is still an independent defect (EDP-Memory#16); this composition ONLY calls EDP-Memory NoWait. No ISR safety guarantee. Direct producer-to-LogRecord capture, stable governed root TypeIdentifier, cross-revision coherent Clock/Mesh, downstream sinks and physical concurrency/HIL qualification are not yet implemented. These semantics should not be presented as production-ready or full G1 closure.

## Tests

From EDP-Logging, with six exact pinned abstract EDP siblings: CXX=g++ bash tests/run-bounded-record-admission-tests.sh (also CXX=clang++). Test covers real Memory/Threading admission, queue capacity saturation without premature pool ownership, published release wake, finite deadline, cancellation and a queue-stage producer counted during shutdown. Run transactional queue and admission lifecycle suites alongside. The authoritative Policy/main handoff records exact detached RPI400 run identifiers and remaining qualification gaps.

## Producer-slot authority and independent publication health

The lifecycle coordinator's new ValidProducerSlot(slot) checks the exclusive producer range and rejects the dedicated shutdown drainer slot (or any out-of-range value) **before** a queue slot is claimed or a Threading WaitUntil is attempted. InvalidProducerSlot (strong enum value 11) leaves all caller-owned Queue and Memory state unchanged. RecordAdmissionOutcome also includes the independent WakePublication status: the original AdmissionWakePublicationResult produced on an actual Memory grant is retained even if the returned RecordAdmissionStatus is Admitted. A successful reservation with a failed producer wake is not a silently healthy operation; Bootstrap must respond to that provider failure. When both a native Memory release and Queue abort fail, ReleaseUnconfirmed remains the top-level ownership-uncertainty status and Queue carries the second failure. Tests include genuine Memory grants with an injected Threading wake failure and invalid use of the reserved shutdown slot. This remains incomplete G1, not finished physical qualification.


## 2026-10-19 — physical full-queue deadline evidence

New opt-in native HIL test uses real ESP32 FreeRTOS EDP-Threading,
EDP-Clock, EDP-Memory and the two-slot queue under a 40 ms producer
deadline. Requires a DeadlineElapsed result, no Memory acquisition,
no outstanding pool lease/reservation, zero live records and queue
capacity returned, within a 60 ms measured wall-clock check.
Separate verified flash plus three independent actual WROOM runs
were **PASS**, measured 31.810, 31.808, 31.678 ms, factory SHA256
e4bb59b63d1e0d3c03c7a17bf95bf68da43589184276dae330f340464e0b1f1b.
Exact jobs and capture-hash proof are in
[HIL validation](../tests/hil/g1-espidf/VALIDATION.MD).
GCC+Clang actual Memory/Threading owner admission tests
20261019T125527-7f3a13e2 PASS exit0.

This proves the **full queue / no-memory-borrow** deadline path,
not all contended native internal locks, coordinator shutdown,
multiple producer slots and all strict product worst-case bounds.
G1 release remains open.



## 2026-10-19 — independent native cross-core queue-mutex contention

Beyond the previously physically qualified 40 ms **full queue**
timeout, actual ESP32 FreeRTOS Core 1 now locks
the same EDP-Threading queue mutex while Core 0 attempts
AcquireRecordUntil with a 40 ms EDP-Clock absolute deadline.
Three fresh physical runs PASS with observed **40.035 ms**
elapsed each; DeadlineElapsed, MemoryAttempted=false, scratch
lease empty, no queue output owner, zero live records and
verified true-owner mutex release. Existing 18/18 HIL probes
remain PASS. Full build, flash verify, capture jobs and
SHA-256 recorded in HIL VALIDATION.MD.

The first two images failed because the test owner task
poll delay `pdMS_TO_TICKS(2)` rounded to **zero RTOS ticks**,
allowing premature mutex release and legitimate producer
admission. The test now uses `vTaskDelay(1U)` and
demonstrated the intended actual held-mutex path.
These tests are *not* exhaustive all-provider cancellation
or all-race qualification.
