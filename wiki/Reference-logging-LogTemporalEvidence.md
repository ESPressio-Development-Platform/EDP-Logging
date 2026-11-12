# Reference — `src/logging/LogTemporalEvidence.hpp`

[Source](../src/logging/LogTemporalEvidence.hpp) · [Coherent temporal evidence contract](../docs/COHERENT-TEMPORAL-EVIDENCE.MD) · [G1–G15 evidence](../docs/ACCEPTANCE-G1-G15.MD)

## Public composition API — `CoherentLogClockSample`

This value aggregate transfers one observation from the Clock/coordinator selected by application Bootstrap. The provider must capture all fields from the same committed epoch. The aggregate owns its field values and retains no provider references. Its first four fields are required for the existing aggregate-initialization form. The final two have conservative defaults.

| Member | Purpose |
| --- | --- |
| `Monotonic` | Canonical local monotonic observation carried into the record regardless of synchronized availability. |
| `Synchronized` | Clock owner's coordinate, uncertainty and synchronization state for the same observation. |
| `Era` | Owner-issued coordinate era. It must be valid for a present coordinate and invalid when the coordinate is absent. |
| `HasSynchronizedCoordinate` | Explicit presence asserted by the owner, including for a coordinate whose value is zero. |
| `ProjectionStatus` | Owner `ClockCorrelationProjectionStatus`, defaulting to `CorrelationUnavailable`. |
| `Assessment` | Owner `TemporalQualityAssessment`, defaulting to `Unreliable`. |

## Public result API — `LogTemporalEvidenceStatus` and `QualifiedLogTime`

`LogTemporalEvidenceStatus` is a `std::uint8_t` classification of the projection.

| Value | Meaning |
| --- | --- |
| `Synchronized` (0) | The Clock reports Synchronized and supplies a coordinate with a valid era. |
| `Degraded` (1) | The Clock supplies a coordinate with a valid era in Holdover or Reacquiring state. |
| `Unavailable` (2) | The coherent observation supplies local time without a synchronized coordinate. |
| `Incoherent` (3) | Presence, era, state or owner-issued metadata violates the transfer contract. |

`QualifiedLogTime` holds the by-value projection for source assembly. The temporal status governs interpretation of the coordinate.

| Member | Purpose |
| --- | --- |
| `Monotonic` | Unmodified canonical local time from the input. |
| `Synchronized` | The owner's coordinate when usable. Unavailable and incoherent results carry a default coordinate. |
| `Uncertainty` | The owner's uncertainty for a usable coordinate. Otherwise it carries the owner type's maximum uncertainty. |
| `Era` | The owner's valid era for a usable coordinate. Otherwise it is invalid. |
| `ClockState` | The observed owner synchronization state, including when projection fails. |
| `Status` | Logging's classification for deciding whether the coordinate is usable. |
| `ProjectionStatus` | The original owner correlation outcome for coherent observations. An incoherent observation yields `CorrelationUnavailable`. |
| `Assessment` | The original owner quality assessment for coherent observations. An incoherent observation yields `Unreliable`. |

`HasSynchronizedCoordinate() const noexcept` returns true for Synchronized and Degraded. A coordinate of zero is valid when the status reports presence.

## Public operations

`QualifyLogClockSample(const CoherentLogClockSample& sample) constexpr noexcept` borrows the input for the call and returns a new `QualifiedLogTime`. It checks the bounds of both owner enums and rejects a `Correlated` projection with no coordinate. A present coordinate requires a valid era and Synchronized, Holdover or Reacquiring state. An absent coordinate requires an invalid era and a state other than Synchronized. Unavailable or incoherent results preserve the exact monotonic observation and clear the synchronized coordinate and era. The operation is deterministic, allocation-free and read-only.

`CaptureLogTime<TCoherentClockProvider>(const TCoherentClockProvider& provider) noexcept` invokes the borrowed provider's `CaptureCoherent()` exactly once and qualifies the returned sample. `TCoherentClockProvider` selects the application composition and must expose `CaptureCoherent() const noexcept` returning `CoherentLogClockSample` by value. The provider owns epoch coherence, synchronization and any wait or ISR constraints. The adapter itself retains no state and acquires no locks. Physical timing and cross-node correlation require evidence from the selected Clock owner.

`tests/LogTemporalEvidenceTests.cpp` exercises deterministic value qualification, invalid combinations and one-call capture with a controlled provider. The named native and generated-record cases in the G1–G15 evidence record the integration and its scope.
