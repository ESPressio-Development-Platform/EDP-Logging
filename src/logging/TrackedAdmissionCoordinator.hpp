#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "AdmissionLifecycleCoordinator.hpp"

namespace ESPressio::Logging {

    enum class TrackedAdmissionValidation : std::uint8_t {
        Valid = 0U,
        WrongCoordinator = 1U,
        LeaseOccupied = 2U
    };

    struct TrackedAdmissionResult final {
        TrackedAdmissionValidation Validation;
        CoordinatedAdmissionResult Admission;
    };

    enum class TrackedDrainStatus : std::uint8_t {
        Quiescent = 0U,
        DeadlineElapsed = 1U,
        WakeRetryLimitReached = 2U,
        DrainWaitProviderFailure = 3U,
        UnconfirmedNativeRelease = 4U
    };

    struct TrackedDrainResult final {
        TrackedDrainStatus Status;
        AdmissionWakePublicationResult WakePublication;
        std::uint32_t ActiveAcquisitions;
        std::uint32_t OutstandingLeases;
        std::uint32_t UnconfirmedReleases;
        std::uint32_t WakeFailures;
    };

    /// Opt-in, bounded owner of grants made THROUGH this class only.
    /// Does not retroactively track raw ObjectPoolLease instances.
    /// Bootstrap must stop new calls and join tasks before destruction.
    template <std::size_t TProducerCount, class TWaitWakeProvider>
    class TrackedAdmissionCoordinator final {
        static_assert(TProducerCount > 0U);
        static_assert(TWaitWakeProvider::Capacity > TProducerCount);
        static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

        TWaitWakeProvider *_waitWake;
        AdmissionLifecycleCoordinator<TProducerCount, TWaitWakeProvider> _lifecycle;
        std::atomic<std::uint32_t> _inflight{0U};
        std::atomic<std::uint32_t> _outstanding{0U};
        std::atomic<std::uint32_t> _unconfirmedReleases{0U};
        std::atomic<std::uint32_t> _wakeFailures{0U};

        static constexpr std::size_t DrainSlot = TProducerCount;

        void WakeDrain() noexcept {
            if (_lifecycle.IsShutdownRequested() &&
                _waitWake->Wake(DrainSlot) != ESPressio::Threading::BoundedWaitWakeResult::Woken) {
                _wakeFailures.fetch_add(1U, std::memory_order_relaxed);
            }
        }

        /// Last-chance *best-effort* notification following the NATIVE lease
        /// destructor, whose success/failure is not observable from this adapter.
        /// Never counts the lease as returned when owner has no proof.
        void WakeAfterNativeLeaseDestruction() noexcept {
            for (std::size_t slot = 0U; slot < TProducerCount; ++slot) {
                if (_waitWake->Wake(slot) != ESPressio::Threading::BoundedWaitWakeResult::Woken) {
                    _wakeFailures.fetch_add(1U, std::memory_order_relaxed);
                }
            }
            WakeDrain();
        }

        struct InflightScope final {
            TrackedAdmissionCoordinator *Owner;
            explicit InflightScope(TrackedAdmissionCoordinator *owner) noexcept : Owner(owner) {}
            InflightScope(const InflightScope &) = delete;
            InflightScope &operator=(const InflightScope &) = delete;
            ~InflightScope() noexcept {
                Owner->_inflight.fetch_sub(1U, std::memory_order_acq_rel);
                Owner->WakeDrain();
            }
        };

      public:
        explicit TrackedAdmissionCoordinator(TWaitWakeProvider &owner) noexcept : _waitWake(&owner), _lifecycle(owner) {}

        TrackedAdmissionCoordinator(const TrackedAdmissionCoordinator &) = delete;
        TrackedAdmissionCoordinator &operator=(const TrackedAdmissionCoordinator &) = delete;
        TrackedAdmissionCoordinator(TrackedAdmissionCoordinator &&) = delete;
        TrackedAdmissionCoordinator &operator=(TrackedAdmissionCoordinator &&) = delete;

        /// RAII owner: a native pool lease is PRIVATE and cannot be manually
        /// destroyed, moved, or returned without the owner notification.
        /// The trailing notifier is declared BEFORE NativeLease, so its
        /// destructor executes AFTER the native lease's own destructor.
        template <class TLease>
        class TrackedLease final {
            struct NativeDestructorNotifier final {
                TrackedAdmissionCoordinator *Owner;
                bool MayHaveReleasedCapacity{false};

                ~NativeDestructorNotifier() noexcept {
                    if (MayHaveReleasedCapacity) {
                        Owner->WakeAfterNativeLeaseDestruction();
                    }
                }
            };

            NativeDestructorNotifier _postNative;
            TLease _lease{};
            TrackedAdmissionCoordinator *_owner;
            bool _tracked{false};

            friend class TrackedAdmissionCoordinator;

          public:
            explicit TrackedLease(TrackedAdmissionCoordinator &owner) noexcept : _postNative{&owner}, _owner(&owner) {
                static_assert(std::is_default_constructible_v<TLease>);
                static_assert(noexcept(std::declval<TLease &>().Release()));
                static_assert(noexcept(std::declval<const TLease &>().IsEmpty()));
            }

            TrackedLease(const TrackedLease &) = delete;
            TrackedLease &operator=(const TrackedLease &) = delete;
            TrackedLease(TrackedLease &&) = delete;
            TrackedLease &operator=(TrackedLease &&) = delete;

            ~TrackedLease() noexcept {
                if (_tracked) {
                    const auto outcome = Release();
                    if (outcome == ESPressio::Logging::CoordinatedReleaseResult::MemoryProviderFailure) {
                        // Native ObjectPoolLease::~ObjectPoolLease will make
                        // another attempt AFTER this destructor body. Its
                        // outcome cannot be observed; therefore conservatively
                        // retain outstanding accounting and report uncertainty.
                        _owner->_unconfirmedReleases.fetch_add(
                            1U, std::memory_order_relaxed);
                        _postNative.MayHaveReleasedCapacity = true;
                    }
                }
            }

            [[nodiscard]] bool IsEmpty() const noexcept { return _lease.IsEmpty(); }
            [[nodiscard]] auto Get() noexcept { return _lease.Get(); }
            [[nodiscard]] auto Get() const noexcept { return _lease.Get(); }

            [[nodiscard]] CoordinatedReleaseResult Release() noexcept {
                if (!_tracked) return CoordinatedReleaseResult::AlreadyEmpty;

                const auto status = _owner->_lifecycle.ReleaseAndNotify(_lease);
                if (
                    status == CoordinatedReleaseResult::ReleasedAndNotified ||
                    status == CoordinatedReleaseResult::ReleasedNotificationFailed) {
                    _tracked = false;
                    _owner->_outstanding.fetch_sub(1U, std::memory_order_acq_rel);
                    if (status == CoordinatedReleaseResult::ReleasedNotificationFailed) {
                        _owner->_wakeFailures.fetch_add(1U, std::memory_order_relaxed);
                    }
                    _owner->WakeDrain();
                }
                return status;
            }
        };

        template <
            std::size_t TMaximumWakeRetries,
            class TObjectPool,
            class TLease,
            class... TArguments>
        [[nodiscard]] TrackedAdmissionResult AcquireUntil(
            std::size_t slot,
            TObjectPool &pool,
            TrackedLease<TLease> &guardedLease,
            ESPressio::Clock::MonotonicTimestamp deadline,
            TArguments &&...arguments) noexcept {
            if (guardedLease._owner != this) {
                return {
                    TrackedAdmissionValidation::WrongCoordinator,
                    {AdmissionWaitResult::OutputLeaseOccupied,
                     AdmissionWakePublicationResult::NotAttempted}};
            }
            if (guardedLease._tracked || !guardedLease._lease.IsEmpty()) {
                return {
                    TrackedAdmissionValidation::LeaseOccupied,
                    {AdmissionWaitResult::OutputLeaseOccupied,
                     AdmissionWakePublicationResult::NotAttempted}};
            }

            // This *outer* active reservation bridges the interval between
            // the nested admission returning and the tracked lease counter
            // being incremented, preventing a false shutdown quiescence.
            _inflight.fetch_add(1U, std::memory_order_acq_rel);
            InflightScope scope{this};
            const auto result = _lifecycle.template AcquireUntil<TMaximumWakeRetries>(
                slot, pool, guardedLease._lease, deadline, std::forward<TArguments>(arguments)...);
            if (
                result.Admission == AdmissionWaitResult::Admitted ||
                result.Admission == AdmissionWaitResult::GrantedAfterDeadline ||
                result.Admission == AdmissionWaitResult::GrantedDuringShutdown) {
                _outstanding.fetch_add(1U, std::memory_order_acq_rel);
                guardedLease._tracked = true;
            }
            return {TrackedAdmissionValidation::Valid, result};
        }

        [[nodiscard]] AdmissionWakePublicationResult RequestShutdown() noexcept {
            return _lifecycle.RequestShutdown();
        }

        [[nodiscard]] std::uint32_t OutstandingLeases() const noexcept {
            return _outstanding.load(std::memory_order_acquire);
        }

        [[nodiscard]] std::uint32_t UnconfirmedReleases() const noexcept {
            return _unconfirmedReleases.load(std::memory_order_acquire);
        }

        /// Finite, single-caller drain of both active acquisition operations
        /// and all *tracked* leases. It does NOT join Tasks or track leases
        /// issued via the old raw admission coordinator.
        template <std::size_t TMaximumDrainWakes>
        [[nodiscard]] TrackedDrainResult ShutdownTrackedUntil(
            ESPressio::Clock::MonotonicTimestamp deadline) noexcept {
            static_assert(TMaximumDrainWakes > 0U);
            const auto publication = RequestShutdown();

            const auto snapshot = [&](TrackedDrainStatus status) noexcept {
                return TrackedDrainResult{
                    status,
                    publication,
                    _inflight.load(std::memory_order_acquire),
                    _outstanding.load(std::memory_order_acquire),
                    _unconfirmedReleases.load(std::memory_order_acquire),
                    _wakeFailures.load(std::memory_order_acquire)};
            };

            for (std::size_t attempt = 0U; attempt < TMaximumDrainWakes; ++attempt) {
                if (UnconfirmedReleases() != 0U) {
                    return snapshot(TrackedDrainStatus::UnconfirmedNativeRelease);
                }
                if (_inflight.load(std::memory_order_acquire) == 0U &&
                    OutstandingLeases() == 0U &&
                    _lifecycle.ActiveAdmissions() == 0U) {
                    return snapshot(TrackedDrainStatus::Quiescent);
                }
                if (ESPressio::Clock::MonotonicNow() >= deadline) {
                    return snapshot(TrackedDrainStatus::DeadlineElapsed);
                }
                const auto result = _waitWake->WaitUntil(DrainSlot, deadline);
                if (result == ESPressio::Threading::BoundedWaitWakeResult::TimedOut) {
                    return snapshot(TrackedDrainStatus::DeadlineElapsed);
                }
                if (result != ESPressio::Threading::BoundedWaitWakeResult::Woken) {
                    return snapshot(TrackedDrainStatus::DrainWaitProviderFailure);
                }
            }
            if (UnconfirmedReleases() != 0U) {
                return snapshot(TrackedDrainStatus::UnconfirmedNativeRelease);
            }
            if (_inflight.load(std::memory_order_acquire) == 0U &&
                OutstandingLeases() == 0U &&
                _lifecycle.ActiveAdmissions() == 0U) {
                return snapshot(TrackedDrainStatus::Quiescent);
            }
            return snapshot(TrackedDrainStatus::WakeRetryLimitReached);
        }
    };

} // namespace ESPressio::Logging
