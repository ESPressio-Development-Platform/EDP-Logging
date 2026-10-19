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
