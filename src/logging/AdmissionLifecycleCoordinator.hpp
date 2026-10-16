#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Threading.hpp>

#include "AdmissionWakeCoordinator.hpp"

namespace ESPressio::Logging {

    /// A finite attempt to prevent new admissions and drain in-flight attempts.
    enum class AdmissionShutdownStatus : std::uint8_t {
        Quiescent = 0U,
        DeadlineElapsed = 1U,
        WakeRetryLimitReached = 2U,
        DrainWaitProviderFailure = 3U
    };

    /// Distinct results: all waiters quiescent and notification delivery health.
    /// ActiveAdmissions is a diagnostic only and may change if callers violate
    /// the explicit stop-invocation precondition.
    struct AdmissionShutdownResult final {
        AdmissionShutdownStatus Status;
        AdmissionWakePublicationResult WakePublication;
        std::uint32_t ActiveAdmissions;
    };


    /// Adds finite cancellation and in-flight admission quiescence to the
    /// existing composed Memory/Threading coordinator.
    ///
    /// Bootstrap assigns one UNIQUE producer slot to each producer task. A
    /// dedicated extra Threading wake slot is reserved for shutdown drain;
    /// it MUST NOT be used by any producer. External task orchestration MUST
    /// stop creating new calls and join tasks before destroying this object.
    ///
    /// This tracks *in-flight acquisition calls only*, not leases granted to
    /// consumers. Outstanding leases must be released and capacity published
    /// by their owners before MemoryTopology teardown; it cannot be inferred
    /// from a zero ActiveAdmissions count. No internal task/heap/mutex/sleep.
    template<std::size_t TProducerCount, class TWaitWakeProvider>
    class AdmissionLifecycleCoordinator final {
        static_assert(TProducerCount > 0U, "At least one producer required");
        static_assert(
            TWaitWakeProvider::Capacity > TProducerCount,
            "One extra EDP-Threading slot is reserved for shutdown drain"
        );
        static_assert(
            std::atomic<std::uint32_t>::is_always_lock_free,
            "G1 admission lifecycle requires nonblocking atomic state"
        );

        TWaitWakeProvider* _waitWake;
        AdmissionWakeCoordinator<TProducerCount, TWaitWakeProvider> _coordinator;
        std::atomic<bool> _shuttingDown{false};
        std::atomic<std::uint32_t> _activeAdmissions{0U};

        static constexpr std::size_t DrainSlot = TProducerCount;

        /// Always decrements on each return, including deadline/provider error.
        /// All active calls retain the parent coordinator's lifetime.
        struct ActiveAdmissionScope final {
            AdmissionLifecycleCoordinator* Owner;

            explicit ActiveAdmissionScope(AdmissionLifecycleCoordinator* owner) noexcept :
                Owner(owner) {}

            ActiveAdmissionScope(const ActiveAdmissionScope&) = delete;
            ActiveAdmissionScope& operator=(const ActiveAdmissionScope&) = delete;

            ~ActiveAdmissionScope() noexcept {
                Owner->_activeAdmissions.fetch_sub(1U, std::memory_order_acq_rel);
                // The shutdown drainer may already be waiting. A latched wake
                // handles completion-before-registration and is also safe when
                // no shutdown is requested yet.
                if (Owner->_shuttingDown.load(std::memory_order_acquire)) {
                    static_cast<void>(Owner->_waitWake->Wake(DrainSlot));
                }
            }
        };


    public:
        explicit AdmissionLifecycleCoordinator(TWaitWakeProvider& owner) noexcept :
            _waitWake(&owner), _coordinator(owner) {}

        AdmissionLifecycleCoordinator(const AdmissionLifecycleCoordinator&) = delete;
        AdmissionLifecycleCoordinator& operator=(const AdmissionLifecycleCoordinator&) = delete;
        AdmissionLifecycleCoordinator(AdmissionLifecycleCoordinator&&) = delete;
        AdmissionLifecycleCoordinator& operator=(AdmissionLifecycleCoordinator&&) = delete;

        [[nodiscard]] bool ShutdownRequested() const noexcept {
            return _shuttingDown.load(std::memory_order_acquire);
        }

        [[nodiscard]] std::uint32_t ActiveAdmissions() const noexcept {
            return _activeAdmissions.load(std::memory_order_acquire);
        }

        /// Prevents future grants by this coordinator and wakes all producers.
        ///
        /// A failed publication is explicitly returned. Caller must retain the
        /// coordinator alive and retry publication/escalate until the active
        /// workers either wake or expire by their pre-existing deadlines.
        [[nodiscard]] AdmissionWakePublicationResult RequestShutdown() noexcept {
            _shuttingDown.store(true, std::memory_order_release);
            return _coordinator.PublishCapacityAvailable();
        }

        /// Exclusive per-producer slot. Admission retries stop as soon as the
        /// cancellation signal is observed, including following a latched wake.
        /// A grant racing the shutdown remains owned and is explicitly reported.
        template<
            std::size_t TMaximumWakeRetries,
            class TObjectPool,
            class TLease,
            class... TArguments
        >
        [[nodiscard]] CoordinatedAdmissionResult AcquireUntil(
            std::size_t slot,
            TObjectPool& pool,
            TLease& lease,
            ESPressio::Clock::MonotonicTimestamp deadline,
            TArguments&&... arguments
        ) noexcept {
            if (slot >= TProducerCount) {
                return {
                    AdmissionWaitResult::InvalidProducerSlot,
                    AdmissionWakePublicationResult::NotAttempted
                };
            }

            // Count before examining shutdown. If shutdown is already set,
            // no pool claim can occur; this call only briefly enters/exits.
            _activeAdmissions.fetch_add(1U, std::memory_order_acq_rel);
            ActiveAdmissionScope active{this};
            if (ShutdownRequested()) {
                return {
                    AdmissionWaitResult::ShutdownRequested,
                    AdmissionWakePublicationResult::NotAttempted
                };
            }

            Detail::ThreadingWaitSlot<TWaitWakeProvider> waitPoint{_waitWake, slot};
            const auto stop = [this]() noexcept -> bool {
                return ShutdownRequested();
            };
            const auto status = AcquirePoolUntilGuarded<TMaximumWakeRetries>(
                pool, waitPoint, lease, deadline, stop,
                std::forward<TArguments>(arguments)...
            );

            if (
                status == AdmissionWaitResult::Admitted ||
                status == AdmissionWaitResult::GrantedAfterDeadline ||
                status == AdmissionWaitResult::GrantedDuringShutdown
            ) {
                return {status, _coordinator.PublishCapacityAvailable()};
            }
            return {status, AdmissionWakePublicationResult::NotAttempted};
        }

        /// Remains usable during shutdown to return outstanding Memory leases.
        template<class TLease>
        [[nodiscard]] CoordinatedReleaseResult ReleaseAndNotify(TLease& lease) noexcept {
            return _coordinator.ReleaseAndNotify(lease);
        }

        /// Bounded drain of *acquisition calls* with one absolute Clock deadline.
        /// One caller only; not reentrant. Provider can wake spuriously, therefore
        /// the compile-time wake budget prevents repeated immediate Wake storms.
        /// A Quiescent result is NOT evidence that accepted leases were returned
        /// or application producer tasks have joined; Bootstrap must ensure both.
        template<std::size_t TMaximumDrainWakes>
        [[nodiscard]] AdmissionShutdownResult ShutdownUntil(
            ESPressio::Clock::MonotonicTimestamp deadline
        ) noexcept {
            static_assert(TMaximumDrainWakes > 0U);
            const auto publication = RequestShutdown();
            for (std::size_t i = 0U; i < TMaximumDrainWakes; ++i) {
                const auto active = ActiveAdmissions();
                if (active == 0U) {
                    return {
                        AdmissionShutdownStatus::Quiescent,
                        publication,
                        0U
                    };
                }
                if (ESPressio::Clock::MonotonicNow() >= deadline) {
                    return {
                        AdmissionShutdownStatus::DeadlineElapsed,
                        publication,
                        active
                    };
                }

                const auto result = _waitWake->WaitUntil(DrainSlot, deadline);
                if (result == ESPressio::Threading::BoundedWaitWakeResult::TimedOut) {
                    return {
                        AdmissionShutdownStatus::DeadlineElapsed,
                        publication,
                        ActiveAdmissions()
                    };
                }
                if (result != ESPressio::Threading::BoundedWaitWakeResult::Woken) {
                    return {
                        AdmissionShutdownStatus::DrainWaitProviderFailure,
                        publication,
                        ActiveAdmissions()
                    };
                }
            }

            return {
                ActiveAdmissions() == 0U
                    ? AdmissionShutdownStatus::Quiescent
                    : AdmissionShutdownStatus::WakeRetryLimitReached,
                publication,
                ActiveAdmissions()
            };
        }
    };

} // namespace ESPressio::Logging
