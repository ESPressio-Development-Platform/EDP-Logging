#pragma once

#include <cstddef>
#include <cstdint>
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

    // Bootstrap must supply a dedicated EDP-Threading ordinary-context mutex for the
    // incarnation-wide attempt counter. Capture is inside this exclusive section,
    // before any queue or Memory wait. Source records are constructed by EDP-Memory
    // using (AttemptSequence, QualifiedLogTime, caller arguments...).
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
        RecordProductionOutcome<TWidth> outcome{};
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

        const auto issued = sequence.Issue(outcome.Attempt);
        // Clock capture must precede the potentially blocking queue/Memory stage.
        // Only one immutable read is allowed; the temporal provider is composed
        // by Bootstrap. No Mesh type is needed or imported.
        if (issued != AttemptSequenceAllocationResult::Issued) {
            const auto released = sequencerMutex.Release();
            outcome.Status = released ==
                ESPressio::Threading::OrdinaryMutexReleaseResult::Released
                ? RecordProductionStatus::AttemptExhausted
                : RecordProductionStatus::SequencerReleaseUnconfirmed;
            return outcome;
        }
        const auto time = CaptureLogTime(temporal);
        outcome.TimeCaptured = true;
        if (sequencerMutex.Release() !=
            ESPressio::Threading::OrdinaryMutexReleaseResult::Released) {
            outcome.Status = RecordProductionStatus::SequencerReleaseUnconfirmed;
            return outcome;
        }

        outcome.Admission = AcquireRecordUntil<
            TMaximumQueueWakes, TMaximumMemoryWakes
        >(
            queue, admission, wake, producerSlot, pool, scratch, reservation,
            deadline, stopRequested, outcome.Attempt, time,
            std::forward<TArguments>(arguments)...
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

} // namespace ESPressio::Logging
