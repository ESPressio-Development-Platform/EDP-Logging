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
