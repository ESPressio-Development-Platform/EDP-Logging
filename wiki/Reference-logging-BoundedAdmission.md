# logging/BoundedAdmission.hpp — optional G1 admission bridge

This is a **partial**, non-production application-composed bridge and **not** a complete finite Logging producer. Package version remains `0.1.0`. The header is opt-in, not imported through the default Logging umbrella.

## Namespace and reusable declarations

- `ESPressio::Logging::AdmissionWaitResult`: explicit strong `std::uint8_t` enum: `Admitted=0`, `GrantedAfterDeadline=1`, `DeadlineElapsed=2`, `WakeRetryLimitReached=3`, `WaitProviderFailure=4`, `MemoryProviderFailure=5`, `MemoryNotInitialized=6`, `TopologyUnavailable=7`, `OutputLeaseOccupied=8`, `InvalidProducerSlot=9` (returned only by the optional composed wake coordinator when the caller supplies an out-of-range statically configured producer slot).
- `template<std::size_t TMaximumWakeRetries, TObjectPool, TWaitPoint, TLease, ...TArguments> AcquirePoolUntil(...)`: public noexcept function returning the result. The positive compile-time wake/retry bound is chosen by the application. It delegates the actual object claim to Memory `Acquire(...,NoWait())` and original canonical deadline wait to Threading `WaitUntil(deadline)`, using EDP-Clock `MonotonicNow()` for pre/post-claim checks.

No runtime state is retained by this helper; all provider state and ObjectPool backing remain owned by Bootstrap's selected Memory/Threading/Platform/Clock implementations. No new C++ heap, blocking native Mutex, internal task, signal or clock is introduced. The wait point belongs exclusively to the caller for the whole invocation.

## Required life cycle / invariants

Bootstrap binds one monotonic clock before any concurrent caller. A producer's Threading wait point must be exclusive, lifetime-stable and based on a wake-before-wait-safe Platform Signal. The **future** Logging coordinator must wake the producer after each pool release and after conflicting claims that may mask free capacity. A returned `GrantedAfterDeadline` means the lease **is populated** and the caller must release explicitly; the helper does not secretly block to return it. Other failure outcomes do not populate a previously empty lease, assuming the Memory provider obeys its published contract.

## Outstanding G1 proof obligations

There is no FIFO or pooled-queue slot entitlement, no multi-producer wake registration, no fair capacity recheck, no ownership-safe shutdown/cancellation, no actual ESP32 build, no physical deadline proof, and no completed end-to-end Logging runtime. EDP-Memory's original finite `ObjectPool::Acquire` still contains indefinite mutex entrance/relock and caller-stack waiters; this helper deliberately **does not call it**.

The RPI400 optional C++20 host suite (GCC and Clang) passed in detached run `20261016T071414-d6ec50d8`. See [bounded-admission docs](../docs/BOUNDED-ADMISSION.MD), [Architecture](../docs/ARCHITECTURE.MD), and [Dependency Graph](../DEPENDENCY_GRAPH.MD). The tests use fake pool/wait-point instances with real public owner contracts; they are not evidence of full EDP-Memory integration or hardware validation.
