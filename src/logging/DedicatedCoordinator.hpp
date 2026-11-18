#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Threading.hpp>

#include "CoordinatorQuantum.hpp"
#include "CoordinatorSinkService.hpp"

namespace ESPressio::Logging {

    /// Terminal reason published by one EDP-Threading Dedicated Thread activation.
    enum class DedicatedCoordinatorExit : std::uint8_t {
        /// No Dedicated Thread activation has reported an exit.
        NeverStarted = 0U,
        /// The Threading context requested stop and the worker returned.
        CooperativeStop = 1U,
        /// A source claim transition failed before successful consumption.
        ClaimFailed = 2U,
        /// The existing source claim could not be confirmed retired.
        ReclaimUnconfirmed = 3U,
        /// An earlier claim still requires owner reconciliation.
        PriorClaimUnresolved = 4U,
        /// The topology-owned wake operation failed.
        WakeProviderFailure = 5U,
        /// A sink service callback requires Bootstrap reconciliation.
        SinkServiceRecoveryRequired = 6U
    };

    /// Shared read-only-from-Bootstrap operational report.
    /// Only the Dedicated Thread mutates these fields while it is running.
    struct DedicatedCoordinatorReport final {
        /// Latest terminal activation reason visible to Bootstrap.
        std::atomic<DedicatedCoordinatorExit> Exit{DedicatedCoordinatorExit::NeverStarted};
        /// Records whose source Memory ownership was confirmed retired.
        std::atomic<std::uint64_t> RecordsRetired{0U};
        /// Records accepted by all callbacks; not a durability count.
        std::atomic<std::uint64_t> AllSinksAccepted{0U};
        /// Source records with at least one unaccepted sink submission.
        std::atomic<std::uint64_t> PartialSubmissions{0U};
        /// Confirmed releases whose advisory capacity notification failed.
        std::atomic<std::uint64_t> NotificationFailures{0U};
        /// Timeout observations from the existing topology-owned wait.
        std::atomic<std::uint64_t> IdleTimeouts{0U};
        /// Sum of newly confirmed per-sink record completions, not unique source records.
        std::atomic<std::uint64_t> DurableSinkCompletions{0U};
        /// Fatal ordinary sink-service observations requiring Bootstrap reconciliation.
        std::atomic<std::uint64_t> SinkServiceFailures{0U};
        /// Ordinary finite queue lock contention before any claim or sink invocation.
        std::atomic<std::uint64_t> QueueContentions{0U};
        /// Exact queue retirement result when Exit is ReclaimUnconfirmed.
        std::atomic<RecordQueueResult> ReclaimFailure{RecordQueueResult::Empty};
    };

    /// Explicit compile-time opt-in to finite locking on the SAME queue and ordinary mutex.
    /// @tparam TMaximumNanoseconds Positive per-transition mutex budget; no global sink-time
    /// guarantee.
    template <std::int64_t TMaximumNanoseconds>
    struct CoordinatorQueueLockWait final {
        static_assert(TMaximumNanoseconds > 0);
        /// Canonical physical duration applied separately before claim and retirement.
        static constexpr std::int64_t Nanoseconds = TMaximumNanoseconds;
    };

    /// Real EDP-Threading DedicatedThread-compatible callable.
    ///
    /// Bootstrap owns the actual Dedicated Thread topology, its native
    /// execution provider, wake signal, queue, sink callbacks and report.
    /// This callable is movable but non-allocating: it stores only pointers.
    /// No second scheduler/worker context, unmanaged std::thread or polling
    /// provider is created. Use BindDedicatedThread<TIdentity>(callable).
    ///
    /// A zero-work QueueEmpty quantum calls the ThreadContext finite WaitFor
    /// on the *same topology-owned wake* published by Thread::Wake(); finite
    /// timeout is a conservative lost/coalesced wake fallback, not a separate
    /// clock or queue waiter. On wake, canonical Queue is checked again.
    ///
    /// A sink may additionally expose noexcept ServiceCoordinator returning
    /// CoordinatorSinkServiceOutcome. It is polled after every healthy quantum,
    /// including a consumed budget. On empty input the earliest canonical
    /// deadline uses ThreadContext::WaitUntil; all clocks MUST share its origin.
    /// Per-sink durable completions remain separate from RAM acceptance/retirement.
    /// RecoveryRequired terminates without retry or implicit shutdown flushing.
    ///
    /// Stop is cooperatively requested via Thread::RequestStop() and followed
    /// by finite JoinUntil/JoinFor under Bootstrap. This callable exits on
    /// stop without silently draining indefinitely. Bootstrap must reject new
    /// producers, then choose an explicit bounded shutdown/retention policy.
    /// When a native claim cannot be reclaimed, it remains owned by the
    /// stable callback object: no pointer/claim is destroyed until topology
    /// reclaims or aborts it according to the original owner contract.
    ///
    /// WARNING: each synchronous sink callback MUST return within an
    /// application-budgeted period; this adapter cannot preempt a blocking
    /// sink and cannot establish a global bounded Join guarantee itself.
    /// @tparam TRecordsPerQuantum Positive finite count of records before a service/stop check.
    /// @tparam TIdleWaitNanoseconds Positive finite fallback when no sink has a pending deadline.
    /// @tparam TQueue Existing transactional source queue and Memory claim owner.
    /// @tparam TSinks One through eight synchronous source-copying callbacks.
    template <std::size_t TRecordsPerQuantum,
              std::int64_t TIdleWaitNanoseconds,
              class TQueue,
              class... TSinks>
    class DedicatedCoordinator final {
        static_assert(TRecordsPerQuantum > 0U);
        static_assert(TIdleWaitNanoseconds > 0);
        static_assert(sizeof...(TSinks) >= 1U && sizeof...(TSinks) <= 8U);
        static_assert(((!requires(TSinks &sink) {
                           sink.ServiceCoordinator();
                       } || CoordinatorServiceSink<TSinks>) && ...),
                      "Optional ServiceCoordinator must be noexcept and return "
                      "CoordinatorSinkServiceOutcome");
        /// Existing canonical source queue, retained through claim reconciliation.
        TQueue *_queue;
        /// Stable synchronous sink callbacks and their optional ordinary service methods.
        std::tuple<TSinks *...> _sinks;
        /// Bootstrap-owned atomic operational report.
        DedicatedCoordinatorReport *_report;
        /// Stable exclusive source claim retained after unconfirmed retirement.
        typename TQueue::ConsumerClaim _claim{};
        /// Zero selects immediate NoWait transitions; a compile-time opt-in supplies a finite positive
        /// budget.
        std::int64_t _queueMutexWaitNanoseconds{0};

        /// Existing queue must expose bounded transitions before that opt-in can be selected.
        static constexpr bool SupportsBoundedQueueWait =
            requires(TQueue &queue,
                     typename TQueue::ConsumerClaim &claim,
                     Clock::MonotonicTimestamp deadline) {
                { queue.PopUntil(
                    claim,
                    deadline) } noexcept -> std::same_as<RecordQueueResult>;
                { claim.CompleteUntil(deadline) } noexcept -> std::same_as<RecordQueueResult>;
            };

        /// Polls only compile-time opted-in callbacks in the existing single owner.
        /// Absolute deadlines cannot be extended by later callbacks or arrivals.
        [[nodiscard]] CoordinatorSinkServiceOutcome ServiceSinks() noexcept {
            CoordinatorSinkServiceOutcome result{};
            std::apply(
                [&](TSinks *...sinks) noexcept {
                    (([&]() noexcept {
                         if constexpr (CoordinatorServiceSink<TSinks>) {
                             if (result.Status == CoordinatorSinkServiceStatus::RecoveryRequired) {
                                 return;
                             }
                             const auto serviced = sinks->ServiceCoordinator();
                             _report->DurableSinkCompletions.fetch_add(
                                 serviced.DurableSinkCompletions,
                                 std::memory_order_relaxed);
                             if (serviced.Status ==
                                 CoordinatorSinkServiceStatus::RecoveryRequired) {
                                 _report->SinkServiceFailures.fetch_add(
                                     1U,
                                     std::memory_order_relaxed);
                                 result.Status = serviced.Status;
                                 return;
                             }
                             if (serviced.NextDeadline.has_value() &&
                                 (!result.NextDeadline.has_value() ||
                                  serviced.NextDeadline->Nanoseconds() <
                                      result.NextDeadline->Nanoseconds())) {
                                 result.NextDeadline = serviced.NextDeadline;
                             }
                         }
                     }()),
                     ...);
                },
                _sinks);
            return result;
        }

      public:
        /// Borrows the existing owners and selects immediate NoWait queue transitions.
        explicit DedicatedCoordinator(TQueue &queue,
                                      DedicatedCoordinatorReport &report,
                                      TSinks &...sinks) noexcept
            : _queue(&queue), _sinks(&sinks...), _report(&report) {}

        /// Selects bounded transitions using the already-bound canonical Clock and existing queue
        /// mutex.
        /// @tparam TMaximumNanoseconds Positive independent claim/retirement mutex wait budget.
        template <std::int64_t TMaximumNanoseconds>
        explicit DedicatedCoordinator(CoordinatorQueueLockWait<TMaximumNanoseconds>,
                                      TQueue &queue,
                                      DedicatedCoordinatorReport &report,
                                      TSinks &...sinks) noexcept
            : _queue(&queue), _sinks(&sinks...), _report(&report),
              _queueMutexWaitNanoseconds(TMaximumNanoseconds) {
            static_assert(SupportsBoundedQueueWait,
                          "Bounded coordinator locking requires existing queue PopUntil and claim "
                          "CompleteUntil");
        }

        /// A live source claim cannot be copied to another coordinator.
        DedicatedCoordinator(const DedicatedCoordinator &) = delete;
        /// Prevents replacing the owner of a possibly unresolved source claim.
        DedicatedCoordinator &operator=(const DedicatedCoordinator &) = delete;
        /// Transfers the stable claim and selected mutex budget into the Threading binding.
        DedicatedCoordinator(DedicatedCoordinator &&source) noexcept
            : _queue(source._queue), _sinks(source._sinks), _report(source._report),
              _claim(std::move(source._claim)),
              _queueMutexWaitNanoseconds(source._queueMutexWaitNanoseconds) {}
        /// Prevents assignment across active coordinator ownership boundaries.
        DedicatedCoordinator &operator=(DedicatedCoordinator &&) = delete;

        /// Runs finite quanta and optional service inside the one managed Threading context.
        void operator()(Threading::ThreadContext &context) noexcept {
            _report->ReclaimFailure.store(RecordQueueResult::Empty, std::memory_order_relaxed);
            while (!context.IsStopRequested()) {
                const auto quantum = std::apply(
                    [&](TSinks *...sinks) noexcept {
                        if constexpr (SupportsBoundedQueueWait) {
                            if (_queueMutexWaitNanoseconds > 0) {
                                return Detail::DrainCoordinatorQuantumUnderQueuePolicy<
                                    TRecordsPerQuantum,
                                    true>(
                                    *_queue,
                                    _claim,
                                    _queueMutexWaitNanoseconds,
                                    *sinks...);
                            }
                        }
                        return DrainCoordinatorQuantum<TRecordsPerQuantum>(
                            *_queue,
                            _claim,
                            *sinks...);
                    },
                    _sinks);
                _report->RecordsRetired.fetch_add(
                    quantum.Processed,
                    std::memory_order_relaxed);
                _report->AllSinksAccepted.fetch_add(
                    quantum.AcceptedByAll,
                    std::memory_order_relaxed);
                _report->PartialSubmissions.fetch_add(
                    quantum.FailedSubmissionRecords,
                    std::memory_order_relaxed);
                _report->NotificationFailures.fetch_add(
                    quantum.NotificationFailures,
                    std::memory_order_relaxed);
                if (quantum.Status == CoordinatorQuantumStatus::QueueContended) {
                    _report->QueueContentions.fetch_add(
                        1U,
                        std::memory_order_relaxed);
                }
                if (quantum.Status == CoordinatorQuantumStatus::ReclaimUnconfirmed) {
                    _report->ReclaimFailure.store(
                        quantum.ReclaimFailure,
                        std::memory_order_relaxed);
                    _report->Exit.store(
                        DedicatedCoordinatorExit::ReclaimUnconfirmed,
                        std::memory_order_release);
                    return;
                }
                if (quantum.Status == CoordinatorQuantumStatus::PriorClaimUnresolved) {
                    _report->Exit.store(
                        DedicatedCoordinatorExit::PriorClaimUnresolved,
                        std::memory_order_release);
                    return;
                }
                if (quantum.Status == CoordinatorQuantumStatus::ClaimFailed) {
                    _report->Exit.store(
                        DedicatedCoordinatorExit::ClaimFailed,
                        std::memory_order_release);
                    return;
                }
                if (context.IsStopRequested()) {
                    break;
                }
                const auto serviced = ServiceSinks();
                if (serviced.Status == CoordinatorSinkServiceStatus::RecoveryRequired) {
                    _report->Exit.store(
                        DedicatedCoordinatorExit::SinkServiceRecoveryRequired,
                        std::memory_order_release);
                    return;
                }
                if (quantum.Status == CoordinatorQuantumStatus::BudgetConsumed) {
                    continue;
                }
                // One existing topology-owned wake: pending sinks use their
                // earliest canonical absolute deadline; an empty sink set keeps
                // the prior finite idle fallback. No extra waiter or timer.
                const auto waited = serviced.NextDeadline.has_value()
                                        ? context.WaitUntil(*serviced.NextDeadline)
                                        : context.WaitFor(Threading::Duration::FromNanoseconds(
                                              TIdleWaitNanoseconds));
                if (waited == Threading::ThreadWaitResult::ProviderFailure &&
                    !context.IsStopRequested()) {
                    _report->Exit.store(
                        DedicatedCoordinatorExit::WakeProviderFailure,
                        std::memory_order_release);
                    return;
                }
                if (waited == Threading::ThreadWaitResult::TimedOut) {
                    _report->IdleTimeouts.fetch_add(
                        1U,
                        std::memory_order_relaxed);
                }
            }
            _report->Exit.store(
                DedicatedCoordinatorExit::CooperativeStop,
                std::memory_order_release);
        }
    };

} // namespace ESPressio::Logging
