#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <ESPressio_Threading.hpp>

#include "BoundedAdmission.hpp"
#include "detail/ThreadingWaitSlot.hpp"

namespace ESPressio::Logging {

    /// Outcome from publishing an entire configured producer wake fanout.
    enum class AdmissionWakePublicationResult : std::uint8_t {
        NotAttempted = 0U,
        Published = 1U,
        ProviderFailure = 2U
    };

    /// Typed admission status plus notification-health evidence.
    struct CoordinatedAdmissionResult final {
        AdmissionWaitResult Admission;
        AdmissionWakePublicationResult Notification;
    };

    /// Capacity-release and wake-publication outcome.
    enum class CoordinatedReleaseResult : std::uint8_t {
        ReleasedAndNotified = 0U,
        ReleasedNotificationFailed = 1U,
        AlreadyEmpty = 2U,
        MemoryProviderFailure = 3U
    };

    /// Fixed exclusive producer slots bound by Bootstrap to one Threading wait/wake owner.
    ///
    /// Each caller must be assigned a DIFFERENT slot and remain alive until all its
    /// outstanding waits finish. No mutable registration list or caller-stack
    /// pointer escapes to the coordinator. NotifyAll is bounded by TProducerCount
    /// and deliberately over-notifies to avoid sleeping across a capacity
    /// handoff or a provider-level NoWait mutex-contention result.
    ///
    /// This is not yet a FIFO entitlement allocator or proven full G1 producer.
    /// Every capacity release MUST go through ReleaseAndNotify or a manually
    /// checked PublishCapacityAvailable, including release on RAII teardown.
    ///
    /// @tparam TProducerCount Number of statically assigned producer slots.
    /// @tparam TWaitWakeProvider Bootstrap-provided EDP-Threading BoundedWaitWake owner.
    template<std::size_t TProducerCount, class TWaitWakeProvider>
    class AdmissionWakeCoordinator final {
        static_assert(TProducerCount > 0U, "Producer count must be positive");
        static_assert(
            TWaitWakeProvider::Capacity >= TProducerCount,
            "EDP-Threading wake owner has fewer slots than Logging producers"
        );

        TWaitWakeProvider* _waitWake;


    public:
        /// The composed wait/wake provider must outlive all active producers.
        explicit AdmissionWakeCoordinator(TWaitWakeProvider& owner) noexcept :
            _waitWake(&owner) {}

        AdmissionWakeCoordinator(const AdmissionWakeCoordinator&) = delete;
        AdmissionWakeCoordinator& operator =(const AdmissionWakeCoordinator&) = delete;
        AdmissionWakeCoordinator(AdmissionWakeCoordinator&&) = delete;
        AdmissionWakeCoordinator& operator =(AdmissionWakeCoordinator&&) = delete;

        /// Fan out a latched wake to ALL statically configured slots.
        /// Attempt remaining slots even if one native notify fails.
        [[nodiscard]] AdmissionWakePublicationResult PublishCapacityAvailable() noexcept {
            auto status = AdmissionWakePublicationResult::Published;
            for (std::size_t slot = 0U; slot < TProducerCount; ++slot) {
                if (_waitWake->Wake(slot) != ESPressio::Threading::BoundedWaitWakeResult::Woken) {
                    status = AdmissionWakePublicationResult::ProviderFailure;
                }
            }
            return status;
        }

        /// Return one currently owned Memory lease, then notify all possible producers.
        /// An unsuccessful Memory release cannot be advertised as available capacity.
        template<class TLease>
        [[nodiscard]] CoordinatedReleaseResult ReleaseAndNotify(TLease& lease) noexcept {
            switch (lease.Release()) {
                case ESPressio::Memory::ObjectPoolLeaseReleaseResult::Released:
                    return PublishCapacityAvailable() == AdmissionWakePublicationResult::Published
                        ? CoordinatedReleaseResult::ReleasedAndNotified
                        : CoordinatedReleaseResult::ReleasedNotificationFailed;

                case ESPressio::Memory::ObjectPoolLeaseReleaseResult::AlreadyEmpty:
                    return CoordinatedReleaseResult::AlreadyEmpty;

                default:
                    return CoordinatedReleaseResult::MemoryProviderFailure;
            }
        }

        /// Attempt bounded acquisition on the caller's EXCLUSIVE slot; publish
        /// a wake on any grant (other capacity may remain). Deadline is never reset.
        /// A grant that completed late leaves the lease owned by the caller.
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

            Detail::ThreadingWaitSlot<TWaitWakeProvider> waiter{_waitWake, slot};
            const auto acquired = AcquirePoolUntil<TMaximumWakeRetries>(
                pool, waiter, lease, deadline,
                std::forward<TArguments>(arguments)...
            );

            if (
                acquired == AdmissionWaitResult::Admitted ||
                acquired == AdmissionWaitResult::GrantedAfterDeadline
            ) {
                return {acquired, PublishCapacityAvailable()};
            }
            return {acquired, AdmissionWakePublicationResult::NotAttempted};
        }
    };

} // namespace ESPressio::Logging
