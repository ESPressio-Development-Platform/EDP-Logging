#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>

#include <ESPressio_Threading.hpp>

#include "CoordinatorQuantum.hpp"
#include "CoordinatorSinkService.hpp"

namespace ESPressio::Logging {

    /// Terminal reason published by one EDP-Threading Dedicated Thread activation.
    enum class DedicatedCoordinatorExit : std::uint8_t {
        NeverStarted = 0U,
        CooperativeStop = 1U,
        ClaimFailed = 2U,
        ReclaimUnconfirmed = 3U,
        PriorClaimUnresolved = 4U,
        WakeProviderFailure = 5U,
        SinkServiceRecoveryRequired = 6U
    };

    /// Shared read-only-from-Bootstrap operational report.
    /// Only the Dedicated Thread mutates these fields while it is running.
    struct DedicatedCoordinatorReport final {
        std::atomic<DedicatedCoordinatorExit> Exit{
            DedicatedCoordinatorExit::NeverStarted
        };
        std::atomic<std::uint64_t> RecordsRetired{0U};
        std::atomic<std::uint64_t> AllSinksAccepted{0U};
        std::atomic<std::uint64_t> PartialSubmissions{0U};
        std::atomic<std::uint64_t> NotificationFailures{0U};
        std::atomic<std::uint64_t> IdleTimeouts{0U};
        /// Sum of newly confirmed per-sink record completions, not unique source records.
        std::atomic<std::uint64_t> DurableSinkCompletions{0U};
        /// Fatal ordinary sink-service observations requiring Bootstrap reconciliation.
        std::atomic<std::uint64_t> SinkServiceFailures{0U};
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
    template<
        std::size_t TRecordsPerQuantum,
        std::int64_t TIdleWaitNanoseconds,
        class TQueue,
        class... TSinks
    >
    class DedicatedCoordinator final {
        static_assert(TRecordsPerQuantum > 0U);
        static_assert(TIdleWaitNanoseconds > 0);
        static_assert(sizeof...(TSinks) >= 1U && sizeof...(TSinks) <= 8U);
        static_assert(((!requires(TSinks& sink) {
            sink.ServiceCoordinator();
        } || CoordinatorServiceSink<TSinks>) && ...),
            "Optional ServiceCoordinator must be noexcept and return CoordinatorSinkServiceOutcome");
        TQueue* _queue;
        std::tuple<TSinks*...> _sinks;
        DedicatedCoordinatorReport* _report;
        typename TQueue::ConsumerClaim _claim{};

        /// Polls only compile-time opted-in callbacks in the existing single owner.
        /// Absolute deadlines cannot be extended by later callbacks or arrivals.
        [[nodiscard]] CoordinatorSinkServiceOutcome ServiceSinks() noexcept {
            CoordinatorSinkServiceOutcome result{};
            std::apply(
                [&](TSinks*... sinks) noexcept {
                    (([&]() noexcept {
                        if constexpr (CoordinatorServiceSink<TSinks>) {
                            if (result.Status ==
                                CoordinatorSinkServiceStatus::RecoveryRequired) {
                                return;
                            }
                            const auto serviced = sinks->ServiceCoordinator();
                            _report->DurableSinkCompletions.fetch_add(
                                serviced.DurableSinkCompletions,
                                std::memory_order_relaxed
                            );
                            if (serviced.Status ==
                                CoordinatorSinkServiceStatus::RecoveryRequired) {
                                _report->SinkServiceFailures.fetch_add(
                                    1U, std::memory_order_relaxed
                                );
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
                    }()), ...);
                },
                _sinks
            );
            return result;
        }

    public:
        explicit DedicatedCoordinator(
            TQueue& queue,
            DedicatedCoordinatorReport& report,
            TSinks&... sinks
        ) noexcept : _queue(&queue), _sinks(&sinks...), _report(&report) {}

        DedicatedCoordinator(const DedicatedCoordinator&) = delete;
        DedicatedCoordinator& operator=(const DedicatedCoordinator&) = delete;
        DedicatedCoordinator(DedicatedCoordinator&& source) noexcept
            : _queue(source._queue),
              _sinks(source._sinks),
              _report(source._report),
              _claim(std::move(source._claim)) {}
        DedicatedCoordinator& operator=(DedicatedCoordinator&&) = delete;

        void operator()(Threading::ThreadContext& context) noexcept {
            while (!context.IsStopRequested()) {
                const auto quantum = std::apply(
                    [&](TSinks*... sinks) noexcept {
                        return DrainCoordinatorQuantum<TRecordsPerQuantum>(
                            *_queue, _claim, *sinks...
                        );
                    },
                    _sinks
                );
                _report->RecordsRetired.fetch_add(
                    quantum.Processed, std::memory_order_relaxed
                );
                _report->AllSinksAccepted.fetch_add(
                    quantum.AcceptedByAll, std::memory_order_relaxed
                );
                _report->PartialSubmissions.fetch_add(
                    quantum.FailedSubmissionRecords, std::memory_order_relaxed
                );
                _report->NotificationFailures.fetch_add(
                    quantum.NotificationFailures, std::memory_order_relaxed
                );
                if (quantum.Status == CoordinatorQuantumStatus::ReclaimUnconfirmed) {
                    _report->Exit.store(
                        DedicatedCoordinatorExit::ReclaimUnconfirmed,
                        std::memory_order_release
                    );
                    return;
                }
                if (quantum.Status == CoordinatorQuantumStatus::PriorClaimUnresolved) {
                    _report->Exit.store(
                        DedicatedCoordinatorExit::PriorClaimUnresolved,
                        std::memory_order_release
                    );
                    return;
                }
                if (quantum.Status == CoordinatorQuantumStatus::ClaimFailed) {
                    _report->Exit.store(
                        DedicatedCoordinatorExit::ClaimFailed,
                        std::memory_order_release
                    );
                    return;
                }
                if (context.IsStopRequested()) {
                    break;
                }
                const auto serviced = ServiceSinks();
                if (serviced.Status == CoordinatorSinkServiceStatus::RecoveryRequired) {
                    _report->Exit.store(
                        DedicatedCoordinatorExit::SinkServiceRecoveryRequired,
                        std::memory_order_release
                    );
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
                    : context.WaitFor(
                        Threading::Duration::FromNanoseconds(TIdleWaitNanoseconds)
                    );
                if (waited == Threading::ThreadWaitResult::ProviderFailure &&
                    !context.IsStopRequested()) {
                    _report->Exit.store(
                        DedicatedCoordinatorExit::WakeProviderFailure,
                        std::memory_order_release
                    );
                    return;
                }
                if (waited == Threading::ThreadWaitResult::TimedOut) {
                    _report->IdleTimeouts.fetch_add(1U, std::memory_order_relaxed);
                }
            }
            _report->Exit.store(
                DedicatedCoordinatorExit::CooperativeStop,
                std::memory_order_release
            );
        }
    };

} // ESPressio::Logging
