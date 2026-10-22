#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "RecordProduction.hpp"

namespace ESPressio::Logging {

    /// One bounded, synchronous coordinator execution quantum, NOT a Thread.
    /// The application-composed EDP-Threading dedicated worker owns wake/stop/join.
    enum class CoordinatorQuantumStatus : std::uint8_t {
        QueueEmpty = 0U,
        BudgetConsumed = 1U,
        ClaimFailed = 2U,
        ReclaimUnconfirmed = 3U,
        PriorClaimUnresolved = 4U
    };

    /// Exact per-sink submission and reclamation evidence from one bounded pass.
    /// No Accepted result here denotes Persistence durability or Serial receipt.
    template<std::size_t TSinkCount>
    struct CoordinatorQuantumOutcome final {
        static_assert(TSinkCount >= 1U && TSinkCount <= 8U);
        CoordinatorQuantumStatus Status{CoordinatorQuantumStatus::QueueEmpty};
        std::size_t Processed{0U};
        std::size_t AcceptedByAll{0U};
        std::size_t FailedSubmissionRecords{0U};
        std::size_t NotificationFailures{0U};
        std::array<std::size_t, TSinkCount> PerSinkAccepted{};
        std::array<std::size_t, TSinkCount> PerSinkUnaccepted{};
    };

    /// Drain at most TMaximumRecords immutable queue records to 1..8 sinks.
    ///
    /// Uses the existing single owner-composed queue/Memory reclaim and
    /// exactly-once per-sink fanout. No lock is held during sink callback.
    /// No promises about a native sink's blocking duration or retention.
    ///
    /// Borrowed original LogRecord is valid ONLY for the synchronous callbacks.
    /// If native lease reclaim cannot be confirmed, the caller-provided claim
    /// remains active and must be resolved by the owning Bootstrap rather than
    /// dropped under an infinite retry loop or forcibly destroyed.
    ///
    /// After BudgetConsumed the worker must check its canonical FIFO and
    /// lifecycle state before sleeping on an EDP-Threading latched wake:
    /// notification is advisory and may be coalesced.
    template<std::size_t TMaximumRecords, class TQueue, class... TSinks>
    [[nodiscard]] CoordinatorQuantumOutcome<sizeof...(TSinks)>
    DrainCoordinatorQuantum(
        TQueue& queue,
        typename TQueue::ConsumerClaim& claim,
        TSinks&... sinks
    ) noexcept {
        constexpr std::size_t Count=sizeof...(TSinks);
        static_assert(TMaximumRecords>=1U,
            "Coordinator drain quantum must have positive finite budget");
        static_assert(Count>=1U && Count<=8U,
            "The coordinator requires 1..8 static sink callbacks");

        CoordinatorQuantumOutcome<Count> result{};
        if (claim.IsActive()) {
            result.Status=CoordinatorQuantumStatus::PriorClaimUnresolved;
            return result;
        }
        for (std::size_t n=0U;n<TMaximumRecords;++n) {
            const auto outcome=DrainOneRecordToSinks(queue,claim,sinks...);
            if (outcome.Source.Status==RecordDrainStatus::Empty) {
                result.Status=CoordinatorQuantumStatus::QueueEmpty;
                return result;
            }
            if (outcome.Source.Status==RecordDrainStatus::ReclaimUnconfirmed) {
                result.Status=CoordinatorQuantumStatus::ReclaimUnconfirmed;
                return result;
            }
            if (outcome.Source.Status==RecordDrainStatus::ClaimFailure) {
                result.Status=CoordinatorQuantumStatus::ClaimFailed;
                return result;
            }
            ++result.Processed;
            if (outcome.FullySubmittedAndReclaimed()) {
                ++result.AcceptedByAll;
            } else {
                ++result.FailedSubmissionRecords;
            }
            if (outcome.Source.Reclaim==RecordQueueResult::ReleasedNotificationFailed) {
                ++result.NotificationFailures;
            }
            for (std::size_t i=0U;i<Count;++i) {
                if (outcome.Submissions[i]==RecordSinkSubmission::Accepted) {
                    ++result.PerSinkAccepted[i];
                } else {
                    ++result.PerSinkUnaccepted[i];
                }
            }
        }
        result.Status=CoordinatorQuantumStatus::BudgetConsumed;
        return result;
    }

} // namespace ESPressio::Logging
