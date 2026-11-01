#pragma once

#include <atomic>
#include <concurrency/AtomicWord32Contract.hpp>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Threading.hpp>

#include "AdmissionWakeCoordinator.hpp"

namespace ESPressio::Logging {


    namespace Detail {


        /// Selects an existing canonical atomic word without creating a provider.
        /// @tparam TAtomicWordProvider Bootstrap-selected Platform AtomicWord32 owner.
        template <class TAtomicWordProvider>
        struct AdmissionAtomicWordSelection {
            /// Validated existing capability storage.
            using Word = typename ESPressio::Platform::Concurrency::Detail::
                AtomicWord32ProviderTraits<TAtomicWordProvider>::Word;
        };

        /// Retains the original standard-atomic contract for existing two-argument users.
        template <>
        struct AdmissionAtomicWordSelection<void> {
            /// Existing standard word; the coordinator still requires always-lock-free support.
            using Word = std::atomic<std::uint32_t>;
        };

    } // namespace Detail

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
    /// @tparam TProducerCount Number of uniquely owned producer wake slots.
    /// @tparam TWaitWakeProvider Existing bounded Threading wake owner, with a drain slot.
    /// @tparam TAtomicWordProvider Optional canonical Platform AtomicWord32 provider;
    /// void preserves the original always-lock-free standard-atomic requirement.
    template <std::size_t TProducerCount, class TWaitWakeProvider, class TAtomicWordProvider = void>
    class AdmissionLifecycleCoordinator final {
        static_assert(TProducerCount > 0U, "At least one producer required");
        static_assert(
            TWaitWakeProvider::Capacity > TProducerCount,
            "One extra EDP-Threading slot is reserved for shutdown drain");
        static_assert(
            !std::is_void_v<TAtomicWordProvider> || std::atomic<std::uint32_t>::is_always_lock_free,
            "G1 admission lifecycle requires nonblocking atomic state");

        // Stable dependencies and finite lifecycle state.

        /// Existing wake owner; it outlives admission scopes and shutdown.
        TWaitWakeProvider* _waitWake;
        /// Existing capacity fanout and guarded Memory admission owner.
        AdmissionWakeCoordinator<TProducerCount, TWaitWakeProvider> _coordinator;
        /// Existing selected word representation, validated by its owning contract.
        using AtomicWord = typename Detail::AdmissionAtomicWordSelection<TAtomicWordProvider>::Word;
        /// Zero while open, one after shutdown publication.
        AtomicWord _shuttingDown{};
        /// Counts active producer operations, not outstanding accepted leases.
        AtomicWord _activeAdmissions{};
        /// Dedicated wake slot used only by the shutdown drainer.
        static constexpr std::size_t DrainSlot = TProducerCount;

        // Ordering over the selected existing word operations.

        /// Reads a lifecycle value with the same acquire ordering in both bindings.
        [[nodiscard]] static std::uint32_t LoadAcquire(const AtomicWord& word) noexcept {
            if constexpr (std::is_void_v<TAtomicWordProvider>) {
                return word.load(std::memory_order_acquire);
            } else {
                return word.LoadAcquire();
            }
        }

        /// Initializes an unpublished word without assuming a provider's default value.
        static void InitializeWord(AtomicWord& word) noexcept {
            if constexpr (std::is_void_v<TAtomicWordProvider>) {
                word.store(
                    0U,
                    std::memory_order_relaxed
                );
            } else {
                word.StoreRelaxed(0U);
            }
        }

        /// Publishes shutdown after prior writes, before waking producers.
        void PublishShutdown() noexcept {
            if constexpr (std::is_void_v<TAtomicWordProvider>) {
                _shuttingDown.store(
                    1U,
                    std::memory_order_release
                );
            } else {
                _shuttingDown.StoreRelease(1U);
            }
        }

        /// Registers/unregisters one scope using the existing atomic primitive.
        /// Failed CAS refreshes the observed value; another operation made progress.
        void ChangeActiveCount(std::int32_t adjustment) noexcept {
            if constexpr (std::is_void_v<TAtomicWordProvider>) {
                if (adjustment > 0) {
                    _activeAdmissions.fetch_add(
                        1U,
                        std::memory_order_acq_rel
                    );
                } else {
                    _activeAdmissions.fetch_sub(
                        1U,
                        std::memory_order_acq_rel
                    );
                }
            } else {
                auto observed = _activeAdmissions.LoadAcquire();
                while (!_activeAdmissions.CompareExchangeAcqRel(
                    observed,
                    observed + static_cast<std::uint32_t>(adjustment)
                )) {
                }
            }
        }

        /// Always decrements on each return, including deadline/provider error.
        /// All active calls retain the parent coordinator's lifetime.
        struct ActiveAdmissionScope final {
            /// Parent remains alive until this complete admission operation exits.
            AdmissionLifecycleCoordinator* Owner;

            /// Registers before queue waits or Memory acquisition may begin.
            explicit ActiveAdmissionScope(AdmissionLifecycleCoordinator* owner) noexcept
                : Owner(owner) {
                // Register before any queue wait or Memory acquisition can start.
                Owner->ChangeActiveCount(1);
            }

            /// Ownership of an active operation cannot be copied.
            ActiveAdmissionScope(const ActiveAdmissionScope&) = delete;
            /// Assignment cannot duplicate an active operation.
            ActiveAdmissionScope& operator=(const ActiveAdmissionScope&) = delete;

            /// Leaves once and wakes the existing drainer after shutdown.
            ~ActiveAdmissionScope() noexcept {
                Owner->ChangeActiveCount(-1);
                // The shutdown drainer may already be waiting. A latched wake
                // handles completion-before-registration and is also safe when
                // no shutdown is requested yet.
                if (Owner->IsShutdownRequested()) {
                    static_cast<void>(Owner->_waitWake->Wake(DrainSlot));
                }
            }
        };

      public:
        // Stable Bootstrap lifecycle.

        /// Binds the existing wake owner and initializes both unpublished words.
        explicit AdmissionLifecycleCoordinator(TWaitWakeProvider& owner) noexcept
            : _waitWake(&owner), _coordinator(owner) {
            InitializeWord(_shuttingDown);
            InitializeWord(_activeAdmissions);
        }

        /// Copying would duplicate live wake and scope ownership.
        AdmissionLifecycleCoordinator(const AdmissionLifecycleCoordinator&) = delete;
        /// Assignment cannot replace live lifecycle state.
        AdmissionLifecycleCoordinator& operator=(const AdmissionLifecycleCoordinator&) = delete;
        /// A lifecycle owner stays address-stable while scopes reference it.
        AdmissionLifecycleCoordinator(AdmissionLifecycleCoordinator&&) = delete;
        /// Moving cannot relocate live lifecycle state.
        AdmissionLifecycleCoordinator& operator=(AdmissionLifecycleCoordinator&&) = delete;

        // Read-only lifecycle observations.

        /// Indicates whether shutdown has been published.
        [[nodiscard]] bool IsShutdownRequested() const noexcept {
            return LoadAcquire(_shuttingDown) != 0U;
        }

        /// Returns in-flight operations without inferring accepted lease quiescence.
        [[nodiscard]] std::uint32_t ActiveAdmissions() const noexcept {
            return LoadAcquire(_activeAdmissions);
        }

        /// Valid producer slots exclude the dedicated shutdown-drainer slot.
        /// This must be checked before a higher-level queue-capacity wait.
        [[nodiscard]] static constexpr bool IsValidProducerSlot(
            std::size_t slot) noexcept {
            return slot < TProducerCount;
        }

        /// Track one complete producer transaction, including queue wait, Memory
        /// admission and rollback. The returned non-copyable scope must stay
        /// alive through every exit path; Bootstrap joins producer tasks before
        /// destroying this coordinator.
        [[nodiscard]] ActiveAdmissionScope TrackAdmission() noexcept {
            return ActiveAdmissionScope(this);
        }

        /// Prevents future grants by this coordinator and wakes all producers.
        ///
        /// A failed publication is explicitly returned. Caller must retain the
        /// coordinator alive and retry publication/escalate until the active
        /// workers either wake or expire by their pre-existing deadlines.
        [[nodiscard]] AdmissionWakePublicationResult RequestShutdown() noexcept {
            PublishShutdown();
            return _coordinator.PublishCapacityAvailable();
        }

        /// Exclusive per-producer slot. Admission retries stop as soon as the
        /// cancellation signal is observed, including following a latched wake.
        /// A grant racing the shutdown remains owned and is explicitly reported.
        /// @tparam TMaximumWakeRetries Finite capacity wake retry budget.
        /// @tparam TObjectPool Existing Memory object-pool owner.
        /// @tparam TLease Existing caller-owned lease output.
        /// @tparam TArguments Owned constructor values forwarded to Memory.
        template <
            std::size_t TMaximumWakeRetries,
            class TObjectPool,
            class TLease,
            class... TArguments>
        [[nodiscard]] CoordinatedAdmissionResult AcquireUntil(
            std::size_t slot,
            TObjectPool& pool,
            TLease& lease,
            ESPressio::Clock::MonotonicTimestamp deadline,
            TArguments&&... arguments) noexcept {
            if (slot >= TProducerCount) {
                return {
                    AdmissionWaitResult::InvalidProducerSlot,
                    AdmissionWakePublicationResult::NotAttempted};
            }

            // Count before examining shutdown. If shutdown is already set,
            // no pool claim can occur; this call only briefly enters/exits.
            ActiveAdmissionScope active{this};
            if (IsShutdownRequested()) {
                return {
                    AdmissionWaitResult::ShutdownRequested,
                    AdmissionWakePublicationResult::NotAttempted};
            }

            Detail::ThreadingWaitSlot<TWaitWakeProvider> waitPoint{_waitWake, slot};
            const auto stop = [this]() noexcept -> bool {
                return IsShutdownRequested();
            };
            const auto status = AcquirePoolUntilGuarded<TMaximumWakeRetries>(
                pool,
                waitPoint,
                lease,
                deadline,
                stop,
                std::forward<TArguments>(arguments)...
            );

            if (
                status == AdmissionWaitResult::Admitted ||
                status == AdmissionWaitResult::GrantedAfterDeadline ||
                status == AdmissionWaitResult::GrantedDuringShutdown) {
                return {status, _coordinator.PublishCapacityAvailable()};
            }
            return {status, AdmissionWakePublicationResult::NotAttempted};
        }

        /// Publish bounded capacity wake fanout for queue-slot retirement.
        /// This reuses the same exclusive producer wake slots as Memory admission.
        [[nodiscard]] AdmissionWakePublicationResult PublishCapacityAvailable() noexcept {
            return _coordinator.PublishCapacityAvailable();
        }

        /// Remains usable during shutdown to return outstanding Memory leases.
        /// @tparam TLease Existing owned lease returned through its Memory contract.
        template <class TLease>
        [[nodiscard]] CoordinatedReleaseResult ReleaseAndNotify(TLease& lease) noexcept {
            return _coordinator.ReleaseAndNotify(lease);
        }

        /// Bounded drain of *acquisition calls* with one absolute Clock deadline.
        /// One caller only; not reentrant. Provider can wake spuriously, therefore
        /// the compile-time wake budget prevents repeated immediate Wake storms.
        /// A Quiescent result is NOT evidence that accepted leases were returned
        /// or application producer tasks have joined; Bootstrap must ensure both.
        /// @tparam TMaximumDrainWakes Finite drainer wake budget under one deadline.
        template <std::size_t TMaximumDrainWakes>
        [[nodiscard]] AdmissionShutdownResult ShutdownUntil(
            ESPressio::Clock::MonotonicTimestamp deadline) noexcept {
            static_assert(TMaximumDrainWakes > 0U);
            const auto publication = RequestShutdown();
            for (std::size_t i = 0U; i < TMaximumDrainWakes; ++i) {
                const auto active = ActiveAdmissions();
                if (active == 0U) {
                    return {
                        AdmissionShutdownStatus::Quiescent,
                        publication,
                        0U};
                }
                if (ESPressio::Clock::MonotonicNow() >= deadline) {
                    return {
                        AdmissionShutdownStatus::DeadlineElapsed,
                        publication,
                        active};
                }

                const auto result = _waitWake->WaitUntil(
                    DrainSlot,
                    deadline
                );
                if (result == ESPressio::Threading::BoundedWaitWakeResult::TimedOut) {
                    return {
                        AdmissionShutdownStatus::DeadlineElapsed,
                        publication,
                        ActiveAdmissions()};
                }
                if (result != ESPressio::Threading::BoundedWaitWakeResult::Woken) {
                    return {
                        AdmissionShutdownStatus::DrainWaitProviderFailure,
                        publication,
                        ActiveAdmissions()};
                }
            }

            return {
                ActiveAdmissions() == 0U
                    ? AdmissionShutdownStatus::Quiescent
                    : AdmissionShutdownStatus::WakeRetryLimitReached,
                publication,
                ActiveAdmissions()};
        }
    };

} // namespace ESPressio::Logging
