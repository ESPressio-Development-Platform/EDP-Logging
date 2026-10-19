#pragma once
#include <cstdint>
#include <logging/LogTemporalEvidence.hpp>

// These are injected Clock owner facts, NOT a claim that this WROOM has
// a live synchronized mesh or that a separate capture is single-epoch.
inline bool CheckG2OwnerClockFactsOnDevice() noexcept {
    namespace C = ESPressio::Clock;
    namespace L = ESPressio::Logging;
    const L::CoherentLogClockSample source{
        // Run() binds the actual MonotonicClock later. The early governed
        // record check must not invoke the unbound global Clock.
        C::MonotonicTimestamp::FromNanoseconds(123456U),
        C::SynchronizedReading{
            C::SynchronizedTimestamp::FromNanoseconds(0U),
            C::SynchronizationUncertainty::FromNanoseconds(20U),
            C::SynchronizationState::Synchronized
        },
        C::ClockEra::FromValue(9U), true,
        C::ClockCorrelationProjectionStatus::Correlated,
        C::TemporalQualityAssessment::Reliable
    };
    struct Provider final {
        L::CoherentLogClockSample Source;
        mutable unsigned Calls{0U};
        L::CoherentLogClockSample CaptureCoherent() const noexcept {
            ++Calls;
            return Source;
        }
    };
    Provider owner{source};
    const auto qualified = L::CaptureLogTime(owner);
    if (owner.Calls != 1U ||
        qualified.Status != L::LogTemporalEvidenceStatus::Synchronized ||
        qualified.Synchronized.Nanoseconds() != 0U ||
        qualified.Era.Value() != 9U ||
        qualified.Uncertainty.Nanoseconds() != 20U ||
        qualified.ProjectionStatus != C::ClockCorrelationProjectionStatus::Correlated ||
        qualified.Assessment != C::TemporalQualityAssessment::Reliable) return false;
    auto absent = source;
    absent.HasSynchronizedCoordinate = false;
    absent.Era = C::ClockEra();
    absent.Synchronized = C::SynchronizedReading{
        C::SynchronizedTimestamp::FromNanoseconds(0U),
        C::SynchronizationUncertainty::Maximum(),
        C::SynchronizationState::NeverSynchronized
    };
    absent.ProjectionStatus = C::ClockCorrelationProjectionStatus::CorrelationUnavailable;
    absent.Assessment = C::TemporalQualityAssessment::NotRequired;
    const auto unavailable = L::QualifyLogClockSample(absent);
    if(unavailable.Status != L::LogTemporalEvidenceStatus::Unavailable ||
       unavailable.HasSynchronizedCoordinate() ||
       unavailable.Assessment != C::TemporalQualityAssessment::NotRequired ||
       unavailable.Era.IsValid()) return false;
    absent.ProjectionStatus = C::ClockCorrelationProjectionStatus::Correlated;
    const auto contradicted = L::QualifyLogClockSample(absent);
    if(contradicted.Status != L::LogTemporalEvidenceStatus::Incoherent ||
       contradicted.HasSynchronizedCoordinate() ||
       contradicted.ProjectionStatus != C::ClockCorrelationProjectionStatus::CorrelationUnavailable ||
       contradicted.Assessment != C::TemporalQualityAssessment::Unreliable) return false;
    absent.ProjectionStatus = static_cast<C::ClockCorrelationProjectionStatus>(255U);
    const auto unknown = L::QualifyLogClockSample(absent);
    return unknown.Status == L::LogTemporalEvidenceStatus::Incoherent &&
           !unknown.HasSynchronizedCoordinate();
}
