# Optional G1 bounded-admission foundation (partial)

**Source:** `src/logging/BoundedAdmission.hpp` on `feature/logging-v1`.
**Status:** isolated, **NOT** the production Logging producer, sequencer, FIFO or end-to-end G1 proof.
**Package version:** `0.1.0` (unchanged).

The free `ESPressio::Logging::AcquirePoolUntil<TMaximumWakeRetries>` algorithm accepts an application-composed EDP-Memory ObjectPool, one caller-exclusive application-composed EDP-Threading bounded wait point, an initially empty ObjectPoolLease, and **one absolute EDP-Clock MonotonicTimestamp** deadline. It calls `Clock::MonotonicNow()` before each attempt and after a successful claim, invokes **only** `Memory::ObjectPool::Acquire(..., Platform::Synchronization::WaitTimeout::NoWait(), ...)`, then delegates unavailability waits to the already existing `Threading::BoundedWaitPoint::WaitUntil(deadline)`. It never calls the known-unsafe EDP-Memory finite acquisition path and does not create a second timeline, stack-linked waiter, private semaphore, polling sleep or extra dynamic allocation.

The caller selects a positive compile-time maximum wake count. Even a permanently spurious-waking backend therefore cannot cause an unlimited inner loop; the result is `WakeRetryLimitReached` rather than false success. The interface preserves distinct `MemoryNotInitialized`, `TopologyUnavailable`, `OutputLeaseOccupied`, `WaitProviderFailure` and `MemoryProviderFailure` outcomes. **If native NoWait grants the lease but a subsequent monotonic observation is at/after the deadline, the result is `GrantedAfterDeadline`; the lease deliberately remains occupied and the caller must explicitly release it.** It must never silently turn that ownership event into `DeadlineElapsed` or try an unbounded hidden release.

## Mandatory composition and wake responsibilities

Application Bootstrap must bind the one canonical EDP-Clock monotonic provider before any concurrent code. The caller owns a stable, **exclusive** bounded wait point for its producer; the underlying EDP-Platform Signal must latch wake-before-wait notifications, as required by the Threading owner. The eventual producer/coordinator must wake the appropriate producer(s) following **every capacity release**, and following any conflicting pool admission that may transiently mask remaining capacity due to native NoWait mutex contention. A real multi-producer implementation must prove wake placement, fairness, slot lifetime, shutdown, release failure propagation, reservation linearisation and absence of hidden indefinite waits. This foundation **does not register producers or provide these behaviours automatically**.

The currently introduced helper is opt-in only; no umbrella `ESPressio_Logging.hpp` include imports it. Only builds explicitly including `<logging/BoundedAdmission.hpp>` activate direct dependencies on the EDP-Clock, EDP-Memory, EDP-Threading and EDP-Platform abstractions. Concrete platforms remain selected via application Composition. The static wake retry budget is an application-specific liveness safeguard, **not** a time substitute and never refreshes the canonical deadline.

**Non-goals:** It does not fix `EDP-Memory` finite Acquire, handle durable archive/Serial sinks, supply a LogRecord TypeIdentifier, or establish real ESP32 deadline/ISR/Flash evidence. The Memory owner G1 stack-waiter hazard remains open.

## Host validation

With six matching EDP sibling checkouts, run:

```sh
EDP_LOGGING_DEPENDENCY_ROOT=/path/to/paired-repositories CXX=g++ bash tests/run-bounded-admission-tests.sh
EDP_LOGGING_DEPENDENCY_ROOT=/path/to/paired-repositories CXX=clang++ bash tests/run-bounded-admission-tests.sh
```

The optional suite uses fake bounded providers with the actual EDP-Clock, EDP-Threading, EDP-Memory and EDP-Platform public result/timeout contracts. It tests immediate success, contention wake/retry, original deadline forwarding, zero-wait only, expired deadline, wait/native failure, explicit late lease ownership and a deterministic finite spurious-wake limit. It does **not** substitute for a real concurrent MemoryTopology provider-integrated test.

On RPI400 (2026-10-16), GCC and Clang completed successfully in durable detached run `20261016T071414-d6ec50d8`, against pinned dependencies in `/srv/espressio/scratch/gpt6-logging-downstream-20261016/manifest-sha.txt`.
