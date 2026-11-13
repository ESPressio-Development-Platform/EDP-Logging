#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "AdmissionLifecycleCoordinator.hpp"

namespace ESPressio::Logging {

    /// Validates the caller's output guard before attempting Memory acquisition.
    enum class TrackedAdmissionValidation : std::uint8_t {
        /// The guard belongs to this coordinator and admission was attempted.
        Valid = 0U,
        /// The guard belongs to another coordinator.
        WrongCoordinator = 1U,
        /// The guard already tracks or contains a native lease.
        LeaseOccupied = 2U
    };

    /// Terminal outcome of a bounded tracked shutdown drain.
    enum class TrackedDrainStatus : std::uint8_t {
        /// All tracked acquisitions and leases have drained.
        Quiescent = 0U,
        /// The canonical deadline elapsed before quiescence was confirmed.
        DeadlineElapsed = 1U,
        /// The configured drain wake budget was exhausted.
        WakeRetryLimitReached = 2U,
        /// The Threading wait provider failed during drain.
        DrainWaitProviderFailure = 3U,
        /// A native lease release remains unconfirmed.
        UnconfirmedNativeRelease = 4U
    };

    /// Guard validation and the corresponding lifecycle admission outcome.
    struct TrackedAdmissionResult final {
        /// Whether the guard passed validation before admission.
        TrackedAdmissionValidation Validation;
        /// Typed lifecycle result, including any late grant and wake publication.
        CoordinatedAdmissionResult Admission;
    };

    /// Drain result and individually sampled counters observed on return.
    struct TrackedDrainResult final {
        /// Whether tracked quiescence was confirmed and why drain stopped.
        TrackedDrainStatus Status;
        /// Wake publication result from requesting shutdown.
        AdmissionWakePublicationResult WakePublication;
        /// Active outer acquisition calls observed at return.
        std::uint32_t ActiveAcquisitions;
        /// Tracked leases whose native release has not been confirmed.
        std::uint32_t OutstandingLeases;
        /// Sticky count of unconfirmed destructor releases.
        std::uint32_t UnconfirmedReleases;
        /// Observed count of failed advisory wake attempts.
        std::uint32_t WakeFailures;
    };

    /// Tracks grants acquired through this coordinator until native release is confirmed.
    /// Bootstrap stops new calls and joins tasks before destroying this owner.
    /// @tparam TProducerCount Number of stable producer wake slots.
    /// @tparam TWaitWakeProvider Bootstrap-owned Threading wait/wake provider with one extra drain slot.
    template <std::size_t TProducerCount, class TWaitWakeProvider>
    class TrackedAdmissionCoordinator final {
        static_assert(TProducerCount > 0U);
        static_assert(TWaitWakeProvider::Capacity > TProducerCount);
        static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

        // Admission and wake owners retained for the coordinator's lifetime.
        /// Borrowed wait/wake provider shared with the embedded lifecycle coordinator.
        TWaitWakeProvider *_waitWake;
        /// Owns cancellation-aware admission and producer wake publication.
        AdmissionLifecycleCoordinator<TProducerCount, TWaitWakeProvider> _lifecycle;

        // Accounting used to decide whether tracked work has drained.
        /// Active outer acquisitions, including handover into tracked lease ownership.
        std::atomic<std::uint32_t> _inflight{0U};
        /// Granted leases awaiting confirmed native release.
        std::atomic<std::uint32_t> _outstanding{0U};
        /// Native destructor outcomes that could not be observed.
        std::atomic<std::uint32_t> _unconfirmedReleases{0U};
        /// Failed advisory producer or drain wake attempts.
        std::atomic<std::uint32_t> _wakeFailures{0U};

        /// Extra wake slot reserved for the single shutdown-drain caller.
        static constexpr std::size_t DrainSlot = TProducerCount;

        // Notifications after a change in acquisition or release accounting.
        /// Wakes the drain caller after shutdown and records notification failure.
        void WakeDrain() noexcept {
            if (_lifecycle.IsShutdownRequested() &&
                _waitWake->Wake(DrainSlot) != ESPressio::Threading::BoundedWaitWakeResult::Woken) {
                _wakeFailures.fetch_add(
                    1U,
                    std::memory_order_relaxed);
            }
        }

        /// Last-chance *best-effort* notification following the NATIVE lease
        /// destructor, whose success/failure is not observable from this adapter.
        /// Never counts the lease as returned when owner has no proof.
        void WakeAfterNativeLeaseDestruction() noexcept {
            for (std::size_t slot = 0U; slot < TProducerCount; ++slot) {
                if (_waitWake->Wake(slot) != ESPressio::Threading::BoundedWaitWakeResult::Woken) {
                    _wakeFailures.fetch_add(
                        1U,
                        std::memory_order_relaxed);
                }
            }
            WakeDrain();
        }

        /// Owns retirement of one outer acquisition's in-flight count.
        struct InflightScope final {
            /// Coordinator whose count was incremented before creating the guard.
            TrackedAdmissionCoordinator *Owner;

            // One guard retires one count.
            /// Associates this scope with the already-counted acquisition.
            explicit InflightScope(TrackedAdmissionCoordinator *owner) noexcept : Owner(owner) {}
            /// Copying would retire the same acquisition twice.
            InflightScope(const InflightScope &) = delete;
            /// Reassignment would change the acquisition owner.
            InflightScope &operator=(const InflightScope &) = delete;
            /// Retires the outer count and notifies the drain caller.
            ~InflightScope() noexcept {
                Owner->_inflight.fetch_sub(
                    1U,
                    std::memory_order_acq_rel);
                Owner->WakeDrain();
            }
        };

      public:
        // Coordinator lifetime and stable identity.
        /// Borrows the provider and binds the embedded admission lifecycle to it.
        explicit TrackedAdmissionCoordinator(TWaitWakeProvider &owner) noexcept : _waitWake(&owner), _lifecycle(owner) {}

        /// Copying would duplicate the wake and lease accounting owner.
        TrackedAdmissionCoordinator(const TrackedAdmissionCoordinator &) = delete;
        /// Assignment would replace the identity referenced by existing guards.
        TrackedAdmissionCoordinator &operator=(const TrackedAdmissionCoordinator &) = delete;
        /// Moving would invalidate the owner address retained by existing guards.
        TrackedAdmissionCoordinator(TrackedAdmissionCoordinator &&) = delete;
        /// Move assignment would replace the owner while guards can still refer to it.
        TrackedAdmissionCoordinator &operator=(TrackedAdmissionCoordinator &&) = delete;

        /// RAII owner of one native Memory lease and its release accounting.
        /// The notifier precedes the native lease so it is destroyed after that lease.
        /// @tparam TLease Default-constructible Memory lease with noexcept release and empty queries.
        template <class TLease>
        class TrackedLease final {
            /// Sends a final advisory wake after an unconfirmed native release.
            struct NativeDestructorNotifier final {
                // Notification target and reason.
                /// Coordinator owning the producer and drain wake slots.
                TrackedAdmissionCoordinator *Owner;
                /// Whether native destruction may have released a tracked lease.
                bool MayHaveReleasedCapacity{false};

                /// Issues advisory wakes after native lease destruction if required.
                ~NativeDestructorNotifier() noexcept {
                    if (MayHaveReleasedCapacity) {
                        Owner->WakeAfterNativeLeaseDestruction();
                    }
                }
            };

            // Lease ownership and destruction order.
            /// Runs after the native lease destructor because members unwind in reverse order.
            NativeDestructorNotifier _postNative;
            /// Native lease kept under the guard's exclusive ownership.
            TLease _lease{};
            /// Coordinator that may acquire and account for this lease.
            TrackedAdmissionCoordinator *_owner;
            /// Whether a grant is still counted as outstanding.
            bool _tracked{false};

            friend class TrackedAdmissionCoordinator;

          public:
            // Guard lifetime and stable coordinator binding.
            /// Creates an empty guard tied to one coordinator.
            explicit TrackedLease(TrackedAdmissionCoordinator &owner) noexcept : _postNative{&owner}, _owner(&owner) {
                static_assert(std::is_default_constructible_v<TLease>);
                static_assert(noexcept(std::declval<TLease &>().Release()));
                static_assert(noexcept(std::declval<const TLease &>().IsEmpty()));
            }

            /// Copying would duplicate native lease ownership.
            TrackedLease(const TrackedLease &) = delete;
            /// Copy assignment would bypass coordinator accounting.
            TrackedLease &operator=(const TrackedLease &) = delete;
            /// Moving would transfer the bound guard outside the acquisition path.
            TrackedLease(TrackedLease &&) = delete;
            /// Move assignment would overwrite the destination's tracked lease.
            TrackedLease &operator=(TrackedLease &&) = delete;

            /// Attempts release and preserves uncertainty if the native provider fails.
            ~TrackedLease() noexcept {
                if (_tracked) {
                    const auto outcome = Release();
                    if (outcome == ESPressio::Logging::CoordinatedReleaseResult::MemoryProviderFailure) {
                        // Native ObjectPoolLease::~ObjectPoolLease will make
                        // another attempt AFTER this destructor body. Its
                        // outcome cannot be observed; therefore conservatively
                        // retain outstanding accounting and report uncertainty.
                        _owner->_unconfirmedReleases.fetch_add(
                            1U,
                            std::memory_order_relaxed);
                        _postNative.MayHaveReleasedCapacity = true;
                    }
                }
            }

            // Access while the guard owns the Memory lease.
            /// Reports whether the native lease is empty.
            [[nodiscard]] bool IsEmpty() const noexcept { return _lease.IsEmpty(); }
            /// Borrows the mutable object for the lifetime of the live lease.
            [[nodiscard]] auto Get() noexcept { return _lease.Get(); }
            /// Borrows the object through the native lease's const access path.
            [[nodiscard]] auto Get() const noexcept { return _lease.Get(); }

            /// Releases native ownership and updates tracking only after confirmed release.
            [[nodiscard]] CoordinatedReleaseResult Release() noexcept {
                if (!_tracked) return CoordinatedReleaseResult::AlreadyEmpty;

                const auto status = _owner->_lifecycle.ReleaseAndNotify(_lease);
                if (
                    status == CoordinatedReleaseResult::ReleasedAndNotified ||
                    status == CoordinatedReleaseResult::ReleasedNotificationFailed) {
                    _tracked = false;
                    _owner->_outstanding.fetch_sub(
                        1U,
                        std::memory_order_acq_rel);
                    if (status == CoordinatedReleaseResult::ReleasedNotificationFailed) {
                        _owner->_wakeFailures.fetch_add(
                            1U,
                            std::memory_order_relaxed);
                    }
                    _owner->WakeDrain();
                }
                return status;
            }
        };

        /// Claims a Memory lease using the canonical deadline and records every granted outcome.
        /// @tparam TMaximumWakeRetries Finite wake/retry limit for the lifecycle coordinator.
        /// @tparam TObjectPool Bootstrap-owned Memory pool providing the native acquisition.
        /// @tparam TLease Native lease type managed by the supplied output guard.
        /// @tparam TArguments Object construction arguments forwarded to the pool.
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
            _inflight.fetch_add(
                1U,
                std::memory_order_acq_rel);
            InflightScope scope{this};
            const auto result = _lifecycle.template AcquireUntil<TMaximumWakeRetries>(
                slot,
                pool,
                guardedLease._lease,
                deadline,
                std::forward<TArguments>(arguments)...);
            if (
                result.Admission == AdmissionWaitResult::Admitted ||
                result.Admission == AdmissionWaitResult::GrantedAfterDeadline ||
                result.Admission == AdmissionWaitResult::GrantedDuringShutdown) {
                _outstanding.fetch_add(
                    1U,
                    std::memory_order_acq_rel);
                guardedLease._tracked = true;
            }
            return {TrackedAdmissionValidation::Valid, result};
        }

        // Shutdown initiation and individually sampled accounting.
        /// Requests terminal admission shutdown and returns the producer wake outcome.
        [[nodiscard]] AdmissionWakePublicationResult RequestShutdown() noexcept {
            return _lifecycle.RequestShutdown();
        }

        /// Samples the number of granted leases awaiting confirmed release.
        [[nodiscard]] std::uint32_t OutstandingLeases() const noexcept {
            return _outstanding.load(std::memory_order_acquire);
        }

        /// Samples the sticky count of unconfirmed native release outcomes.
        [[nodiscard]] std::uint32_t UnconfirmedReleases() const noexcept {
            return _unconfirmedReleases.load(std::memory_order_acquire);
        }

        /// Finite, single-caller drain of both active acquisition operations
        /// and all *tracked* leases. It does NOT join Tasks or track leases
        /// issued by another admission owner.
        /// @tparam TMaximumDrainWakes Positive limit on waits by the single drain caller.
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
                const auto result = _waitWake->WaitUntil(
                    DrainSlot,
                    deadline);
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
