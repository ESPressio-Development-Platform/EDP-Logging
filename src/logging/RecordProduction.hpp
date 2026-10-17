#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Threading.hpp>

#include "AttemptSequence.hpp"
#include "BoundedRecordAdmission.hpp"
#include "LogTemporalEvidence.hpp"

namespace ESPressio::Logging {

    // Records ONE instrumentation attempt, even if bounded admission subsequently fails.
    // These statuses never conflate publication, sink acceptance, or durable storage.
    enum class RecordProductionStatus : std::uint8_t {
        Published,
        OutputOccupied,
        InvalidProducerSlot,
        StopRequested,
        SequencerTimedOut,
        SequencerProviderFailure,
        SequencerReleaseUnconfirmed,
        AttemptExhausted,
        AdmissionFailed,
        CommitFailed
    };

    template<AttemptSequenceWidth TWidth = DefaultAttemptSequenceWidth>
    struct RecordProductionOutcome final {
        RecordProductionStatus Status{RecordProductionStatus::AdmissionFailed};
        AttemptSequence<TWidth> Attempt{};
        RecordAdmissionOutcome Admission{};
        RecordQueueResult Commit{RecordQueueResult::Empty};
        bool TimeCaptured{false};
    };

    // Bootstrap supplies a dedicated EDP-Threading ordinary-context mutex for
    // the incarnation-wide attempt counter. Coherent time and all caller values
    // are captured ONCE at producer entry, before sequencer mutex contention,
    // Queue or Memory waits. Only the ticket must be issued under the mutex.
    // Source records are constructed by EDP-Memory using
    // (AttemptSequence, captured QualifiedLogTime, owned snapshot values...).
    // This generic entry supports finite scalar/enum values, not pointer-bearing
    // aggregate wrappers or spans that may retain a dangling external buffer.
    // Richer diagnostic arguments require the generated owned LogRecord assembler
    // before admission and must never be forwarded as live borrowed storage.
    //
    // On a failed Commit, the move-only Reservation remains with the caller; failed
    // native releases are never silently treated as reclaimed. Caller must retain
    // both scratch lease and Reservation until their ownership is resolved.
    // This is a generic producer envelope, NOT the canonical governed LogRecord
    // (which requires EDP-System's permanent TypeIdentifier).
    template<
        AttemptSequenceWidth TWidth,
        std::size_t TMaximumQueueWakes,
        std::size_t TMaximumMemoryWakes,
        class TSequencerMutex,
        class TTemporalProvider,
        class TQueue,
        class TAdmissionCoordinator,
        class TWaitWakeProvider,
        class TObjectPool,
        class TLease,
        class TStopPredicate,
        class... TArguments
    >
    [[nodiscard]] RecordProductionOutcome<TWidth> ProduceRecordUntil(
        AttemptSequenceCounter<TWidth>& sequence,
        TSequencerMutex& sequencerMutex,
        const TTemporalProvider& temporal,
        TQueue& queue,
        TAdmissionCoordinator& admission,
        TWaitWakeProvider& wake,
        std::size_t producerSlot,
        TObjectPool& pool,
        TLease& scratch,
        typename TQueue::Reservation& reservation,
        ESPressio::Clock::MonotonicTimestamp deadline,
        TStopPredicate& stopRequested,
        TArguments&&... arguments
    ) noexcept {
        static_assert(noexcept(stopRequested()) &&
            std::is_same_v<decltype(stopRequested()), bool>);
        static_assert(
            ((
                (std::is_arithmetic_v<std::remove_cvref_t<TArguments>> ||
                 std::is_enum_v<std::remove_cvref_t<TArguments>>) &&
                std::is_trivially_copyable_v<std::remove_cvref_t<TArguments>>
            ) && ...),
            "Generic Logging producer requires finite owned non-pointer values; "
            "use generated owner-backed LogRecord source assembly for other types"
        );
        RecordProductionOutcome<TWidth> outcome{};

        // The occurrence precedes ALL potentially blocking internal calls.
        // Even subsequent refusal/exhaustion must not silently move that time.
        const auto time = CaptureLogTime(temporal);
        outcome.TimeCaptured = true;
        const std::tuple<std::remove_cvref_t<TArguments>...> sourceSnapshot{
            std::forward<TArguments>(arguments)...
        };
        if (reservation.IsActive() || !scratch.IsEmpty()) {
            outcome.Status = RecordProductionStatus::OutputOccupied;
            return outcome;
        }
        if (!admission.ValidProducerSlot(producerSlot)) {
            outcome.Status = RecordProductionStatus::InvalidProducerSlot;
            return outcome;
        }
        if (stopRequested() || admission.ShutdownRequested()) {
            outcome.Status = RecordProductionStatus::StopRequested;
            return outcome;
        }
        const auto acquired = sequencerMutex.Acquire(
            Detail::RemainingRecordAdmissionWait(deadline)
        );
        if (acquired != ESPressio::Threading::OrdinaryMutexAcquireResult::Acquired) {
            outcome.Status = acquired ==
                ESPressio::Threading::OrdinaryMutexAcquireResult::TimedOut
                ? RecordProductionStatus::SequencerTimedOut
                : RecordProductionStatus::SequencerProviderFailure;
            return outcome;
        }

        // The attempt ticket is issued under the sequencer lock, but neither
        // timestamp nor caller-value ownership is delayed by this acquisition.
        const auto issued = sequence.Issue(outcome.Attempt);
        if (issued != AttemptSequenceAllocationResult::Issued) {
            const auto released = sequencerMutex.Release();
            outcome.Status = released ==
                ESPressio::Threading::OrdinaryMutexReleaseResult::Released
                ? RecordProductionStatus::AttemptExhausted
                : RecordProductionStatus::SequencerReleaseUnconfirmed;
            return outcome;
        }
        if (sequencerMutex.Release() !=
            ESPressio::Threading::OrdinaryMutexReleaseResult::Released) {
            outcome.Status = RecordProductionStatus::SequencerReleaseUnconfirmed;
            return outcome;
        }

        outcome.Admission = std::apply(
            [&](const auto&... owned) noexcept {
                return AcquireRecordUntil<
                    TMaximumQueueWakes, TMaximumMemoryWakes
                >(
                    queue, admission, wake, producerSlot, pool, scratch,
                    reservation, deadline, stopRequested,
                    outcome.Attempt, time, owned...
                );
            },
            sourceSnapshot
        );
        if (outcome.Admission.Status != RecordAdmissionStatus::Admitted) {
            outcome.Status = RecordProductionStatus::AdmissionFailed;
            return outcome;
        }
        outcome.Commit = reservation.Commit();
        outcome.Status = outcome.Commit == RecordQueueResult::Succeeded
            ? RecordProductionStatus::Published
            : RecordProductionStatus::CommitFailed;
        return outcome;
    }

    // Accepted by an output interface does NOT imply durable or physically received.
    enum class RecordSinkSubmission : std::uint8_t {
        Accepted, Rejected, Partial, ProviderFailure
    };

    enum class RecordDrainStatus : std::uint8_t {
        Empty, AcceptedAndReclaimed, FailedAndReclaimed,
        ClaimFailure, ReclaimUnconfirmed
    };

    struct RecordDrainOutcome final {
        RecordDrainStatus Status{RecordDrainStatus::ClaimFailure};
        RecordQueueResult Pop{RecordQueueResult::Empty};
        RecordQueueResult Reclaim{RecordQueueResult::Empty};
        RecordSinkSubmission Submission{RecordSinkSubmission::ProviderFailure};
        bool SubmissionAttempted{false};
    };

    // One consumer owns one FIFO claim while calling the application-composed
    // bounded sink. The sink sees the Memory-owned SOURCE record by reference.
    // Completion retires the source even on rejection/partial acceptance, and
    // the result preserves both sink health and Memory-release health.
    // A failed reclaim retains caller's claim so an uncertain lease is not lost.
    template<class TQueue, class TSink>
    [[nodiscard]] RecordDrainOutcome DrainOneRecord(
        TQueue& queue,
        typename TQueue::ConsumerClaim& owner,
        TSink&& sink
    ) noexcept {
        RecordDrainOutcome outcome{};
        if (owner.IsActive()) {
            outcome.Pop = RecordQueueResult::OutputHandleOccupied;
            return outcome;
        }
        outcome.Pop = queue.Pop(owner);
        if (outcome.Pop == RecordQueueResult::Empty) {
            outcome.Status = RecordDrainStatus::Empty;
            return outcome;
        }
        if (outcome.Pop != RecordQueueResult::Succeeded || !owner.IsActive()) {
            outcome.Status = RecordDrainStatus::ClaimFailure;
            return outcome;
        }
        auto* lease = owner.Borrow();
        if (lease != nullptr && !lease->IsEmpty() && lease->Get() != nullptr) {
            static_assert(noexcept(sink(*lease->Get())));
            static_assert(std::is_same_v<
                decltype(sink(*lease->Get())), RecordSinkSubmission
            >);
            outcome.SubmissionAttempted = true;
            outcome.Submission = sink(*lease->Get());
        }
        outcome.Reclaim = owner.Complete();
        if (outcome.Reclaim != RecordQueueResult::Succeeded &&
            outcome.Reclaim != RecordQueueResult::ReleasedNotificationFailed) {
            outcome.Status = RecordDrainStatus::ReclaimUnconfirmed;
        } else {
            outcome.Status = outcome.Submission == RecordSinkSubmission::Accepted
                ? RecordDrainStatus::AcceptedAndReclaimed
                : RecordDrainStatus::FailedAndReclaimed;
        }
        return outcome;
    }


    /// Per-sink outcomes remain separate from one native Memory reclaim result.
    /// TResultCount is the count of caller-composed independent delivery sinks.
    template<std::size_t TResultCount>
    struct RecordFanoutOutcome final {

        /// Source lease ownership, sink aggregate, and native reclamation evidence.
        RecordDrainOutcome Source{};

        /// Native, per-sink submission results in invocation order.
        std::array<RecordSinkSubmission, TResultCount> Submissions{};

        /// Count of sinks invoked (zero if no available source).
        std::size_t Attempted{0U};

        /// Count of sinks which accepted a delivery submission (not durability).
        std::size_t Accepted{0U};

        /// Count of sinks which did not accept their delivery submission.
        std::size_t Unaccepted{0U};

        /// All participating submissions accepted AND native lease reclaimed.
        [[nodiscard]] bool FullySubmittedAndReclaimed() const noexcept {
            return Source.Status == RecordDrainStatus::AcceptedAndReclaimed &&
                   Attempted == TResultCount && Accepted == TResultCount;
        }

    };

    /// Deliver one source-owned record to 2..8 independently chosen sinks.
    ///
    /// Calls each sink once in provided order while ONE EDP-Memory claim exists.
    /// Reuses DrainOneRecord for actual native claim/lease lifecycle.
    /// Each sink must own/copy its payload before returning; any borrow from
    /// the source expires at the return from this function. Sinks that returned
    /// Accepted are NOT automatically called again if a later sink rejects.
    /// Partial and failed outcomes are observable separately for each sink;
    /// an accepted enqueue is NOT equivalent to durable/physical receipt.
    /// No allocations or platform SDK/Mesh/Serial dependencies are introduced.
    ///
    /// @tparam TQueue Existing bounded transactional Logging record queue.
    /// @tparam TSinks Bootstrap-provided noexcept synchronous sink callbacks.
    template<class TQueue, class... TSinks>
    [[nodiscard]] RecordFanoutOutcome<sizeof...(TSinks)> DrainOneRecordToSinks(
        TQueue& queue,
        typename TQueue::ConsumerClaim& owner,
        TSinks&&... sinks
    ) noexcept {
        constexpr std::size_t SinkCount = sizeof...(TSinks);
        static_assert(SinkCount >= 2U && SinkCount <= 8U,
            "Multi-sink fanout requires 2 to 8 bounded sink callbacks");
        RecordFanoutOutcome<SinkCount> result{};
        result.Submissions.fill(RecordSinkSubmission::ProviderFailure);
        const auto aggregate = [&](const auto& record) noexcept -> RecordSinkSubmission {
            std::size_t index = 0U;
            bool anyPartial = false;
            bool anyFailure = false;
            const auto submit = [&](auto&& sink) noexcept {
                using TSink = decltype(sink);
                static_assert(noexcept(std::declval<TSink>()(record)));
                static_assert(std::is_same_v<
                    decltype(std::declval<TSink>()(record)), RecordSinkSubmission>);
                const auto status = sink(record);
                result.Submissions[index++] = status;
                ++result.Attempted;
                if (status == RecordSinkSubmission::Accepted) {
                    ++result.Accepted;
                } else {
                    ++result.Unaccepted;
                }
                anyPartial |= status == RecordSinkSubmission::Partial;
                anyFailure |= status == RecordSinkSubmission::ProviderFailure;
            };
            (submit(sinks), ...);
            if (result.Accepted == SinkCount) {
                return RecordSinkSubmission::Accepted;
            }
            if (anyPartial || result.Accepted != 0U) {
                return RecordSinkSubmission::Partial;
            }
            return anyFailure
                ? RecordSinkSubmission::ProviderFailure
                : RecordSinkSubmission::Rejected;
        };
        result.Source = DrainOneRecord(queue, owner, aggregate);
        return result;
    }

} // namespace ESPressio::Logging
