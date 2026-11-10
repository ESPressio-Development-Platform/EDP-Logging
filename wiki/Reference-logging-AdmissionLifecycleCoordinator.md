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
- `IsShutdownRequested() const noexcept`, `ActiveAdmissions() const noexcept`: lock-free state snapshots, not a substitute for task join.
- `ReleaseAndNotify(lease) noexcept`: forwards to the composed Memory lease return/wake fanout, remains usable during stop.

## Lifecycle and failure semantics

The application must **stop spawning callers, join all producer/consumer tasks, and release every Memory lease** before destroying the coordinator/Threading provider/MemoryTopology. Quiescent only counts active AcquireUntil invocations; it **does not track grants handed to consumers**. A native Memory finite ObjectPool Acquire remains prohibited due to stack waiter/unbounded mutex hazard; the G1 helper uses only NoWait. Every capacity release including late/racing grants MUST publish capacity. RAII destructors of raw Memory leases can bypass notification and remain an open G1 hazard. No G1 FIFO fairness, complete bounded provider latency, target/physical power-loss qualification or durable sink is claimed.

## Tests

`tests/AdmissionLifecycleTests.cpp` and `tests/run-admission-lifecycle-tests.sh` reuse the actual MemoryTopology host fixture and EDP-Threading fixed signal owner. GCC+Clang PASS RPI400 **20261016T082446-9a9fb25f**: 15 real two-producer shutdown cycles, no-new-admission, late grant ownership, failed wake and deliberately active producer timeout, and after-join quiescence retry. See [G1 lifecycle contract](../docs/ADMISSION-LIFECYCLE.MD), [prior capacity-release coordinator](Reference-logging-AdmissionWakeCoordinator) and [owner dependency graph](../DEPENDENCY_GRAPH.MD).

## Queue-before-Memory lifecycle additions

TrackAdmission() returns an internal noncopyable scope which registers an active admission before queue waiting begins and retires it after all rollback and Memory work. Nested counts with AcquireUntil's existing scope are intentional; all teardown must still follow task join and native lease return. PublishCapacityAvailable() forwards to the existing AdmissionWakeCoordinator producer wake fanout after an empty queue-slot retirement; it creates no additional signal. Quiescent does not certify sink callbacks or pool teardown. See [composed admission](Reference-logging-BoundedRecordAdmission).

## Producer slot validity helper

IsValidProducerSlot(slot) is public static constexpr, no state mutation: true iff slot is below TProducerCount; false for shutdown-drainer slot TProducerCount and out-of-range slots. The bounded record composer uses it before the queue stage so a producer never borrows the shutdown-drainer Threading wait point.


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

## Full source-level state and lifetime contract (10 November 2026)

**Exact reviewed source:** [`AdmissionLifecycleCoordinator.hpp`](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/8d2b307a8a1db1aa5596abc904cd0da1f3c032cd/src/logging/AdmissionLifecycleCoordinator.hpp). The optional direct imports are existing abstract **EDP-Clock, EDP-Threading and EDP-Platform AtomicWord32**, with nested admission requiring existing Memory NoWait. No independent provider, Task, queue, heap or unbounded waiter array is created here.

**Public `AdmissionShutdownStatus` enum:** **Quiescent (0)** means zero observed active producer *operations*, **DeadlineElapsed (1)** means canonical EDP-Clock absolute deadline passed while operations remained, **WakeRetryLimitReached (2)** means the bounded number of drain wake transitions was exhausted before quiescence, **DrainWaitProviderFailure (3)** means the existing Threading drain wait returned a provider failure. `AdmissionShutdownResult` has public **Status**, separate **WakePublication : AdmissionWakePublicationResult** (truth of stop wake publication, which may fail even if operations become quiescent), and **ActiveAdmissions : uint32_t** (last observed in-flight count, **not** a number of still-owned Memory leases). These fields never certify that the application joined all tasks or flushed every sink.

**Class template parameters:** `TProducerCount` is the exact positive number of uniquely assigned producer slots; `TWaitWakeProvider` is a borrowed Bootstrap-owned Threading bounded wait/wake object whose compile-time Capacity is strictly greater than TProducerCount, preserving the final slot for a *single shutdown drainer*; optional `TAtomicWordProvider=void` selects a preexisting canonical Platform AtomicWord32 provider, with void retaining the standard C++ always-lock-free 32-bit atomic contract. The class is noncopyable and nonmovable so its active scope pointers and reserved slot remain valid. `Detail::AdmissionAtomicWordSelection<TAtomicWordProvider>` maps to the existing provider's validated `Word` Type; its `void` specialization selects `std::atomic<uint32_t>` and rejects unsupported targets rather than bypassing the invariant.

**Every private state member:** `_waitWake : TWaitWakeProvider*` borrows stable external Threading wake topology; **`_coordinator : AdmissionWakeCoordinator<TProducerCount,TWaitWakeProvider>`** is the sole already-owned composed producer admission/capacity-notification path; **`AtomicWord`** alias is the exact chosen owner-provided atomic representation; **`_shuttingDown`** is monotonic 0=open or 1=stop published; **`_activeAdmissions`** is a count of actual active scope registrations, incremented on entry and decremented on every exit; and **`DrainSlot = TProducerCount`** is the constant reserved, never-registered-as-producer wake identity. Atomic state is authoritative, not telemetry-only. No counter includes an accepted native Memory lease after its acquisition call exits.

**Private operations:** `LoadAcquire(word)` acquires an ordered snapshot of either standard or Platform-backed word; `InitializeWord(word)` explicitly sets initial zero using relaxed semantics, never assuming a native provider's implicit default; `PublishShutdown()` performs a release-ordered store of terminal one before waking tasks; `ChangeActiveCount(+1/-1)` retains exactly-once registration using std fetch_add/sub or a provider's successful CompareExchangeAcqRel loop. `ActiveAdmissionScope` privately owns its **Owner** pointer; its constructor increments count **before** any queue wait/Memory operation; its destructor decrements count **even on error/late grant**, then best-effort wakes the existing drain slot if shutdown already published. Copy/assignment are prohibited. This scope is a lifetime/count guard, *not* a lease owner or an independent thread.

**Public methods:** constructor binds the existing Threading wake owner and initializes two words. `IsShutdownRequested()` and `ActiveAdmissions()` observe the atomic words, but cannot prove all tasks joined. `IsValidProducerSlot(slot)` permits only [0,TProducerCount), rejecting reserved drain slot. `TrackAdmission()` returns noncopyable active scope for a complete outer queue-plus-Memory transaction; intentionally nested with `AcquireUntil`'s own scope, preventing a quiescent gap across an outer transaction. `RequestShutdown()` publishes stop **before** best-effort waking all producer slots and returns wake health. `AcquireUntil<TMaximumWakeRetries,TObjectPool,TLease,TArguments...>` uses the existing Threading wake owner, one canonical absolute deadline, Memory **NoWait** claims and a stop predicate; a grant racing shutdown is returned **with lease still live and owned**, not discarded. `PublishCapacityAvailable()` forwards advisory producer wake fanout; `ReleaseAndNotify<TLease>(lease)` forwards actual Memory lease return and notification during shutdown. `ShutdownUntil<TMaximumDrainWakes>(deadline)` is a **single-caller, non-reentrant** event-driven bounded drain: requests stop, checks active operations, then waits on the one reserved Threading slot for at most the compile-time wake budget and exact original Clock deadline. TimedOut, provider failure and retry-limit results remain separate from Quiescent.

**Safety limits:** a Quiescent result concerns **in-flight acquisition invocations only**, not already granted leases, Task joins or durable sink completions; for tracked lease lifetime use [TrackedAdmissionCoordinator](Reference-logging-TrackedAdmissionCoordinator). All external provider and coordinator objects must outlive every active scope, wake and lease release. Native direct Memory finite/Forever blocked acquisition still has a separately unqualified failed-mutex-lifetime path in [Memory #16](https://github.com/ESPressio-Development-Platform/EDP-Memory/issues/16), which this route avoids via NoWait; no ISR or indefinite latency/wear guarantee follows. GCC/Clang/UBSan and bounded real Worker integration/strict-NVS-ACK evidence are distinguished in [G1–G15](../docs/ACCEPTANCE-G1-G15.MD).
