#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Memory.hpp>
#include <ESPressio_Platform.hpp>
#include <ESPressio_Threading.hpp>

namespace ESPressio::Logging {

    /// Diagnostic producer's bounded pool admission outcome.
    ///
    /// A late grant still owns its lease and MUST be released explicitly.
    enum class AdmissionWaitResult : std::uint8_t {
        Admitted = 0U,
        GrantedAfterDeadline = 1U,
        DeadlineElapsed = 2U,
        WakeRetryLimitReached = 3U,
        WaitProviderFailure = 4U,
        MemoryProviderFailure = 5U,
        MemoryNotInitialized = 6U,
        TopologyUnavailable = 7U,
        OutputLeaseOccupied = 8U,
        InvalidProducerSlot = 9U
    };


    /// Finite, per-producer admission over the Memory owner and Threading wait seam.
    ///
    /// This PARTIAL G1 foundation never invokes EDP-Memory's presently unsafe
    /// finite ObjectPool acquisition. Every attempt uses native NoWait, while
    /// waiting is delegated to the existing EDP-Threading WaitUntil mechanism.
    ///
    /// Preconditions:
    /// - Bootstrap bound ONE canonical EDP-Clock before concurrent use;
    /// - caller owns one exclusive lifetime-stable wait point per producer;
    /// - owner wakes that point after every capacity release and after any
    ///   conflicting admission attempt that could have masked free capacity;
    /// - the wait provider latches wake-before-wait notifications;
    /// - caller explicitly handles a late grant's non-empty lease.
    ///
    /// This is NOT a complete FIFO/coordinator or proven multi-producer path.
    ///
    /// @tparam TMaximumWakeRetries Finite maximum wake/retry transitions.
    /// @tparam TObjectPool Application-composed EDP-Memory ObjectPool-like owner.
    /// @tparam TWaitPoint Application-composed EDP-Threading bounded wait point.
    /// @tparam TLease Empty move-only ObjectPool lease output.
    /// @tparam TArguments Nothrow-constructible object constructor arguments.
    /// @param pool Exclusive application-owned Memory ObjectPool facade.
    /// @param waitPoint Exclusive stable producer wake target.
    /// @param lease Empty output lease; only populated on a successful grant.
    /// @param deadline One absolute canonical EDP-Clock monotonic deadline.
    /// @param arguments Constructor arguments forwarded only when a claim succeeds.
    /// @return Explicit admission/wait/Memory outcome; never hides a late lease.
    template<
        std::size_t TMaximumWakeRetries,
        class TObjectPool,
        class TWaitPoint,
        class TLease,
        class... TArguments
    >
    [[nodiscard]] AdmissionWaitResult AcquirePoolUntil(
        TObjectPool& pool,
        TWaitPoint& waitPoint,
        TLease& lease,
        ESPressio::Clock::MonotonicTimestamp deadline,
        TArguments&&... arguments
    ) noexcept {
        static_assert(
            TMaximumWakeRetries > 0U,
            "Logging bounded admission needs a non-zero static wake retry budget"
        );

        static_assert(
            std::is_same_v<
                decltype(waitPoint.WaitUntil(deadline)),
                ESPressio::Threading::BoundedWaitResult
            >,
            "Logging admission must reuse the canonical Threading WaitUntil result"
        );

        static_assert(
            noexcept(waitPoint.WaitUntil(deadline)),
            "Logging admission requires a noexcept Threading wait provider"
        );

        std::size_t wakes = 0U;

        while (true) {
            if (ESPressio::Clock::MonotonicNow() >= deadline) {
                return AdmissionWaitResult::DeadlineElapsed;
            }

            const auto acquired = pool.Acquire(
                lease,
                ESPressio::Platform::Synchronization::WaitTimeout::NoWait(),
                std::forward<TArguments>(arguments)...
            );

            switch (acquired) {
                case ESPressio::Memory::ObjectPoolAcquisitionResult::Succeeded:
                    // Never erase the ownership fact when a grant crosses
                    // the requested deadline during the native NoWait call.
                    return ESPressio::Clock::MonotonicNow() >= deadline
                        ? AdmissionWaitResult::GrantedAfterDeadline
                        : AdmissionWaitResult::Admitted;

                case ESPressio::Memory::ObjectPoolAcquisitionResult::CapacityUnavailable:
                    break;

                case ESPressio::Memory::ObjectPoolAcquisitionResult::NotInitialized:
                    return AdmissionWaitResult::MemoryNotInitialized;

                case ESPressio::Memory::ObjectPoolAcquisitionResult::TopologyUnavailable:
                    return AdmissionWaitResult::TopologyUnavailable;

                case ESPressio::Memory::ObjectPoolAcquisitionResult::OutputLeaseOccupied:
                    return AdmissionWaitResult::OutputLeaseOccupied;

                default:
                    return AdmissionWaitResult::MemoryProviderFailure;
            }

            const auto waited = waitPoint.WaitUntil(deadline);

            switch (waited) {
                case ESPressio::Threading::BoundedWaitResult::Woken:
                    ++wakes;
                    if (wakes >= TMaximumWakeRetries) {
                        return AdmissionWaitResult::WakeRetryLimitReached;
                    }
                    break;

                case ESPressio::Threading::BoundedWaitResult::TimedOut:
                    return AdmissionWaitResult::DeadlineElapsed;

                default:
                    return AdmissionWaitResult::WaitProviderFailure;
            }
        }
    }

} // namespace ESPressio::Logging
