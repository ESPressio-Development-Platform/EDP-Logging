#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Platform.hpp>
#include <ESPressio_Threading.hpp>

#include "AdmissionLifecycleCoordinator.hpp"
#include "TransactionalRecordQueue.hpp"

namespace ESPressio::Logging {

    /// Results of the composed bounded queue-slot + Memory pool transaction.
    enum class RecordAdmissionStatus : std::uint8_t {
        Admitted = 0U,
        DeadlineElapsed = 1U,
        ShutdownRequested = 2U,
        QueueWakeRetryLimit = 3U,
        QueueWaitProviderFailure = 4U,
        QueueFailure = 5U,
        MemoryFailure = 6U,
        LateMemoryGrantReturned = 7U,
        ReleaseUnconfirmed = 8U,
        ReleaseNotificationFailed = 9U,
        OutputOccupied = 10U,
        InvalidProducerSlot = 11U
    };

    struct RecordAdmissionOutcome final {
        RecordAdmissionStatus Status{RecordAdmissionStatus::QueueFailure};
        RecordQueueResult Queue{RecordQueueResult::Empty};
        AdmissionWaitResult Memory{AdmissionWaitResult::DeadlineElapsed};
        CoordinatedReleaseResult Release{CoordinatedReleaseResult::AlreadyEmpty};
        bool MemoryAttempted{false};
        bool ReleaseAttempted{false};
        /// Health of wake fanout performed by a successful or late Memory grant.
        /// Admission success and wake publication are independent facts.
        AdmissionWakePublicationResult WakePublication{
            AdmissionWakePublicationResult::NotAttempted
        };
    };

    namespace Detail {
        /// Only a relative provider wait mechanism; semantic time remains
        /// the caller's one canonical EDP-Clock absolute deadline.
        [[nodiscard]] inline ESPressio::Platform::Synchronization::WaitTimeout
        RemainingRecordAdmissionWait(
            ESPressio::Clock::MonotonicTimestamp deadline
        ) noexcept {
            const auto now = ESPressio::Clock::MonotonicNow();
            if (now >= deadline) {
                return ESPressio::Platform::Synchronization::WaitTimeout::NoWait();
            }
            const auto remaining = ESPressio::Clock::Delta(now, deadline).Nanoseconds();
            if (remaining <= 0) {
                return ESPressio::Platform::Synchronization::WaitTimeout::NoWait();
            }
            return ESPressio::Platform::Synchronization::WaitTimeout::ForNanoseconds(
                static_cast<std::uint64_t>(remaining)
            );
        }
    }

    /// One canonical monotonic deadline covers BOTH Queue-slot and Memory
    /// availability. Always claim bounded queue-slot occupancy BEFORE the
    /// potentially blocking Memory acquisition, so a producer cannot hold a
    /// native Memory lease merely because the FIFO is full.
    ///
    /// Reuses:
    /// - EDP-BoundedTopology slot/FIFO as owned by TQueue
    /// - EDP-Memory NoWait via the EXISTING AdmissionLifecycleCoordinator
    /// - EDP-Threading BoundedWaitWakeProvider WaitUntil on the producer's
    ///   existing exclusive slot (no additional Signal or waiter structure)
    /// - one canonical EDP-Clock deadline and native remaining mutex budget.
    ///
    /// On successful return, output is a move-only active Reservation; caller
    /// must fill/Commit it or let it auto-abort. On provider/failure paths a
    /// lease that could not be released remains explicitly caller-owned and
    /// ReleaseUnconfirmed is returned, never falsely claimed as reclaimed.
    template<
        std::size_t TMaximumQueueWakes,
        std::size_t TMaximumMemoryWakes,
        class TQueue,
        class TAdmissionCoordinator,
        class TWaitWakeProvider,
        class TObjectPool,
        class TLease,
        class TStopPredicate,
        class... TArguments
    >
    [[nodiscard]] RecordAdmissionOutcome AcquireRecordUntil(
        TQueue& queue,
        TAdmissionCoordinator& admission,
        TWaitWakeProvider& waitWake,
        std::size_t producerSlot,
        TObjectPool& pool,
        TLease& scratchLease,
        typename TQueue::Reservation& output,
        ESPressio::Clock::MonotonicTimestamp deadline,
        TStopPredicate& stopRequested,
        TArguments&&... arguments
    ) noexcept {
        static_assert(TMaximumQueueWakes > 0U);
        static_assert(TMaximumMemoryWakes > 0U);
        static_assert(noexcept(stopRequested()) &&
            std::is_same_v<decltype(stopRequested()), bool>);

        RecordAdmissionOutcome outcome{};
        if (output.IsActive() || !scratchLease.IsEmpty()) {
            outcome.Status = RecordAdmissionStatus::OutputOccupied;
            return outcome;
        }
        // The producer must NEVER borrow the lifecycle's reserved drain slot
        // during a queue wait; check before reserving any queue capacity.
        if (!admission.ValidProducerSlot(producerSlot)) {
            outcome.Status = RecordAdmissionStatus::InvalidProducerSlot;
            return outcome;
        }
        // Register all real producer work, including queue waits and rollback.
        auto completeAdmissionScope = admission.TrackAdmission();

        std::size_t queueWakes = 0U;
        while (true) {
            if (stopRequested() || admission.ShutdownRequested()) {
                outcome.Status = RecordAdmissionStatus::ShutdownRequested;
                return outcome;
            }
            if (ESPressio::Clock::MonotonicNow() >= deadline) {
                outcome.Status = RecordAdmissionStatus::DeadlineElapsed;
                return outcome;
            }

            outcome.Queue = queue.ReserveSlot(
                output, Detail::RemainingRecordAdmissionWait(deadline)
            );
            if (outcome.Queue == RecordQueueResult::Succeeded) break;

            if (outcome.Queue == RecordQueueResult::CapacityUnavailable) {
                if (queueWakes >= TMaximumQueueWakes) {
                    outcome.Status = RecordAdmissionStatus::QueueWakeRetryLimit;
                    return outcome;
                }
                const auto waited = waitWake.WaitUntil(producerSlot, deadline);
                if (waited == ESPressio::Threading::BoundedWaitWakeResult::TimedOut) {
                    outcome.Status = RecordAdmissionStatus::DeadlineElapsed;
                    return outcome;
                }
                if (waited != ESPressio::Threading::BoundedWaitWakeResult::Woken) {
                    outcome.Status = RecordAdmissionStatus::QueueWaitProviderFailure;
                    return outcome;
                }
                ++queueWakes;
                // The last wake is entitled to one final capacity check.
                continue;
            }

            outcome.Status = (outcome.Queue == RecordQueueResult::Busy &&
                ESPressio::Clock::MonotonicNow() >= deadline)
                    ? RecordAdmissionStatus::DeadlineElapsed
                    : RecordAdmissionStatus::QueueFailure;
            return outcome;
        }

        // This exclusive slot belongs to output already. Any failure leaves
        // an RAII owner responsible for aborting it; explicitly attempt abort
        // before returning, and never silently swallow its provider failure.
        auto abortOutput = [&]() noexcept {
            if (!output.IsActive()) return;
            const auto aborted = output.Abort();
            if (aborted == RecordQueueResult::ReleasedNotificationFailed) {
                if (outcome.Status != RecordAdmissionStatus::ReleaseUnconfirmed) {
                    outcome.Status = RecordAdmissionStatus::ReleaseNotificationFailed;
                }
            } else if (aborted != RecordQueueResult::Succeeded) {
                // The reservation owner remains live on a failed abort.
                // Report the uncertain state rather than claiming a clean exit.
                // If native Memory release also failed, retain that stronger
                // ownership-uncertainty status while preserving Queue details.
                if (outcome.Status != RecordAdmissionStatus::ReleaseUnconfirmed) {
                    outcome.Status = RecordAdmissionStatus::QueueFailure;
                }
                outcome.Queue = aborted;
            }
        };

        if (stopRequested() || admission.ShutdownRequested()) {
            outcome.Status = RecordAdmissionStatus::ShutdownRequested;
            abortOutput();
            return outcome;
        }
        if (ESPressio::Clock::MonotonicNow() >= deadline) {
            outcome.Status = RecordAdmissionStatus::DeadlineElapsed;
            abortOutput();
            return outcome;
        }

        const auto grant = admission.template AcquireUntil<TMaximumMemoryWakes>(
            producerSlot, pool, scratchLease, deadline,
            std::forward<TArguments>(arguments)...
        );
        outcome.MemoryAttempted = true;
        outcome.Memory = grant.Admission;
        outcome.WakePublication = grant.Notification;

        auto releaseScratch = [&]() noexcept {
            if (scratchLease.IsEmpty()) return;
            outcome.ReleaseAttempted = true;
            outcome.Release = admission.ReleaseAndNotify(scratchLease);
            if (outcome.Release == CoordinatedReleaseResult::MemoryProviderFailure ||
                !scratchLease.IsEmpty()) {
                outcome.Status = RecordAdmissionStatus::ReleaseUnconfirmed;
            } else if (outcome.Release ==
                CoordinatedReleaseResult::ReleasedNotificationFailed &&
                outcome.Status != RecordAdmissionStatus::ReleaseUnconfirmed) {
                outcome.Status = RecordAdmissionStatus::ReleaseNotificationFailed;
            }
        };

        if (grant.Admission != AdmissionWaitResult::Admitted) {
            outcome.Status = grant.Admission == AdmissionWaitResult::GrantedAfterDeadline
                ? RecordAdmissionStatus::LateMemoryGrantReturned
                : grant.Admission == AdmissionWaitResult::ShutdownRequested ||
                  grant.Admission == AdmissionWaitResult::GrantedDuringShutdown
                    ? RecordAdmissionStatus::ShutdownRequested
                    : grant.Admission == AdmissionWaitResult::DeadlineElapsed
                        ? RecordAdmissionStatus::DeadlineElapsed
                        : RecordAdmissionStatus::MemoryFailure;
            releaseScratch();
            abortOutput();
            return outcome;
        }

        if (stopRequested() || admission.ShutdownRequested() ||
            ESPressio::Clock::MonotonicNow() >= deadline) {
            outcome.Status = (stopRequested() || admission.ShutdownRequested())
                ? RecordAdmissionStatus::ShutdownRequested
                : RecordAdmissionStatus::LateMemoryGrantReturned;
            releaseScratch();
            abortOutput();
            return outcome;
        }

        outcome.Queue = queue.AttachLease(
            output, scratchLease, Detail::RemainingRecordAdmissionWait(deadline)
        );
        if (outcome.Queue != RecordQueueResult::Succeeded) {
            outcome.Status = RecordAdmissionStatus::QueueFailure;
            releaseScratch();
            abortOutput();
            return outcome;
        }

        // A native queue mutex can be granted just before its remaining
        // timeout, but finish moving/unlocking the owned Memory lease after
        // the caller's canonical Clock deadline. An on-time Memory grant
        // alone is not permission to admit the record late.
        const bool stoppedAfterAttach = stopRequested() ||
            admission.ShutdownRequested();
        if (stoppedAfterAttach || ESPressio::Clock::MonotonicNow() >= deadline) {
            outcome.Status = stoppedAfterAttach
                ? RecordAdmissionStatus::ShutdownRequested
                : RecordAdmissionStatus::DeadlineElapsed;
            // Scratch is already empty; the queue Reservation owns the
            // exact Memory lease now. Abort through the queue's original
            // lifecycle so release + wake failures remain explicit.
            abortOutput();
            return outcome;
        }

        outcome.Status = RecordAdmissionStatus::Admitted;
        return outcome;
    }

} // namespace ESPressio::Logging
