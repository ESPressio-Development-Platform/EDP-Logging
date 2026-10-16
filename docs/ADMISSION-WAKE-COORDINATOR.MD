# G1: application-composed fixed-slot admission wake coordinator

**Scope:** `src/logging/AdmissionWakeCoordinator.hpp` on `feature/logging-v1`, in the already optional `logging/BoundedAdmission.hpp` dependency profile. **Version remains 0.1.0.** This is a qualification milestone, **not** the complete Logging producer/FIFO or full G1 acceptance.

## Ownership and bounded behaviour

`AdmissionWakeCoordinator<TProducerCount,TWaitWakeProvider>` borrows one Bootstrap-supplied `EDP-Threading::BoundedWaitWakeProvider` (or equivalent capability), with a compile-time capacity at least `TProducerCount`. Producer slots are *statically* assigned, **unique** and never dynamically registered or freed while tasks run. No coordinator-managed stack-linked waiter, heap allocation, runtime registry, application mutex or concrete SDK is introduced. The owner must outlive all borrowers.

`AcquireUntil<MaximumWakeRetries>(producerSlot, memoryPool, emptyLease, canonicalMonotonicDeadline, args...)` delegates to the previously tested `AcquirePoolUntil` seam. It uses ONLY EDP-Memory `NoWait`, and performs each wait on the assigned EDP-Threading slot via `WaitUntil(slot, same deadline)`. When a capacity claim succeeds, it *also* publishes bounded notifications to all slots because temporarily contended Memory mutex acquisition may have been reported as `CapacityUnavailable`, and another actual slot may still be available. It never assumes `CapacityUnavailable` means the pool is full.

`ReleaseAndNotify(lease)` releases through EDP-Memory's actual `ObjectPoolLease::Release()`. On `Released`, it invokes `PublishCapacityAvailable()` for **every** configured producer slot. Each wake is attempted even if another slot's notification fails. A Memory release failure never claims capacity was made available. `CoordinatedReleaseResult` distinguishes `ReleasedAndNotified`, `ReleasedNotificationFailed`, `AlreadyEmpty` and `MemoryProviderFailure`. `CoordinatedAdmissionResult` preserves both the original admitted/late/deadline/error status and independent notification-publication health. In particular, a late grant **still owns a live lease** until explicitly returned.

**Strong prerequisites:**

- Bootstrap binds the canonical EDP-Clock monotonic provider before producers start; no separate clock is used.
- Every slot is held exclusively by one producer; concurrent waits **on the same slot are prohibited**.
- Platform Signal used in Threading is **latched** (wake-before-wait safe), as proven for the host provider. Other concrete target providers still need checking.
- **All paths that release a capacity claim must call `ReleaseAndNotify`**. Calling `lease.Release()` or letting its RAII destructor release the lease without notifying bypasses this seam and may starve waiters. Composed lifecycle/lease ownership is still a prerequisite, not an already solved property.
- Capacity released by *other* pool consumers must be signalled explicitly via `PublishCapacityAvailable()`.
- A momentarily contended native Memory NoWait mutex may produce a false `CapacityUnavailable`; absent other wake events, a finite deadline may expire with real capacity available. This is a **remaining G1 limitation**, not a hidden proof of FIFO or wait-free progress.
- Signal `Wake`, Memory `Release` and provider-related error durations are not presently proved to be bounded by one canonical deadline under every concrete platform implementation. A failed wake requires caller escalation.

## Tests and known acceptance gaps

`tests/AdmissionWakeCoordinatorTests.cpp` imports **the actual EDP-Memory owner host test fixture** from the pinned sibling `EDP-Memory/tests/host/main.cpp`, including its real MemoryTopology, shared/dedicated ObjectPool, valid Platform Mutex and latched Platform Signal providers. This avoids duplicating the Memory owner implementation. Tests use two real concurrent producers, full pool, release/wake fanout, 12 repeated contention rounds, canonical steady monotonic deadline, explicit late/expiry/empty handling, notification-provider failure and native Memory mutex temporary contention. Host fixture main is macro-renamed only inside the test TU. Strict GCC/Clang C++20 host builds are described in `tests/run-composed-admission-tests.sh` and require the six exact checked-out sibling owners. This is **not** a physical-device or firmware test.

**Still blocking G1 closure:** safe registration and lifecycle ownership of producer slots/leasing, release from all ownership paths including RAII, fairness under high contention, strict end-to-end deadline covering release and all platform operations, fully composed application Queue/BoundedTopology, cancellation/shutdown, and actual ESP32 HIL timing. EDP-Memory's direct finite acquisition stack-waiter lifetime hazard remains unmodified. No Clock/Mesh/Memory/Threading source change is introduced by this commit.
