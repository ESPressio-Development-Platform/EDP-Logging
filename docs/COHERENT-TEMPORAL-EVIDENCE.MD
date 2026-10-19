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


## 2026-10-19 — G2 EDP-Clock owner-status provenance propagation

The application-composed **single-epoch** `CoherentLogClockSample` has
two optional additional owner-issued facts, defaulting conservatively
for legacy four-member initializers:
`ClockCorrelationProjectionStatus::CorrelationUnavailable` and
`TemporalQualityAssessment::Unreliable`. Logging never makes up
different enum values, and does not infer `Reliable` from a
synchronized timestamp alone.

`QualifyLogClockSample` checks both owner enum domains and rejects
contradictory `Correlated`-without-coordinate evidence. Invalid
samples retain canonical monotonic evidence but are Incoherent and
cannot claim an Era, synchronized coordinate or reliable quality.
Valid owner values survive `QualifiedLogTime` by value and
`AssembleLogRecordSource` copies them to the approved Field 11 pair
of the 15-field structural record. The source provider is still
responsible for genuine *same coordinator epoch* capture; separately
reading time, Era and policy status is NOT a coherent provider.

GCC+Clang host run `20261019T130909-8d20664a` **PASS exit0**:
real Localisation-generated contract source assembly, preservation
of `NotRequired`, owner `Correlated`/`Reliable` pair and fail-closed
unknown statuses. Initial 13:08 compile failure was a test-only
misnamed `LogTemporalProvenance::Assessment`; real field name
`Quality`; test corrected before successful run.

**Physical testing**: new `g2_clock_owner_check.hpp` runs injected
owner facts on an ESP32-D0WD/WROOM within existing governed LogRecord
validation, preserving all 18 prior HIL markers. Its first firmware
built and passed flash verification (build
`20261019T141615-edbf3fc3` / flash
`20261019T142218-5b8937d4`) but the first physical UART
run `20261019T142256-5e601233` **FAIL exit18**,
repeating ESP-IDF `InstrFetchProhibited` NULL fetch and 4/18
markers. The exact image backtrace resolved to
`g2_clock_owner_check.hpp:11` / `Clock::MonotonicNow` invoked
**before Clock::BindMonotonicClock** (the later full HIL Run() does
the bootstrap). This was the *test's startup ordering defect*; no
G2 qualification claimed for that image.

Corrected early synthetic test uses an explicitly injected
`MonotonicTimestamp::FromNanoseconds` without accessing an unbound
global Clock, preventing fake live-clock quality claims.
New corrected PioArduino/ESP-IDF build
`20261019T142627-5ed6a666` **PASS exit0**; factory image SHA256
`ec074528b319a3ad6650545a9e14bbc5ef994c2eb5ade33b88dc678278e7d6f5`.
Actual ESP32 0x0 write plus independent on-chip flash verification
`20261019T142728-f73d9168` **PASS exit0**.
Three independent real UART/NVS-restart cycles
`20261019T142822-974150f9` **PASS exit0**, all 18/18 functional
markers, EDLG framing and CRC, resource/G11 probes and now G2
source-quality check. Private capture evidence manifest SHA256
`3c8187e82aa82c5c5246cf0edaf0493091fdec7ff9d50f18d7ecd8512289d8d0`
at `/srv/espressio/scratch/gpt6-logging-20261019-g11/g2-clock-owner-corrected-captures`.

**Limit:** Injected status values prove owner-vocabulary retention and
rejection in real compiled firmware, **not** that the WROOM has actual
multi-node synchronized Clock consensus or that a production provider
samples all data from a single clock revision. Complete Typed
Envelope, historical audit and remaining G1–G15 release gates remain
open.
