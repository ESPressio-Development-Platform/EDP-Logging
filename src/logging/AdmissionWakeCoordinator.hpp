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
        /// No producer wake publication was attempted.
        NotAttempted = 0U,
        /// Every configured producer slot received its advisory wake.
        Published = 1U,
        /// At least one configured producer wake could not be confirmed.
        ProviderFailure = 2U
    };

    /// Capacity-release and wake-publication outcome.
    enum class CoordinatedReleaseResult : std::uint8_t {
        /// Memory returned the lease and all producer wakes were published.
        ReleasedAndNotified = 0U,
        /// Memory returned the lease but producer wake publication failed.
        ReleasedNotificationFailed = 1U,
        /// The supplied lease already owned no Memory object.
        AlreadyEmpty = 2U,
        /// Memory could not confirm release; preserve the lease and provider outcome.
        MemoryProviderFailure = 3U
    };

    /// Typed admission status plus notification-health evidence.
    struct CoordinatedAdmissionResult final {

        // Admission and independent capacity notification evidence.

        /// Outcome of the bounded admission operation.
        AdmissionWaitResult Admission;

        /// Outcome of notifying waiters when capacity might have changed.
        AdmissionWakePublicationResult Notification;

    };

    /// Fixed exclusive producer slots bound by Bootstrap to one Threading wait/wake owner.
    ///
    /// Each caller must be assigned a DIFFERENT slot and remain alive until all its
    /// outstanding waits finish. No mutable registration list or caller-stack
    /// pointer escapes to the coordinator. NotifyAll is bounded by TProducerCount
    /// and deliberately over-notifies to avoid sleeping across a capacity
    /// handoff or a provider-level NoWait mutex-contention result.
    ///
    /// The caller uses the transactional queue for FIFO slot entitlement.
    /// Every capacity release MUST go through ReleaseAndNotify or a manually
    /// checked PublishCapacityAvailable, including release on RAII teardown.
    ///
    /// @tparam TProducerCount Number of statically assigned producer slots.
    /// @tparam TWaitWakeProvider Bootstrap-provided EDP-Threading BoundedWaitWake owner.
    template<std::size_t TProducerCount, class TWaitWakeProvider>
    class AdmissionWakeCoordinator final {
        static_assert(
            TProducerCount > 0U,
            "Producer count must be positive"
        );
        static_assert(
            TWaitWakeProvider::Capacity >= TProducerCount,
            "EDP-Threading wake owner has fewer slots than Logging producers"
        );

        /// Borrowed provider for the configured producer wake slots.
        TWaitWakeProvider* _waitWake;


    public:
        /// The composed wait/wake provider must outlive all active producers.
        explicit AdmissionWakeCoordinator(TWaitWakeProvider& owner) noexcept :
            _waitWake(&owner) {}

        /// Prevents copying the association with exclusive producer wait slots.
        AdmissionWakeCoordinator(const AdmissionWakeCoordinator&) = delete;
        /// Prevents replacing an active producer-slot association by copy.
        AdmissionWakeCoordinator& operator =(const AdmissionWakeCoordinator&) = delete;
        /// Keeps this coordinator stable while producer references remain active.
        AdmissionWakeCoordinator(AdmissionWakeCoordinator&&) = delete;
        /// Prevents moving another coordinator into an active association.
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
        /// @tparam TLease Caller-owned native Memory lease to return.
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
        /// @tparam TMaximumWakeRetries Bounded retries after advisory producer wakes.
        /// @tparam TObjectPool Bootstrap-owned Memory pool providing the object.
        /// @tparam TLease Native Memory lease populated by a grant.
        /// @tparam TArguments Object construction arguments forwarded to the pool.
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
