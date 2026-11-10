# Reference — `src/logging/BoundedAdmission.hpp`

**Classification:** PUBLIC OPTIONAL G1 admission helper and PRIVATE IMPLEMENTATION deadline-conversion helpers. [Exact source](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/8d2b307a8a1db1aa5596abc904cd0da1f3c032cd/src/logging/BoundedAdmission.hpp). This header is **opt-in** and imports abstract **EDP-Clock, EDP-Memory, EDP-Platform and EDP-Threading** directly. It is not part of the default lightweight Logging umbrella. No local task, queue, Mutex, Signal, heap or independent clock owner is created by this header.

## Public `AdmissionWaitResult` enum — exact result contract

- **Admitted (0):** successful Memory NoWait acquisition before the canonical deadline, output lease populated
- **GrantedAfterDeadline (1):** successful Memory claim raced past the deadline; output lease is **still owned by the caller** and must be released
- **DeadlineElapsed (2):** Clock deadline passed or Threading WaitUntil timed out, no new lease admitted
- **WakeRetryLimitReached (3):** the configured finite number of eligible wakes was exhausted with no successful capacity recheck
- **WaitProviderFailure (4):** Threading wait owner failed, not ordinary elapsed time
- **MemoryProviderFailure (5):** Memory acquisition returned a provider failure or unrecognized outcome
- **MemoryNotInitialized (6):** selected Memory topology has not initialized
- **TopologyUnavailable (7):** topology is cancelled, tearing down or otherwise unavailable
- **OutputLeaseOccupied (8):** caller's output already owns a live object; the helper does not overwrite or release it
- **InvalidProducerSlot (9):** reserved for the higher-level composed wake coordinator when its statically assigned producer slot is out of range; this helper does not manufacture a producer registration
- **ShutdownRequested (10):** the supplied stop predicate reported shutdown before a new successful grant
- **GrantedDuringShutdown (11):** stop was observed **after** a successful Memory grant, so the output lease remains populated and caller must release it during shutdown

These typed outcomes explicitly separate identity, ownership, provider fault, deadline, and orderly stop. A successful NoWait acquisition is never relabeled as empty merely because a deadline or shutdown followed it.

## Public `AcquirePoolUntilGuarded<TMaximumWakeRetries,TObjectPool,TWaitPoint,TLease,TStopPredicate,TArguments...>`

The template parameters select: a **strictly positive** compile-time maximum number of Threading wake/retry transitions; the application-composed Memory ObjectPool facade; the caller's exclusive lifetime-stable Threading wait point; the move-only caller-owned output lease; a noexcept zero-argument Boolean stop predicate; and the nothrow-constructible object constructor arguments. The compile-time assertions require the wait point's `WaitUntil(deadline)` to be noexcept and to return the canonical `Threading::BoundedWaitResult`. All parameters are borrowed or forwarded; this helper retains no state between calls.

`pool` is the Bootstrap-owned Memory facade, `waitPoint` the exclusive wake-before-wait-safe waiting surface, `lease` must initially be empty, `deadline` is one **absolute EDP-Clock monotonic** timestamp, `stopRequested` is a reference to the caller's active predicate, and `arguments...` are forwarded only through actual object construction. Before each Memory attempt it checks stop/deadline; **every** Memory acquisition is `WaitTimeout::NoWait()`. On capacity refusal it may wait via `waitPoint.WaitUntil(deadline)` and retry after a wake, up to `TMaximumWakeRetries`; the final wake still permits its last capacity probe. It does **not** invoke EDP-Memory's independently unqualified native finite/Forever blocked waiter under provider faults. After a successful claim it reevaluates shutdown and canonical time before returning, with the lease retained for either late outcome. The application must explicitly handle each possible result and release any late or shutdown grant.

The stop predicate is application-owned and can be published by a composed lifecycle coordinator. The helper itself supplies no queue entitlement, multi-producer registration, independent wake topology or cancellation epoch; the completed higher-level [record producer](Reference-logging-RecordProduction), [tracked admission](Reference-logging-TrackedAdmissionCoordinator) and [dedicated coordinator](Reference-logging-DedicatedCoordinator) supply separately tested pieces. Statements in older handoffs that *no* real higher-level worker/HIL existed are historical, **not** a description of today's separately implemented integration.

## Compatibility overload and internal helpers

`AcquirePoolUntil<TMaximumWakeRetries,TObjectPool,TWaitPoint,TLease,TArguments...>` has the previous signature and delegates to the guarded form with a local, noexcept `neverStop` predicate. It retains the same absolute deadline/no-wait semantics but cannot observe an application stop request except via the supplied wait point. The internal `Detail::RemainingRecordAdmissionWait(deadline)` converts an EDP-Clock absolute deadline into one nonnegative native relative provider budget, saturating elapsed deadlines to `NoWait` and never constructing a second Clock owner. The internal `Detail::RecordQueueDeadlineAfter(nanoseconds)` creates a separate consumer-queue-transition deadline from the canonical current monotonic value, saturating positive distance on overflow; it **does not restart or extend a producer's earlier admission deadline**. Neither helper is a general Clock substitute or an allocation.

## Failure, resource and proof boundaries

No new memory allocation, background scheduling, platform mutex, ISR surface or implicit writer is introduced. The caller must own a lifetime-stable wait point and ensure every producer of relevant capacity publishes advisory wakes, including after releases and conflicting claims that could conceal availability. This header is one **component** of bounded G1; it does not by itself prove queue fairness, all physical provider timeout resolutions, whole-sink durability or interruption atomicity. Direct Memory finite/Forever stack waiter safety remains unresolved in [Memory #16](https://github.com/ESPressio-Development-Platform/EDP-Memory/issues/16), but this helper never calls that path. The real GCC/Clang host suites and later named native worker/100-rps and external relay PowerLoss profiles are distinguished in [G1–G15 evidence](../docs/ACCEPTANCE-G1-G15.MD); no generic indefinite performance/wear guarantee follows.
