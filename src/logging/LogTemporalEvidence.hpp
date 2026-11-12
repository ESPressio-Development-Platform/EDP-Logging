#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

#include <ESPressio_Clock.hpp>

namespace ESPressio::Logging {

    /// A conservative Logging classification of one immutable temporal sample.
    enum class LogTemporalEvidenceStatus : std::uint8_t {
        /// The Clock reports synchronized state with a usable coordinate.
        Synchronized = 0U,

        /// A usable coordinate from Clock Holdover or Reacquiring state.
        Degraded = 1U,

        /// The coherent sample supplies local monotonic evidence alone.
        Unavailable = 2U,

        /// The supplied presence, state, era or owner metadata contradicts the contract.
        Incoherent = 3U
    };

    /// Owner facts captured together from one committed Clock/coordinator epoch.
    struct CoherentLogClockSample final {

        // Coherent owner observations.

        /// Canonical local monotonic observation.
        ESPressio::Clock::MonotonicTimestamp Monotonic;

        /// Owner reading that carries synchronized coordinate, state and uncertainty.
        ESPressio::Clock::SynchronizedReading Synchronized;

        /// Owner-issued era associated with the coordinate when one is present.
        ESPressio::Clock::ClockEra Era;

        /// Explicit coordinate presence independently of the numerical value.
        bool HasSynchronizedCoordinate;

        /// Owner correlation outcome, defaulting to unavailable if omitted.
        ESPressio::Clock::ClockCorrelationProjectionStatus ProjectionStatus{
            ESPressio::Clock::ClockCorrelationProjectionStatus::CorrelationUnavailable
        };

        /// Owner assessment of temporal quality, defaulting to unreliable if omitted.
        ESPressio::Clock::TemporalQualityAssessment Assessment{
            ESPressio::Clock::TemporalQualityAssessment::Unreliable
        };

    };

    /// Logging's immutable projection of a single coherent Clock sample.
    struct QualifiedLogTime final {

        // Projected time and its owner provenance.

        /// Exact local monotonic observation from the input sample.
        ESPressio::Clock::MonotonicTimestamp Monotonic;

        /// Qualified synchronized coordinate whose presence is described by Status.
        ESPressio::Clock::SynchronizedTimestamp Synchronized;

        /// Owner uncertainty for a usable coordinate, otherwise maximum uncertainty.
        ESPressio::Clock::SynchronizationUncertainty Uncertainty;

        /// Owner-issued era for a usable coordinate, otherwise invalid.
        ESPressio::Clock::ClockEra Era;

        /// Original owner synchronization state for diagnostic use.
        ESPressio::Clock::SynchronizationState ClockState;

        /// Logging classification used to interpret the projected coordinate.
        LogTemporalEvidenceStatus Status;

        /// Owner projection outcome, reset to unavailable for an incoherent sample.
        ESPressio::Clock::ClockCorrelationProjectionStatus ProjectionStatus{
            ESPressio::Clock::ClockCorrelationProjectionStatus::CorrelationUnavailable
        };

        /// Owner quality assessment, reset to unreliable for an incoherent sample.
        ESPressio::Clock::TemporalQualityAssessment Assessment{
            ESPressio::Clock::TemporalQualityAssessment::Unreliable
        };

        // Projected coordinate presence.

        /// Reports a usable synchronized or degraded coordinate, including zero.
        [[nodiscard]] constexpr bool HasSynchronizedCoordinate() const noexcept {
            return Status == LogTemporalEvidenceStatus::Synchronized ||
                   Status == LogTemporalEvidenceStatus::Degraded;
        }

    };

    /// Qualifies a *single already-coherent* Clock/coordinator sample.
    ///
    /// Invalid/incoherent synchronized evidence never gets a fabricated Era.
    /// The one canonical monotonic observation is retained without modification.
    /// NeverSynchronized and LostSynchronization cannot claim a coordinate,
    /// and a coordinate requires a nonzero upstream-assigned ClockEra.
    ///
    /// This pure qualifier does not make a broken producer coherent. The
    /// Bootstrap-composed capture provider remains responsible for sampling a
    /// genuine single state/epoch, and must be separately validated.
    [[nodiscard]] constexpr QualifiedLogTime QualifyLogClockSample(
        const CoherentLogClockSample& sample
    ) noexcept {
        using namespace ESPressio::Clock;

        const auto state = sample.Synchronized.State();
        const auto hasCoordinate = sample.HasSynchronizedCoordinate;
        const auto eraValid = sample.Era.IsValid();
        const auto eligible =
            state == SynchronizationState::Synchronized ||
            state == SynchronizationState::Holdover ||
            state == SynchronizationState::Reacquiring;

        const auto ownerStatusValid =
            static_cast<std::uint8_t>(sample.ProjectionStatus) <=
                static_cast<std::uint8_t>(
                    ClockCorrelationProjectionStatus::SynchronizedCoordinateOverflow) &&
            static_cast<std::uint8_t>(sample.Assessment) <=
                static_cast<std::uint8_t>(TemporalQualityAssessment::Reliable);
        // A claimed correlated projection without any coordinate is false
        // evidence; do not silently convert it into a plausible timestamp.
        const auto contradictoryProjection =
            !hasCoordinate &&
            sample.ProjectionStatus == ClockCorrelationProjectionStatus::Correlated;
        const auto consistent = ownerStatusValid && !contradictoryProjection &&
            (hasCoordinate
                ? eraValid && eligible
                : !eraValid && state != SynchronizationState::Synchronized);

        const auto quality = !consistent
            ? LogTemporalEvidenceStatus::Incoherent
            : !hasCoordinate
                ? LogTemporalEvidenceStatus::Unavailable
                : state == SynchronizationState::Synchronized
                    ? LogTemporalEvidenceStatus::Synchronized
                    : LogTemporalEvidenceStatus::Degraded;

        // No derived timestamp is ever fabricated on an unreliable path.
        const auto usable = hasCoordinate && consistent;

        return {
            sample.Monotonic,
            usable ? sample.Synchronized.Timestamp() : SynchronizedTimestamp(),
            usable ? sample.Synchronized.Uncertainty() : SynchronizationUncertainty::Maximum(),
            usable ? sample.Era : ClockEra(),
            state,
            quality,
            consistent ? sample.ProjectionStatus :
                ClockCorrelationProjectionStatus::CorrelationUnavailable,
            consistent ? sample.Assessment : TemporalQualityAssessment::Unreliable
        };
    }

    /// Performs exactly ONE provider capture and conservatively qualifies it.
    ///
    /// Application Bootstrap chooses a provider that reads Clock/coordinator
    /// state coherently. Independent MonotonicNow()/SynchronizedNow()/Era reads
    /// must never be substituted, even if they usually agree in host tests.
    /// @tparam TCoherentClockProvider Bootstrap-selected coherent Clock provider.
    /// @param provider Borrowed provider with a noexcept value-returning capture.
    template<class TCoherentClockProvider>
    [[nodiscard]] QualifiedLogTime CaptureLogTime(
        const TCoherentClockProvider& provider
    ) noexcept {
        static_assert(
            std::is_same_v<
                decltype(std::declval<const TCoherentClockProvider&>().CaptureCoherent()),
                CoherentLogClockSample
            >,
            "Logging Clock provider must return one CoherentLogClockSample by value"
        );
        static_assert(
            noexcept(std::declval<const TCoherentClockProvider&>().CaptureCoherent()),
            "Logging Clock capture provider must be noexcept"
        );
        return QualifyLogClockSample(provider.CaptureCoherent());
    }

} // namespace ESPressio::Logging
