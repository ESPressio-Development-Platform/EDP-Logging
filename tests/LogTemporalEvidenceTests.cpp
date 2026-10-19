#include <cassert>
#include <cstdint>
#include <type_traits>

#include <logging/LogTemporalEvidence.hpp>

namespace Test {
    namespace Clock = ESPressio::Clock;
    namespace Logging = ESPressio::Logging;

    constexpr Clock::SynchronizedReading Read(
        Clock::SynchronizationState state
    ) {
        return {
            Clock::SynchronizedTimestamp::FromNanoseconds(0U),
            Clock::SynchronizationUncertainty::FromNanoseconds(20U),
            state
        };
    }

    struct CaptureOwner final {
        Logging::CoherentLogClockSample Sample;
        mutable std::uint32_t Calls{0U};

        Logging::CoherentLogClockSample CaptureCoherent() const noexcept {
            ++Calls;
            return Sample;
        }
    };

    void TestCoherentAndDegraded() {
        const Logging::CoherentLogClockSample coherent{
            Clock::MonotonicTimestamp::FromNanoseconds(200U),
            Read(Clock::SynchronizationState::Synchronized),
            Clock::ClockEra::FromValue(5U),
            true,
            Clock::ClockCorrelationProjectionStatus::Correlated,
            Clock::TemporalQualityAssessment::Reliable
        };
        const auto accepted = Logging::QualifyLogClockSample(coherent);
        assert(accepted.Status == Logging::LogTemporalEvidenceStatus::Synchronized);
        assert(accepted.HasSynchronizedCoordinate());
        assert(accepted.Synchronized.Nanoseconds() == 0U); // zero is valid
        assert(accepted.Era.Value() == 5U);
        assert(accepted.Monotonic.Nanoseconds() == 200U);
        assert(accepted.Uncertainty.Nanoseconds() == 20U);
        assert(accepted.ProjectionStatus ==
               Clock::ClockCorrelationProjectionStatus::Correlated);
        assert(accepted.Assessment == Clock::TemporalQualityAssessment::Reliable);

        CaptureOwner provider{coherent};
        const auto readOnce = Logging::CaptureLogTime(provider);
        assert(provider.Calls == 1U);
        assert(readOnce.Monotonic == accepted.Monotonic);
        assert(readOnce.Status == accepted.Status);

        for (const auto quality : {
            Clock::SynchronizationState::Holdover,
            Clock::SynchronizationState::Reacquiring
        }) {
            const Logging::CoherentLogClockSample degraded{
                Clock::MonotonicTimestamp::FromNanoseconds(201U),
                Read(quality),
                Clock::ClockEra::FromValue(5U),
                true
            };
            const auto result = Logging::QualifyLogClockSample(degraded);
            assert(result.Status == Logging::LogTemporalEvidenceStatus::Degraded);
            assert(result.HasSynchronizedCoordinate());
            assert(result.ClockState == quality);
        }
    }

    void TestUnavailableAndInconsistent() {
        const auto never = Logging::QualifyLogClockSample({
            Clock::MonotonicTimestamp::FromNanoseconds(321U),
            Read(Clock::SynchronizationState::NeverSynchronized),
            Clock::ClockEra(),
            false
        });
        assert(never.Status == Logging::LogTemporalEvidenceStatus::Unavailable);
        assert(!never.HasSynchronizedCoordinate());
        assert(never.Monotonic.Nanoseconds() == 321U);
        assert(!never.Era.IsValid());

        const auto lost = Logging::QualifyLogClockSample({
            Clock::MonotonicTimestamp::FromNanoseconds(322U),
            Read(Clock::SynchronizationState::LostSynchronization),
            Clock::ClockEra(),
            false
        });
        assert(lost.Status == Logging::LogTemporalEvidenceStatus::Unavailable);
        assert(!lost.HasSynchronizedCoordinate());

        const auto missingEra = Logging::QualifyLogClockSample({
            Clock::MonotonicTimestamp::FromNanoseconds(400U),
            Read(Clock::SynchronizationState::Synchronized),
            Clock::ClockEra(),
            true
        });
        assert(missingEra.Status == Logging::LogTemporalEvidenceStatus::Incoherent);
        assert(!missingEra.HasSynchronizedCoordinate());
        assert(missingEra.Monotonic.Nanoseconds() == 400U);
        assert(!missingEra.Era.IsValid());
        assert(missingEra.Uncertainty.Nanoseconds() == Clock::SynchronizationUncertainty::Maximum().Nanoseconds());

        const auto forbiddenOldEra = Logging::QualifyLogClockSample({
            Clock::MonotonicTimestamp::FromNanoseconds(401U),
            Read(Clock::SynchronizationState::LostSynchronization),
            Clock::ClockEra::FromValue(5U),
            true
        });
        assert(forbiddenOldEra.Status == Logging::LogTemporalEvidenceStatus::Incoherent);
        assert(!forbiddenOldEra.HasSynchronizedCoordinate());
        assert(!forbiddenOldEra.Era.IsValid());

        const auto falseReady = Logging::QualifyLogClockSample({
            Clock::MonotonicTimestamp::FromNanoseconds(402U),
            Read(Clock::SynchronizationState::Synchronized),
            Clock::ClockEra(),
            false
        });
        assert(falseReady.Status == Logging::LogTemporalEvidenceStatus::Incoherent);
        assert(!falseReady.HasSynchronizedCoordinate());

        const auto notRequired = Logging::QualifyLogClockSample({
            Clock::MonotonicTimestamp::FromNanoseconds(501U),
            Read(Clock::SynchronizationState::NeverSynchronized),
            Clock::ClockEra(), false,
            Clock::ClockCorrelationProjectionStatus::CorrelationUnavailable,
            Clock::TemporalQualityAssessment::NotRequired
        });
        assert(notRequired.Status == Logging::LogTemporalEvidenceStatus::Unavailable);
        assert(notRequired.Assessment == Clock::TemporalQualityAssessment::NotRequired);
        assert(!notRequired.Era.IsValid());

        const auto impossible = Logging::QualifyLogClockSample({
            Clock::MonotonicTimestamp::FromNanoseconds(502U),
            Read(Clock::SynchronizationState::NeverSynchronized),
            Clock::ClockEra(), false,
            Clock::ClockCorrelationProjectionStatus::Correlated,
            Clock::TemporalQualityAssessment::Reliable
        });
        assert(impossible.Status == Logging::LogTemporalEvidenceStatus::Incoherent);
        assert(impossible.ProjectionStatus ==
               Clock::ClockCorrelationProjectionStatus::CorrelationUnavailable);
        assert(impossible.Assessment == Clock::TemporalQualityAssessment::Unreliable);

        const auto unknownOwnerStatus = Logging::QualifyLogClockSample({
            Clock::MonotonicTimestamp::FromNanoseconds(503U),
            Read(Clock::SynchronizationState::Synchronized),
            Clock::ClockEra::FromValue(7U), true,
            static_cast<Clock::ClockCorrelationProjectionStatus>(255U),
            Clock::TemporalQualityAssessment::Reliable
        });
        assert(unknownOwnerStatus.Status == Logging::LogTemporalEvidenceStatus::Incoherent);
        assert(!unknownOwnerStatus.HasSynchronizedCoordinate());
    }

} // Test

static_assert(std::is_trivially_copyable_v<ESPressio::Logging::CoherentLogClockSample>);
static_assert(std::is_trivially_copyable_v<ESPressio::Logging::QualifiedLogTime>);

int main() {
    Test::TestCoherentAndDegraded();
    Test::TestUnavailableAndInconsistent();
}
