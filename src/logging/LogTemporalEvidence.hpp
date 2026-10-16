#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

#include <ESPressio_Clock.hpp>

namespace ESPressio::Logging {

    /// The only shape accepted from an application-composed coherent temporal source.
    /// The source must sample canonical local Monotonic and synchronized quality/era
    /// against the SAME committed clock/coordinator epoch, never as unrelated reads.
    struct CoherentLogClockSample final {
        ESPressio::Clock::MonotonicTimestamp Monotonic;
        ESPressio::Clock::SynchronizedReading Synchronized;
        ESPressio::Clock::ClockEra Era;
        bool HasSynchronizedCoordinate;
    };

    /// A conservative Logging classification of one immutable temporal sample.
    enum class LogTemporalEvidenceStatus : std::uint8_t {
        Synchronized = 0U,
        Degraded = 1U,
        Unavailable = 2U,
        Incoherent = 3U
    };

    /// Logging's immutable temporal projection; zero is a VALID synchronized time,
    /// so Status, never numeric timestamp, is the source-availability discriminator.
    struct QualifiedLogTime final {
        ESPressio::Clock::MonotonicTimestamp Monotonic;
        ESPressio::Clock::SynchronizedTimestamp Synchronized;
        ESPressio::Clock::SynchronizationUncertainty Uncertainty;
        ESPressio::Clock::ClockEra Era;
        ESPressio::Clock::SynchronizationState ClockState;
        LogTemporalEvidenceStatus Status;

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

        const auto consistent = hasCoordinate
            ? eraValid && eligible
            : !eraValid && state != SynchronizationState::Synchronized;

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
            quality
        };
    }

    /// Performs exactly ONE provider capture and conservatively qualifies it.
    ///
    /// Application Bootstrap chooses a provider that reads Clock/coordinator
    /// state coherently. Independent MonotonicNow()/SynchronizedNow()/Era reads
    /// must never be substituted, even if they usually agree in host tests.
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
