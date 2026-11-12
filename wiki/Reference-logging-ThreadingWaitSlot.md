# ThreadingWaitSlot — implementation reference

**Classification:** Private Threading adapter. **Owning header:** [src/logging/detail/ThreadingWaitSlot.hpp](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/detail/ThreadingWaitSlot.hpp). **Version:** 0.1.0, source branch feature/logging-v1. This page describes implementation and owner requirements; it does not replace normative ./docs contracts.

## Responsibility and boundaries

Wraps a single existing EDP-Threading finite wait/wake slot around the owning monotonic Clock deadline; it is an implementation seam rather than a new event, Clock or mutex implementation.

## Resource, failure and lifecycle invariants

The lifecycle of the selected wake provider, fixed slot ownership and source deadline must outlive the waiting call. No dynamic waiter registry or independent timeout timeline is introduced.

## Source declarations and responsibilities

The following source-indexed inventory distinguishes defined types/contracts from operations. Descriptions reflect implementation documentation at the cited line; callers must also respect the preconditions, result meanings, provider choices and lifetimes described above.

- **TWaitWakeProvider** (Type or Concept, [line 16](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/detail/ThreadingWaitSlot.hpp#L16)): Stateless view of one Bootstrap-provisioned EDP-Threading wait/wake slot.  Synchronization storage and canonical monotonic waiting remain entirely owned by the supplied EDP-Threading provider. Logging retains only the caller's exclusive slot identity; no second Signal, waiter pool, or independent clock mechan
- **ThreadingWaitSlot** (Type or Concept, [line 17](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/detail/ThreadingWaitSlot.hpp#L17)): Stateless view of one Bootstrap-provisioned EDP-Threading wait/wake slot.  Synchronization storage and canonical monotonic waiting remain entirely owned by the supplied EDP-Threading provider. Logging retains only the caller's exclusive slot identity; no second Signal, waiter pool, or independent clock mechan
- **WaitUntil** (Method or Function, [line 21](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/detail/ThreadingWaitSlot.hpp#L21)): Defines a named operation within ThreadingWaitSlot; read its source for exact conditional semantics and provider requirements.

## Dependency and test qualification

The implementation owns no replacement EDP-Platform/Memory/Threading/Persistence abstraction. The actual include closure is determined by its opt-in header and the consuming application; the default Logging umbrella remains deliberately narrow. See the current [Dependency Graph](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/DEPENDENCY_GRAPH.MD), [Architecture](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ARCHITECTURE.MD), and [G1–G15 acceptance matrix](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ACCEPTANCE-G1-G15.MD) for exact validation and limitation claims. Never translate an unrelated historical fixture into current production proof.

## 12 November 2026 — complete internal template, data fields and method results

[Exact C++ source](../src/logging/detail/ThreadingWaitSlot.hpp). **INTERNAL (Detail)** `ThreadingWaitSlot<TWaitWakeProvider>` is a short stack-lifetime **borrowed view**, not a new synchronization provider or heap-owned handle. **Template parameter `TWaitWakeProvider`** names the existing app-selected EDP-Threading `BoundedWaitWake` provider Type. Public data member **`WaitWake : TWaitWakeProvider*`** is a nonowning stable pointer and **`Slot : size_t`** is that caller's exclusive, statically selected wake identity. Neither must escape the enclosing admission operation; the source's fixed slot fan-out and provider lifetime remain exclusively owned by Bootstrap.

**`WaitUntil(ESPressio::Clock::MonotonicTimestamp deadline) noexcept -> ESPressio::Threading::BoundedWaitResult`** passes the **same absolute canonical Clock deadline** and Slot into `WaitWake->WaitUntil(Slot,deadline)` once. It maps exact EDP-Threading `BoundedWaitWakeResult::Woken` to `BoundedWaitResult::Woken`, `TimedOut` to `TimedOut`, and every provider failure/unissued result to `ProviderFailure`. It does not rebase/extend the deadline, issue another `MonotonicNow()`, publish a wake, retain a producer pointer or add an allocation, Mutex/Signal, Task, ISR entry point or hidden retry. It returns a typed owner result; **TimedOut** is not a false native provider failure, and a **Woken** result is merely advisory, requiring the caller to retry Memory NoWait capacity checks and re-evaluate its original deadline. A caller must not keep a ThreadingWaitSlot alive after its borrowed provider or exclusive slot registration ends.
