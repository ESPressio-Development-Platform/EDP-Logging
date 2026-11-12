# logging/LogTemporalEvidence.hpp — G2 temporal qualification contract

**Status:** optional, partial G2 transfer/qualification. Package `0.1.0`. **No real production coherent Clock/coordinator provider has been installed by this Logging-only change.** See [detailed G2 limitations](../docs/COHERENT-TEMPORAL-EVIDENCE.MD).

## Types and API

- `CoherentLogClockSample`: aggregate made entirely from `EDP-Clock` strong `MonotonicTimestamp`, `SynchronizedReading`, `ClockEra`, plus `bool HasSynchronizedCoordinate`. This is a transfer *contract*, not a Clock publisher. Bootstrap must supply the native coherence guarantee across Era and coordinated Clock data.
- `LogTemporalEvidenceStatus : std::uint8_t`: `Synchronized=0`, `Degraded=1`, `Unavailable=2`, `Incoherent=3`. Explicit degradation and impossible combinations never create UTC.
- `QualifiedLogTime`: retained local `Monotonic`, qualified `Synchronized`, `Uncertainty`, `Era`, original `ClockState`, strong `Status`, `HasSynchronizedCoordinate() const noexcept`; timestamp **zero is valid**, not an availability marker.
- `QualifyLogClockSample(const CoherentLogClockSample&) constexpr noexcept`: pure allocation-free value validation; suppresses unavailable/incoherent coordinated values, retains local monotonic.
- `CaptureLogTime<TCoherentClockProvider>(const provider&) noexcept`: compile-time exact type and noexcept checks, invokes `CaptureCoherent()` **once**, never independently samples `MonotonicNow`/`SynchronizedNow`/era.

## Tests and prerequisites

`tests/LogTemporalEvidenceTests.cpp` and `tests/run-log-temporal-tests.sh` require direct EDP-Clock/EDP-System/EDP-Platform *abstractions*, using only fake coherent sources; strict C++20 GCC+Clang passed on RPI400 in detached run `20261016T073929-8ef41bf6`. The **production** Clock/coordinator revision-atomic capture owner, era provenance, source transitions, ISR constraints and physical/target tests remain outstanding. EDP-Logging is forbidden from selecting a Mesh concrete or deriving its own Era.


## G2 2026-10-19 — owner-issued Field 11 values through one Clock sample

Optional coherent Clock source captures
`ClockCorrelationProjectionStatus` and
`TemporalQualityAssessment` from EDP-Clock itself; legacy sources
default unavailable/unreliable. Qualifier checks exact owner numeric
vocabulary, rejects correlated-without-coordinate and preserves
monotonic evidence alone on incoherence. Source assembly copies the
pair into structural LogRecord Field11 without invented Logging
status values. GCC/Clang generated-contract tests PASS and corrected
real ESP32 WROOM physical flash and 3/3 runs PASS.
The first physical test panicked by reading unbound Clock before
bootstrap; root cause fixed and included in HIL/Policy handoff.
Application Bootstrap must still supply a **genuine one-epoch** provider:
injected test facts are not synchronization proof.

## 12 November 2026 — current G2 source semantics and owner-based qualification (superseding early partial wording)

The early **partial G2** status at the top documents the original state *before* EDP-Clock's current main owner and the later generated-record integrations were independently qualified. **Current implementation:** [`src/logging/LogTemporalEvidence.hpp`](../src/logging/LogTemporalEvidence.hpp) is a complete, pure value qualification and **one-capture** adapter; EDP-Logging intentionally does **not** own or implement the underlying native clock/coordinator publisher. Absence of a Logging-owned Clock is therefore **not** a missing Logging feature. The application must still select a native EDP-Clock coordinator whose epoch/era and quality statements are genuinely coherent, rather than using a synthetic test provider as physical timing evidence.

**`CoherentLogClockSample`** has six source fields: Monotonic (canonical local), Synchronized (owner SynchronizedReading), Era (owner-issued ClockEra), HasSynchronizedCoordinate (Boolean presence independent of timestamp zero), **ProjectionStatus** (owner ClockCorrelationProjectionStatus, default CorrelationUnavailable) and **Assessment** (owner TemporalQualityAssessment, default Unreliable). These latter two source fields are present on the current branch even though early four-element call sites default to unavailable/unreliable. The sample must come from one atomic owner epoch, not three successive independent Clock calls.

**`QualifiedLogTime`** carries Monotonic, Synchronized, Uncertainty, Era, ClockState, Status, owner ProjectionStatus and owner Assessment; `HasSynchronizedCoordinate()` is true only for Synchronized or Degraded qualified status, not for an arbitrary zero/nonzero numeric coordinate. **`LogTemporalEvidenceStatus`** has Synchronized (0), Degraded (1), Unavailable (2), Incoherent (3). A synchronized coordinate is valid only in Synchronized/Holdover/Reacquiring state and with a **valid nonzero owner Era**. The pure constexpr **`QualifyLogClockSample(sample)`** checks exact owner-issued enum bounds, refuses a Correlated projection without a coordinate, and returns Incoherent when state/era/presence contradict; unavailable paths retain the exact local Monotonic but reset synchronized coordinate, uncertainty and era to conservative unavailability. It must **never synthesize UTC**, an issued Era, or a more accurate measurement.

**`CaptureLogTime<TCoherentClockProvider>(provider)`** requires a borrowed existing Bootstrap-composed provider with `noexcept CaptureCoherent() -> CoherentLogClockSample` *exactly* and invokes it **once**, passing the result to QualifyLogClockSample. It creates no new task, clock, sampler, heap or mutex, never extends a source timestamp lifetime, and is not independently ISR-qualified. Earlier native WROOM unit/owner and bounded source integrations have genuine named host/physical receipts; no claim of **universal cross-node Mesh correlation** follows. See [G1–G15](../docs/ACCEPTANCE-G1-G15.MD) and [coherent temporal owner](../docs/COHERENT-TEMPORAL-EVIDENCE.MD).
