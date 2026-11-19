#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "RecordProduction.hpp"

namespace ESPressio::Logging {


    /// One bounded, synchronous coordinator execution quantum, NOT a Thread.
    /// The application-composed EDP-Threading dedicated worker owns wake/stop/join.
    enum class CoordinatorQuantumStatus : std::uint8_t {
        /// The next claim found no queued record; prior quantum progress is retained.
        QueueEmpty = 0U,
        /// The finite record budget was consumed and lifecycle checks are due.
        BudgetConsumed = 1U,
        /// The queue could not establish a valid source claim.
        ClaimFailed = 2U,
        /// Source retirement failed and its claim remains owned.
        ReclaimUnconfirmed = 3U,
        /// The supplied claim was already active at quantum entry.
        PriorClaimUnresolved = 4U,
        /// An unclaimed source remains queued after finite mutex contention.
        QueueContended = 5U
    };

    /// Exact per-sink submission and reclamation evidence from one bounded pass.
    /// No Accepted result here denotes Persistence durability or Serial receipt.
    /// @tparam TSinkCount Number of statically selected synchronous sinks.
    template <std::size_t TSinkCount>
    struct CoordinatorQuantumOutcome final {
        static_assert(TSinkCount >= 1U && TSinkCount <= 8U);
        /// Why the quantum returned.
        CoordinatorQuantumStatus Status{CoordinatorQuantumStatus::QueueEmpty};
        /// Records that reached sink and reclaim accounting.
        std::size_t Processed{0U};
        /// Records accepted by all sinks with confirmed source reclamation.
        std::size_t AcceptedByAll{0U};
        /// Accounted records for which full acceptance and reclamation were not reported.
        std::size_t FailedSubmissionRecords{0U};
        /// Confirmed reclamations whose advisory capacity notification failed.
        std::size_t NotificationFailures{0U};
        /// Acceptances by sink position.
        std::array<std::size_t, TSinkCount> PerSinkAccepted{};
        /// Non-acceptance outcomes by sink position.
        std::array<std::size_t, TSinkCount> PerSinkUnaccepted{};
        /// Underlying retirement result when the quantum retains an unresolved claim.
        RecordQueueResult ReclaimFailure{RecordQueueResult::Empty};
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
    namespace Detail {


        /// The single quantum algorithm, selected with an explicit queue mutex policy.
        /// @tparam TMaximumRecords Positive finite source-record budget.
        /// @tparam TBoundedQueueWait Explicitly enables canonical queue-transition deadlines.
        /// @tparam TQueue Existing queue and Memory claim owner.
        /// @tparam TSinks Existing synchronous copying callbacks.
        template <std::size_t TMaximumRecords,
                  bool TBoundedQueueWait,
                  class TQueue,
                  class... TSinks>
        [[nodiscard]] CoordinatorQuantumOutcome<sizeof...(TSinks)>
        DrainCoordinatorQuantumUnderQueuePolicy(TQueue& queue,
                                                typename TQueue::ConsumerClaim& claim,
                                                std::int64_t maximumQueueLockWaitNanoseconds,
                                                TSinks&... sinks) noexcept {
            constexpr std::size_t Count = sizeof...(TSinks);
            static_assert(TMaximumRecords >= 1U,
                          "Coordinator drain quantum must have positive finite budget");
            static_assert(Count >= 1U && Count <= 8U,
                          "The coordinator requires 1..8 static sink callbacks");

            CoordinatorQuantumOutcome<Count> result{};
            if (claim.IsActive()) {
                result.Status = CoordinatorQuantumStatus::PriorClaimUnresolved;
                return result;
            }
            for (std::size_t n = 0U; n < TMaximumRecords; ++n) {
                const auto outcome = DrainOneRecordToSinksUnderQueuePolicy<TBoundedQueueWait>(
                    queue,
                    claim,
                    maximumQueueLockWaitNanoseconds,
                    sinks...
                );
                if (outcome.Source.Status == RecordDrainStatus::Empty) {
                    result.Status = CoordinatorQuantumStatus::QueueEmpty;
                    return result;
                }
                if (outcome.Source.Status == RecordDrainStatus::ReclaimUnconfirmed) {
                    result.Status = CoordinatorQuantumStatus::ReclaimUnconfirmed;
                    result.ReclaimFailure = outcome.Source.Reclaim;
                    return result;
                }
                if (outcome.Source.Status == RecordDrainStatus::ClaimFailure) {
                    if constexpr (TBoundedQueueWait) {
                        if (outcome.Source.Pop == RecordQueueResult::Busy ||
                            outcome.Source.Pop == RecordQueueResult::DeadlineElapsed) {
                            result.Status = CoordinatorQuantumStatus::QueueContended;
                            return result;
                        }
                    }
                    result.Status = CoordinatorQuantumStatus::ClaimFailed;
                    return result;
                }
                ++result.Processed;
                if (outcome.IsFullySubmittedAndReclaimed()) {
                    ++result.AcceptedByAll;
                } else {
                    ++result.FailedSubmissionRecords;
                }
                if (outcome.Source.Reclaim == RecordQueueResult::ReleasedNotificationFailed) {
                    ++result.NotificationFailures;
                }
                for (std::size_t i = 0U; i < Count; ++i) {
                    if (outcome.Submissions[i] == RecordSinkSubmission::Accepted) {
                        ++result.PerSinkAccepted[i];
                    } else {
                        ++result.PerSinkUnaccepted[i];
                    }
                }
            }
            result.Status = CoordinatorQuantumStatus::BudgetConsumed;
            return result;
        }

    } // namespace Detail

    /// Original no-wait quantum; explicit bounded queue locking reuses the same implementation.
    /// @tparam TMaximumRecords Positive finite source-record budget.
    /// @tparam TQueue Existing canonical source queue.
    /// @tparam TSinks Existing synchronous copying sinks.
    template <std::size_t TMaximumRecords, class TQueue, class... TSinks>
    [[nodiscard]] CoordinatorQuantumOutcome<sizeof...(TSinks)> DrainCoordinatorQuantum(
        TQueue& queue, typename TQueue::ConsumerClaim& claim, TSinks&... sinks) noexcept {
        return Detail::DrainCoordinatorQuantumUnderQueuePolicy<TMaximumRecords, false>(
            queue,
            claim,
            0,
            sinks...
        );
    }

} // namespace ESPressio::Logging
