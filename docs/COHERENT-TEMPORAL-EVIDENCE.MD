# G2 temporal sample acceptance (partial coherent capture seam)

**Scope:** optional `logging/LogTemporalEvidence.hpp`, feature `feature/logging-v1`, version `0.1.0`. This does **not** implement the missing Clock/coordinator-side coherent snapshot publisher, and does not close G2.

The Logging-specific `CoherentLogClockSample` is a small immutable-by-value transfer of existing **EDP-Clock strong types**: `MonotonicTimestamp`, `SynchronizedReading` (coordinate, uncertainty, state), `ClockEra` and an explicit `HasSynchronizedCoordinate` bool. The producer is required to capture all of these against **one atomic/seqlock/otherwise suitably synchronized Clock/coordinator revision**, rather than doing independent `MonotonicNow()`, `SynchronizedNow()` and `Era` reads which may straddle an era transition. Logging's source does not own synchronization/eras and may NOT invent either.

`CaptureLogTime<TCoherentProvider>` statically requires the Bootstrap-composed provider's **exact** `CoherentLogClockSample CaptureCoherent() const noexcept` return-by-value contract, invokes the provider **exactly once**, and delegates qualification to `QualifyLogClockSample`. The latter is a constexpr pure value transformation with no new clock, heap, mutex or speculative UTC conversion. It retains the canonical captured monotonic coordinate unconditionally.

`LogTemporalEvidenceStatus` explicitly identifies:
- `Synchronized`: source state synchronized, coordinated nonzero Era and published coordinate
- `Degraded`: holdover or reacquiring, valid coordinated Era and coordinate, but lower confidence
- `Unavailable`: no coordinate and invalid Era, and source does not claim Synchronized
- `Incoherent`: missing Era for a claimed coordinate, claimed synchronized quality without a coordinate, LostSynchronization/NeverSynchronized falsely paired with a published coordinate, or similarly contradictory facts

The output `QualifiedLogTime` preserves `Monotonic` and native `ClockState`, and carries synchronized timestamp, uncertainty and era only when the predicate actually establishes evidence. **Timestamp zero is valid**; code must consult `HasSynchronizedCoordinate()` / status, never use zero as availability sentinel. In unavailable/incoherent cases, synchronized coordinate is suppressed, Era reset to invalid, and uncertainty becomes `SynchronizationUncertainty::Maximum()`.

**Owner-level prerequisite still open:** current EDP-Clock SynchronizedClockProvider::Now() yields synchronized coordinate/state/uncertainty coherently but does not atomically incorporate the application/upstream-coordinator-owned ClockEra and monotonic capture. The fabricated *test* provider honours the interface, but that does not demonstrate a production provider. A subsequent component-owner implementation and on-device/cross-era race test must prove this capture seam is really coherent and Bootstrap can compose it without a Mesh dependency in EDP-Logging. Until then, the Logging runtime must not claim synchronized LogRecords are coherent or UTC.

C++20 compile/run tests `tests/LogTemporalEvidenceTests.cpp` exercise successful, zero-coordinate, Holdover, Reacquiring, unavailable, LostSynchronization, missing Era, and mismatched state/coordinate; ensure a provider is called exactly once. RPI400 GCC+Clang host suite passed in `20261016T073929-8ef41bf6`, not physical validation.


## 2026-10-16 clarification — G2-A vs optional upstream G2-B

**G2-A, a Logging/Clock concern:** Bootstrap supplies a Clock-coherent
capture provider; a monotonic-only sample with absent synchronized coordinate,
invalid Era and NeverSynchronized Clock state is fully valid. Thus standalone
Logging source/queue/sink processing is not blocked on Mesh reference election.

**G2-B, application integration concern:** If a mesh-enabled application
selects synchronized Mesh-coordinated time, the owner must publish timestamp,
uncertainty, reference era and root continuity coherently across transitions.
This optional higher-level Clock/Mesh certification may still be open; it
must not be represented as an EDP-Logging compile-time, link-time or
run-time dependency. The Logging provider receives only the abstract
Clock strong types and does not import Mesh.
