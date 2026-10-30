# logging/AdmissionLifecycleCoordinator.hpp

**PUBLIC opt-in G1 cancellation and admission-drain facade** (package 0.1.0; not default Logging umbrella; does not depend on platform concrete).

## Public types

- `AdmissionShutdownStatus : uint8_t`: `Quiescent=0`, `DeadlineElapsed=1`, `WakeRetryLimitReached=2`, `DrainWaitProviderFailure=3`.
- `AdmissionShutdownResult`: `Status`, separate `WakePublication` (Published/ProviderFailure), and diagnostic `ActiveAdmissions`.
- `AdmissionLifecycleCoordinator<TProducerCount,TWaitWakeProvider,TAtomicWordProvider=void>`: noncopyable and nonmovable; borrows Bootstrap-selected EDP-Threading BoundedWaitWake owner with at least **TProducerCount+1 slots**. Last slot reserved for one exclusive shutdown drainer. Owns atomics `_shuttingDown` and `_activeAdmissions`, and the existing `AdmissionWakeCoordinator` (no C++ heap, no extra task).

## Methods

- `RequestShutdown() noexcept`: monotonic one-way shutdown flag followed by best-effort wake publication to all producer slots; returns wake health separately. Caller must address failed publication.
- `AcquireUntil<MaxWakeRetries>(slot,pool,lease,absoluteMonotonicDeadline,args...) noexcept`: one exclusive slot per producer. Uses existing Memory **NoWait** and Threading WaitUntil with a **stop predicate**. Returns typed admission and wake health. `ShutdownRequested=10` means *no new lease*; `GrantedDuringShutdown=11` means **a new lease WAS granted and remains owned**, and caller must release explicitly. Active call count decremented on every exit and drain slot signaled on shutdown.
- `ShutdownUntil<MaxDrainWakes>(absoluteMonotonicDeadline) noexcept`: signals stop and event-drains in-flight *admission calls* on the Threading **reserved drain slot**; strictly one caller; finite wake budget and original deadline, no polling sleeps; reports active count and wake health.
- `ShutdownRequested() const noexcept`, `ActiveAdmissions() const noexcept`: lock-free state snapshots, not a substitute for task join.
- `ReleaseAndNotify(lease) noexcept`: forwards to the composed Memory lease return/wake fanout, remains usable during stop.

## Lifecycle and failure semantics

The application must **stop spawning callers, join all producer/consumer tasks, and release every Memory lease** before destroying the coordinator/Threading provider/MemoryTopology. Quiescent only counts active AcquireUntil invocations; it **does not track grants handed to consumers**. A native Memory finite ObjectPool Acquire remains prohibited due to stack waiter/unbounded mutex hazard; the G1 helper uses only NoWait. Every capacity release including late/racing grants MUST publish capacity. RAII destructors of raw Memory leases can bypass notification and remain an open G1 hazard. No G1 FIFO fairness, complete bounded provider latency, target/physical power-loss qualification or durable sink is claimed.

## Tests

`tests/AdmissionLifecycleTests.cpp` and `tests/run-admission-lifecycle-tests.sh` reuse the actual MemoryTopology host fixture and EDP-Threading fixed signal owner. GCC+Clang PASS RPI400 **20261016T082446-9a9fb25f**: 15 real two-producer shutdown cycles, no-new-admission, late grant ownership, failed wake and deliberately active producer timeout, and after-join quiescence retry. See [G1 lifecycle contract](../docs/ADMISSION-LIFECYCLE.MD), [prior capacity-release coordinator](Reference-logging-AdmissionWakeCoordinator) and [owner dependency graph](../DEPENDENCY_GRAPH.MD).

## Queue-before-Memory lifecycle additions

TrackAdmission() returns an internal noncopyable scope which registers an active admission before queue waiting begins and retires it after all rollback and Memory work. Nested counts with AcquireUntil's existing scope are intentional; all teardown must still follow task join and native lease return. PublishCapacityAvailable() forwards to the existing AdmissionWakeCoordinator producer wake fanout after an empty queue-slot retirement; it creates no additional signal. Quiescent does not certify sink callbacks or pool teardown. See [composed admission](Reference-logging-BoundedRecordAdmission).

## Producer slot validity helper

ValidProducerSlot(slot) is public static constexpr, no state mutation: true iff slot is below TProducerCount; false for shutdown-drainer slot TProducerCount and out-of-range slots. The bounded record composer uses it before the queue stage so a producer never borrows the shutdown-drainer Threading wait point.


## Canonical atomic binding

The existing coordinator optionally selects the existing Platform AtomicWord32
provider for its lifecycle words. Default void keeps the standard-atomic
always-lock-free assertion. Private AdmissionAtomicWordSelection chooses storage;
LoadAcquire, InitializeWord, PublishShutdown and ChangeActiveCount preserve
ordering over the selected operations. Scope registration/destruction and the
same drainer remain one algorithm. See [contract](../docs/ADMISSION-LIFECYCLE-ATOMIC-BINDING.MD).

GCC/Clang UBSan, standard and canonical Portable bindings, contended accounting,
real Memory and shutdown cases pass 20261031T000433-77e1c128.
Broader producer/queue/typed-Task/VFS regressions pass 000929-5a3dd6bc.
Actual three-target typed worker two-boot tests pass 001504-eee39309 with the
existing ESP-IDF hardware word and internal-DRAM qualification.
