#pragma once

#include <ESPressio_Clock.hpp>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

#include <ESPressio_BoundedTopology.hpp>
#include <ESPressio_Platform.hpp>
#include <ESPressio_Threading.hpp>

#include "AdmissionWakeCoordinator.hpp"

namespace ESPressio::Logging {

    /// Results distinguish a transactional transition from the health of the
    /// required native lease release and producer wake publication.
    enum class RecordQueueResult : std::uint8_t {
        Succeeded = 0U,
        Empty = 1U,
        CapacityUnavailable = 2U,
        Busy = 3U,
        InvalidTicket = 4U,
        WrongPhase = 5U,
        OutputLeaseEmpty = 6U,
        GenerationExhausted = 7U,
        ReleasedNotificationFailed = 8U,
        ReleaseUnconfirmed = 9U,
        IntegrityFailure = 10U,
        OutputHandleOccupied = 11U,
        /// No transition was started, or retirement finalization is still owned, at the canonical
        /// deadline.
        DeadlineElapsed = 12U
    };

    /// Logging-specific transactional ordering of already acquired Memory leases.
    ///
    /// Storage/lifetime: EDP-Memory owns the object and its move-only lease.
    /// Topology: EDP-BoundedTopology owns compact slot occupancy and FIFO links.
    /// Mutual exclusion: Bootstrap supplies an EDP-Threading ordinary Mutex.
    /// Wake publication: the existing Logging admission coordinator delegates to
    /// Bootstrap's EDP-Threading BoundedWaitWake provider.
    ///
    /// This class does NOT perform pool acquisition, define a second waiter
    /// registry or invent a Clock. The producer obtains a Memory lease through
    /// the existing bounded admission path BEFORE Begin; Begin transfers that
    /// lease into a stable queue slot. Thereafter only the ticket holder may
    /// commit/abort or consume/complete a particular generation.
    ///
    /// The queue is not destroyed while any reservation, publication, consumer,
    /// or uncertain release remains active. Destructor fails closed if violated.
    /// @tparam TCapacity Compile-time maximum active source slots.
    /// @tparam TLease Existing EDP-Memory move-only source lease.
    /// @tparam TMutexProvider Existing ordinary Threading mutex supplied at Bootstrap.
    /// @tparam TReleaseCoordinator Existing Memory release and producer wake owner.
    /// @tparam TGeneration Unsigned non-rewinding slot-generation representation.
    template <std::size_t TCapacity,
              class TLease,
              class TMutexProvider,
              class TReleaseCoordinator,
              class TGeneration = std::uint32_t>
    class TransactionalRecordQueue final {
        static_assert(TCapacity > 0U, "Logging queue needs a positive static bound");
        static_assert(std::is_integral_v<TGeneration> && std::is_unsigned_v<TGeneration>);
        static_assert(std::is_default_constructible_v<TLease>);
        static_assert(std::is_nothrow_move_assignable_v<TLease>);
        static_assert(noexcept(std::declval<const TLease &>().IsEmpty()));
        static_assert(noexcept(std::declval<TLease &>().Release()));

        struct RecordIndexSpace final {};
        using Topology =
            ESPressio::BoundedTopology::BoundedSlotTopology<RecordIndexSpace, TCapacity>;
        using Queue = ESPressio::BoundedTopology::IntrusiveQueue<RecordIndexSpace, TCapacity>;

      public:
        /// Compact slot index in this queue's owner-specific topology.
        using Index = typename Queue::Index;

        /// Identifies one slot generation for validation under the queue owner.
        struct Ticket final {
            // Slot identity and generation must travel together.
            /// Slot of the associated record or the invalid sentinel.
            Index Slot = Index::Invalid();
            /// Nonzero generation assigned when the slot was reserved.
            TGeneration Generation = 0U;

            /// Tests ticket shape. Queue transitions also validate the live phase and generation.
            [[nodiscard]] bool IsValid() const noexcept {
                return Slot.IsValid() && Generation != 0U;
            }
        };

        /// Move-only owner of an unpublished reservation. Its destructor
        /// aborts the Memory lease unless Commit disarms this handle.
        class Reservation final {
            friend class TransactionalRecordQueue;

            /// Queue owner retained until the matching transition and release are confirmed.
            TransactionalRecordQueue *_owner{nullptr};
            /// Exclusive slot/generation proof for this handle's lifetime.
            Ticket _ticket{};
            /// Stable lease view established under the queue owner, never a concurrent
            /// occupancy-bitmap read.
            TLease *_borrowedLease{nullptr};

            void AbandonOnScopeExit() noexcept {
                if (!_owner)
                    return;
                auto *owner = _owner;
                const auto status = Abort();
                if (status != RecordQueueResult::Succeeded &&
                    status != RecordQueueResult::ReleasedNotificationFailed) {
                    // Do not silently allow Bootstrap to tear down an owner
                    // whose required native lease release was not verified.
                    owner->_integrityFailed.store(
                        true,
                        std::memory_order_release);
                }
            }

          public:
            Reservation() noexcept = default;
            Reservation(const Reservation &) = delete;
            Reservation &operator=(const Reservation &) = delete;
            Reservation(Reservation &&other) noexcept
                : _owner(std::exchange(
                      other._owner,
                      nullptr)),
                  _ticket(std::exchange(
                      other._ticket,
                      Ticket{})),
                  _borrowedLease(std::exchange(
                      other._borrowedLease,
                      nullptr)) {}
            Reservation &operator=(Reservation &&) = delete;
            ~Reservation() noexcept {
                AbandonOnScopeExit();
            }

            [[nodiscard]] bool IsActive() const noexcept {
                return _owner != nullptr;
            }

            [[nodiscard]] TLease *Borrow() noexcept {
                return _owner && !_owner->IsIntegrityFailed() ? _borrowedLease : nullptr;
            }

            [[nodiscard]] RecordQueueResult Commit() noexcept {
                if (!_owner)
                    return RecordQueueResult::InvalidTicket;
                const auto outcome = _owner->Commit(_ticket);
                if (outcome == RecordQueueResult::Succeeded) {
                    _owner = nullptr;
                    _borrowedLease = nullptr;
                }
                return outcome;
            }

            /// Publishes using the original caller deadline and retains this reservation on every
            /// non-success.
            [[nodiscard]] RecordQueueResult CommitUntil(
                ESPressio::Clock::MonotonicTimestamp deadline) noexcept {
                if (!_owner) {
                    return RecordQueueResult::InvalidTicket;
                }
                const auto outcome = _owner->CommitUntil(
                    _ticket,
                    deadline);
                if (outcome == RecordQueueResult::Succeeded) {
                    _owner = nullptr;
                    _borrowedLease = nullptr;
                }
                return outcome;
            }

            /// Explicit bounded abort; a timed-out or unconfirmed retirement remains owned.
            [[nodiscard]] RecordQueueResult AbortUntil(
                ESPressio::Clock::MonotonicTimestamp deadline) noexcept {
                if (!_owner) {
                    return RecordQueueResult::InvalidTicket;
                }
                _borrowedLease = nullptr;
                const auto outcome = _owner->AbortUntil(
                    _ticket,
                    deadline);
                if (outcome == RecordQueueResult::Succeeded ||
                    outcome == RecordQueueResult::ReleasedNotificationFailed) {
                    _owner = nullptr;
                }
                return outcome;
            }

            [[nodiscard]] RecordQueueResult Abort() noexcept {
                if (!_owner)
                    return RecordQueueResult::InvalidTicket;
                _borrowedLease = nullptr;
                const auto outcome = _owner->Abort(_ticket);
                if (outcome == RecordQueueResult::Succeeded ||
                    outcome == RecordQueueResult::ReleasedNotificationFailed) {
                    _owner = nullptr;
                    _borrowedLease = nullptr;
                }
                return outcome;
            }
        };

        /// Move-only exclusive consumer interest. Final source reader releases
        /// through the original Memory/Threading coordinator on completion.
        class ConsumerClaim final {
            friend class TransactionalRecordQueue;

            /// Queue owner retained until the matching transition and release are confirmed.
            TransactionalRecordQueue *_owner{nullptr};
            /// Exclusive slot/generation proof for this handle's lifetime.
            Ticket _ticket{};
            /// Stable lease view established under the queue owner, never a concurrent
            /// occupancy-bitmap read.
            TLease *_borrowedLease{nullptr};

            void CompleteOnScopeExit() noexcept {
                if (!_owner)
                    return;
                auto *owner = _owner;
                const auto status = Complete();
                if (status != RecordQueueResult::Succeeded &&
                    status != RecordQueueResult::ReleasedNotificationFailed) {
                    owner->_integrityFailed.store(
                        true,
                        std::memory_order_release);
                }
            }

          public:
            ConsumerClaim() noexcept = default;
            ConsumerClaim(const ConsumerClaim &) = delete;
            ConsumerClaim &operator=(const ConsumerClaim &) = delete;
            ConsumerClaim(ConsumerClaim &&other) noexcept
                : _owner(std::exchange(
                      other._owner,
                      nullptr)),
                  _ticket(std::exchange(
                      other._ticket,
                      Ticket{})),
                  _borrowedLease(std::exchange(
                      other._borrowedLease,
                      nullptr)) {}
            ConsumerClaim &operator=(ConsumerClaim &&) = delete;
            ~ConsumerClaim() noexcept {
                CompleteOnScopeExit();
            }

            [[nodiscard]] bool IsActive() const noexcept {
                return _owner != nullptr;
            }

            [[nodiscard]] TLease *Borrow() noexcept {
                return _owner && !_owner->IsIntegrityFailed() ? _borrowedLease : nullptr;
            }

            /// Completes this source exactly once under one canonical retirement budget.
            [[nodiscard]] RecordQueueResult CompleteUntil(
                ESPressio::Clock::MonotonicTimestamp deadline) noexcept {
                if (!_owner) {
                    return RecordQueueResult::InvalidTicket;
                }
                _borrowedLease = nullptr;
                const auto outcome = _owner->CompleteUntil(
                    _ticket,
                    deadline);
                if (outcome == RecordQueueResult::Succeeded ||
                    outcome == RecordQueueResult::ReleasedNotificationFailed) {
                    _owner = nullptr;
                }
                return outcome;
            }

            [[nodiscard]] RecordQueueResult Complete() noexcept {
                if (!_owner)
                    return RecordQueueResult::InvalidTicket;
                _borrowedLease = nullptr;
                const auto outcome = _owner->Complete(_ticket);
                if (outcome == RecordQueueResult::Succeeded ||
                    outcome == RecordQueueResult::ReleasedNotificationFailed) {
                    _owner = nullptr;
                    _borrowedLease = nullptr;
                }
                return outcome;
            }
        };

      private:
        /// Queue phase values remain stable for diagnostics.
        enum class Phase : std::uint8_t {
            Vacant = 0U,
            AwaitingMemory = 1U,
            Reserved = 2U,
            Queued = 3U,
            InFlight = 4U,
            Retiring = 5U,
            Exhausted = 6U
        };

        /// Native mutex release is an operation, not a Boolean predicate.
        enum class UnlockStatus : std::uint8_t {
            /// Native mutex ownership was released successfully.
            Released = 0U,
            /// Native release could not be confirmed; queue integrity is quarantined.
            Unconfirmed = 1U
        };

        struct Record final {
            Index Next = Index::Invalid();
            TLease Lease{};
            TGeneration Generation = 0U;
            Phase State = Phase::Vacant;
            // Only one caller may release the source Memory lease. This
            // prevents two copies of one ticket from racing the native owner.
            std::atomic<bool> RetirementClaimed{false};

            /// A confirmed release notification failure survives a later finalization timeout.
            bool RetirementNotificationFailed{false};

            [[nodiscard]] Index QueueNext() const noexcept {
                return Next;
            }
            void SetQueueNext(Index next) noexcept {
                Next = next;
            }
        };

        TMutexProvider *_mutex;
        TReleaseCoordinator *_releases;
        Topology _slots{};
        Queue _fifo{};
        std::array<Record, TCapacity> _records{};
        std::atomic<bool> _integrityFailed{false};

        [[nodiscard]] RecordQueueResult Lock() noexcept {
            if (_integrityFailed.load(std::memory_order_acquire)) {
                return RecordQueueResult::IntegrityFailure;
            }
            const auto acquired = _mutex->TryAcquire();
            if (acquired == ESPressio::Threading::OrdinaryMutexAcquireResult::TimedOut) {
                return RecordQueueResult::Busy;
            }
            if (acquired != ESPressio::Threading::OrdinaryMutexAcquireResult::Acquired) {
                _integrityFailed.store(
                    true,
                    std::memory_order_release);
                return RecordQueueResult::IntegrityFailure;
            }
            if (_integrityFailed.load(std::memory_order_acquire)) {
                static_cast<void>(Unlock());
                return RecordQueueResult::IntegrityFailure;
            }
            return RecordQueueResult::Succeeded;
        }

        /// Confirms release or quarantines the queue when native unlock is uncertain.
        [[nodiscard]] UnlockStatus Unlock() noexcept {
            if (_mutex->Release() != ESPressio::Threading::OrdinaryMutexReleaseResult::Released) {
                _integrityFailed.store(
                    true,
                    std::memory_order_release);
                return UnlockStatus::Unconfirmed;
            }
            return UnlockStatus::Released;
        }

        /// Reserve mutex using an already-computed native remaining budget;
        /// the caller owns the canonical Clock deadline and never restarts it.
        [[nodiscard]] RecordQueueResult LockWith(
            ESPressio::Platform::Synchronization::WaitTimeout remaining) noexcept {
            if (remaining.IsNoWait())
                return Lock();
            if (_integrityFailed.load(std::memory_order_acquire)) {
                return RecordQueueResult::IntegrityFailure;
            }
            const auto acquired = _mutex->Acquire(remaining);
            if (acquired == ESPressio::Threading::OrdinaryMutexAcquireResult::TimedOut) {
                return RecordQueueResult::Busy;
            }
            if (acquired != ESPressio::Threading::OrdinaryMutexAcquireResult::Acquired) {
                _integrityFailed.store(
                    true,
                    std::memory_order_release);
                return RecordQueueResult::IntegrityFailure;
            }
            if (_integrityFailed.load(std::memory_order_acquire)) {
                static_cast<void>(Unlock());
                return RecordQueueResult::IntegrityFailure;
            }
            return RecordQueueResult::Succeeded;
        }

        /// Reuses the same ordinary mutex, rejecting a late native grant before any queue mutation.
        [[nodiscard]] RecordQueueResult LockUntil(
            ESPressio::Clock::MonotonicTimestamp deadline) noexcept {
            if (_integrityFailed.load(std::memory_order_acquire)) {
                return RecordQueueResult::IntegrityFailure;
            }
            if (ESPressio::Clock::MonotonicNow() >= deadline) {
                return RecordQueueResult::DeadlineElapsed;
            }
            const auto acquired = LockWith(Detail::RemainingRecordAdmissionWait(deadline));
            if (acquired != RecordQueueResult::Succeeded) {
                if (acquired == RecordQueueResult::Busy &&
                    ESPressio::Clock::MonotonicNow() >= deadline) {
                    return RecordQueueResult::DeadlineElapsed;
                }
                return acquired;
            }
            if (ESPressio::Clock::MonotonicNow() >= deadline) {
                return UnlockWith(RecordQueueResult::DeadlineElapsed);
            }
            return RecordQueueResult::Succeeded;
        }

        [[nodiscard]] RecordQueueResult UnlockWith(RecordQueueResult outcome) noexcept {
            return Unlock() == UnlockStatus::Released ? outcome
                                                      : RecordQueueResult::IntegrityFailure;
        }

        /// The single compact slot-allocation algorithm shared by native
        /// Begin and ahead-of-Memory slot reservation. Called under mutex.
        [[nodiscard]] RecordQueueResult ClaimSlot(Phase initial, Ticket &ticket) noexcept {
            bool exhausted = false;
            for (std::size_t i = 0U; i < TCapacity; ++i) {
                const auto claim = _slots.Acquire();
                if (!claim.IsSucceeded()) {
                    return exhausted ? RecordQueueResult::GenerationExhausted
                                     : RecordQueueResult::CapacityUnavailable;
                }
                const auto index = claim.AcquiredIndex();
                auto &record = _records[static_cast<std::size_t>(index.Value())];
                if (record.Generation == std::numeric_limits<TGeneration>::max()) {
                    record.State = Phase::Exhausted;
                    exhausted = true;
                    continue;
                }
                ++record.Generation;
                record.RetirementNotificationFailed = false;
                record.State = initial;
                ticket = Ticket{index, record.Generation};
                return RecordQueueResult::Succeeded;
            }
            return RecordQueueResult::GenerationExhausted;
        }

        [[nodiscard]] bool IsMatchingTicket(Ticket ticket) const noexcept {
            return ticket.IsValid() && _slots.IsOccupied(ticket.Slot) &&
                   _records[static_cast<std::size_t>(ticket.Slot.Value())].Generation ==
                       ticket.Generation;
        }

        [[nodiscard]] static constexpr bool IsPhaseLive(Phase phase) noexcept {
            return phase == Phase::AwaitingMemory || phase == Phase::Reserved ||
                   phase == Phase::Queued || phase == Phase::InFlight || phase == Phase::Retiring;
        }

        /// Releases outside the queue mutex, then finalizes under the SAME deadline without double
        /// release.
        [[nodiscard]] RecordQueueResult Retire(
            Ticket ticket,
            Phase expected,
            std::optional<ESPressio::Clock::MonotonicTimestamp> deadline = std::nullopt) noexcept {
            auto outcome = deadline.has_value() ? LockUntil(*deadline) : Lock();
            if (outcome != RecordQueueResult::Succeeded)
                return outcome;

            if (!IsMatchingTicket(ticket)) {
                return UnlockWith(RecordQueueResult::InvalidTicket);
            }
            auto &record = _records[static_cast<std::size_t>(ticket.Slot.Value())];
            if (record.State != expected && record.State != Phase::Retiring &&
                !(expected == Phase::Reserved && record.State == Phase::AwaitingMemory)) {
                return UnlockWith(RecordQueueResult::WrongPhase);
            }
            if (record.RetirementClaimed.exchange(
                    true,
                    std::memory_order_acq_rel)) {
                return UnlockWith(RecordQueueResult::Busy);
            }
            record.State = Phase::Retiring;
            if (Unlock() != UnlockStatus::Released)
                return RecordQueueResult::IntegrityFailure;

            // Never call native Memory release, user code, or Threading wake
            // while holding the queue mutex. A failed release retains this
            // stable Retiring slot, which cannot be recycled or published.
            bool notified = !record.RetirementNotificationFailed;
            if (!record.Lease.IsEmpty()) {
                const auto released = _releases->ReleaseAndNotify(record.Lease);
                if (released != CoordinatedReleaseResult::ReleasedAndNotified &&
                    released != CoordinatedReleaseResult::ReleasedNotificationFailed) {
                    if (record.Lease.IsEmpty()) {
                        // The provider denied a confirmed release, yet the
                        // lease no longer proves ownership. Quarantine.
                        _integrityFailed.store(
                            true,
                            std::memory_order_release);
                        return RecordQueueResult::IntegrityFailure;
                    }
                    record.RetirementClaimed.store(
                        false,
                        std::memory_order_release);
                    return RecordQueueResult::ReleaseUnconfirmed;
                }
                notified = notified && released == CoordinatedReleaseResult::ReleasedAndNotified;
                record.RetirementNotificationFailed = !notified;
                if (!record.Lease.IsEmpty()) {
                    _integrityFailed.store(
                        true,
                        std::memory_order_release);
                    return RecordQueueResult::IntegrityFailure;
                }
            }

            outcome = deadline.has_value() ? LockUntil(*deadline) : Lock();
            if (outcome == RecordQueueResult::Busy ||
                outcome == RecordQueueResult::DeadlineElapsed) {
                // Lease is empty; a later ticket holder can only finalize,
                // and must never release the native Memory object twice.
                record.RetirementClaimed.store(
                    false,
                    std::memory_order_release);
                return outcome;
            }
            if (outcome != RecordQueueResult::Succeeded)
                return outcome;
            if (!IsMatchingTicket(ticket) || record.State != Phase::Retiring ||
                _slots.Release(ticket.Slot) !=
                    ESPressio::BoundedTopology::BoundedSlotReleaseResult::Succeeded) {
                _integrityFailed.store(
                    true,
                    std::memory_order_release);
                static_cast<void>(Unlock());
                return RecordQueueResult::IntegrityFailure;
            }
            record.State = Phase::Vacant;
            record.RetirementClaimed.store(
                false,
                std::memory_order_release);
            if (Unlock() != UnlockStatus::Released)
                return RecordQueueResult::IntegrityFailure;

            // Queue-slot availability is distinct from Memory pool capacity.
            // Publish only AFTER slot retirement so a racing producer cannot
            // miss the sole capacity-release notification.
            if (_releases->PublishCapacityAvailable() !=
                AdmissionWakePublicationResult::Published) {
                notified = false;
            }
            return notified ? RecordQueueResult::Succeeded
                            : RecordQueueResult::ReleasedNotificationFailed;
        }

        /// Transaction commit publishes the already-owned record to FIFO once.
        [[nodiscard]] RecordQueueResult CommitUnderPolicy(
            Ticket ticket, std::optional<ESPressio::Clock::MonotonicTimestamp> deadline) noexcept {
            auto outcome = deadline.has_value() ? LockUntil(*deadline) : Lock();
            if (outcome != RecordQueueResult::Succeeded)
                return outcome;
            if (!IsMatchingTicket(ticket)) {
                return UnlockWith(RecordQueueResult::InvalidTicket);
            }
            auto &record = _records[static_cast<std::size_t>(ticket.Slot.Value())];
            if (record.State != Phase::Reserved) {
                return UnlockWith(RecordQueueResult::WrongPhase);
            }
            if (_fifo.Push(
                    _records,
                    ticket.Slot) !=
                ESPressio::BoundedTopology::IntrusiveQueuePushResult::Succeeded) {
                _integrityFailed.store(
                    true,
                    std::memory_order_release);
                static_cast<void>(Unlock());
                return RecordQueueResult::IntegrityFailure;
            }
            record.State = Phase::Queued;
            return Unlock() == UnlockStatus::Released ? RecordQueueResult::Succeeded
                                                      : RecordQueueResult::IntegrityFailure;
        }

        /// Claims the oldest committed record for exclusive consumer handling.
        [[nodiscard]] RecordQueueResult PopUnderPolicy(
            Ticket &ticket, std::optional<ESPressio::Clock::MonotonicTimestamp> deadline) noexcept {
            auto outcome = deadline.has_value() ? LockUntil(*deadline) : Lock();
            if (outcome != RecordQueueResult::Succeeded)
                return outcome;
            Index index = Index::Invalid();
            if (_fifo.Pop(
                    _records,
                    index) ==
                ESPressio::BoundedTopology::IntrusiveQueuePopResult::Empty) {
                return UnlockWith(RecordQueueResult::Empty);
            }
            auto &record = _records[static_cast<std::size_t>(index.Value())];
            if (record.State != Phase::Queued) {
                _integrityFailed.store(
                    true,
                    std::memory_order_release);
                static_cast<void>(Unlock());
                return RecordQueueResult::IntegrityFailure;
            }
            record.State = Phase::InFlight;
            ticket = Ticket{index, record.Generation};
            return Unlock() == UnlockStatus::Released ? RecordQueueResult::Succeeded
                                                      : RecordQueueResult::IntegrityFailure;
        }

      public:
        static constexpr std::size_t Capacity = TCapacity;
        static constexpr std::size_t StorageBytes = sizeof(TransactionalRecordQueue);

        TransactionalRecordQueue(TMutexProvider &mutex, TReleaseCoordinator &releases) noexcept
            : _mutex(&mutex), _releases(&releases) {}

        TransactionalRecordQueue(const TransactionalRecordQueue &) = delete;
        TransactionalRecordQueue &operator=(const TransactionalRecordQueue &) = delete;

        ~TransactionalRecordQueue() noexcept {
            // An active Memory lease must never fall through native RAII without
            // the coordinator observing its actual release. It is a Bootstrap
            // lifecycle violation to destroy a queue before explicit drain.
            for (const auto &record : _records) {
                if (IsPhaseLive(record.State))
                    std::terminate();
            }
        }

        /// Takes one already-owned lease; output ticket is established before
        /// unlocking. Failed admission without a ticket leaves lease with caller.
        [[nodiscard]] RecordQueueResult Begin(TLease &lease, Ticket &ticket) noexcept {
            if (lease.IsEmpty())
                return RecordQueueResult::OutputLeaseEmpty;
            auto outcome = Lock();
            if (outcome != RecordQueueResult::Succeeded)
                return outcome;

            Ticket candidate{};
            const auto claimed = ClaimSlot(
                Phase::Reserved,
                candidate);
            if (claimed == RecordQueueResult::Succeeded) {
                _records[static_cast<std::size_t>(candidate.Slot.Value())].Lease = std::move(lease);
                ticket = candidate;
            }
            return UnlockWith(claimed);
        }

        /// Strong RAII form: assigns one inactive move-only unpublished owner.
        /// All failure paths that retain caller Memory ownership leave out
        /// inactive. If a native mutex release failed after lease transfer,
        /// out still retains the ticket so the error cannot be misread as
        /// failure-before-ownership-transfer.
        [[nodiscard]] RecordQueueResult Begin(TLease &lease, Reservation &out) noexcept {
            if (out.IsActive())
                return RecordQueueResult::OutputHandleOccupied;
            Ticket ticket{};
            const auto result = Begin(
                lease,
                ticket);
            if (ticket.IsValid() && lease.IsEmpty()) {
                out._owner = this;
                out._ticket = ticket;
                out._borrowedLease = &_records[static_cast<std::size_t>(ticket.Slot.Value())].Lease;
            }
            return result;
        }

        /// Reserve the queue slot first, before any potentially long Memory
        /// pool acquisition. This avoids holding a scarce pool lease while
        /// waiting for FIFO admission. Mutex remaining is derived by the
        /// higher-domain canonical EDP-Clock deadline owner.
        [[nodiscard]] RecordQueueResult ReserveSlot(
            Reservation &out,
            ESPressio::Platform::Synchronization::WaitTimeout mutexRemaining) noexcept {
            if (out.IsActive())
                return RecordQueueResult::OutputHandleOccupied;
            const auto acquired = LockWith(mutexRemaining);
            if (acquired != RecordQueueResult::Succeeded)
                return acquired;
            Ticket candidate{};
            const auto claimed = ClaimSlot(
                Phase::AwaitingMemory,
                candidate);
            const auto released = UnlockWith(claimed);
            if (candidate.IsValid()) {
                out._owner = this;
                out._ticket = candidate;
                out._borrowedLease = nullptr;
            }
            return released;
        }

        /// Attach one real owned Memory lease to a previously reserved queue
        /// slot. A failed lock attempt leaves the lease with the caller.
        [[nodiscard]] RecordQueueResult AttachLease(
            Reservation &out,
            TLease &lease,
            ESPressio::Platform::Synchronization::WaitTimeout mutexRemaining) noexcept {
            if (!out.IsActive() || out._owner != this) {
                return RecordQueueResult::InvalidTicket;
            }
            if (lease.IsEmpty())
                return RecordQueueResult::OutputLeaseEmpty;
            const auto acquired = LockWith(mutexRemaining);
            if (acquired != RecordQueueResult::Succeeded)
                return acquired;
            if (!IsMatchingTicket(out._ticket)) {
                return UnlockWith(RecordQueueResult::InvalidTicket);
            }
            auto &record = _records[static_cast<std::size_t>(out._ticket.Slot.Value())];
            if (record.State != Phase::AwaitingMemory) {
                return UnlockWith(RecordQueueResult::WrongPhase);
            }
            record.Lease = std::move(lease);
            record.State = Phase::Reserved;
            out._borrowedLease = &record.Lease;
            return UnlockWith(RecordQueueResult::Succeeded);
        }

        /// Original no-wait commit behavior remains available.
        [[nodiscard]] RecordQueueResult Commit(Ticket ticket) noexcept {
            return CommitUnderPolicy(
                ticket,
                std::nullopt);
        }

        /// Bounded commit never publishes after its original canonical deadline.
        [[nodiscard]] RecordQueueResult CommitUntil(
            Ticket ticket, ESPressio::Clock::MonotonicTimestamp deadline) noexcept {
            return CommitUnderPolicy(
                ticket,
                deadline);
        }

        /// Aborts an unpublished reservation and returns its Memory capacity.
        [[nodiscard]] RecordQueueResult Abort(Ticket ticket) noexcept {
            return Retire(
                ticket,
                Phase::Reserved);
        }

        /// Bounded abort retains the reservation through timeout or unknown release.
        [[nodiscard]] RecordQueueResult AbortUntil(
            Ticket ticket, ESPressio::Clock::MonotonicTimestamp deadline) noexcept {
            return Retire(
                ticket,
                Phase::Reserved,
                deadline);
        }

        /// Original no-wait consumer claim behavior.
        [[nodiscard]] RecordQueueResult Pop(Ticket &ticket) noexcept {
            return PopUnderPolicy(
                ticket,
                std::nullopt);
        }

        /// Claims one source with an explicit canonical mutex deadline.
        [[nodiscard]] RecordQueueResult PopUntil(
            Ticket &ticket, ESPressio::Clock::MonotonicTimestamp deadline) noexcept {
            return PopUnderPolicy(
                ticket,
                deadline);
        }

        /// Strong RAII form: a successful Pop creates a move-only consuming
        /// handle. On normal scope exit it confirms Memory lease release.
        [[nodiscard]] RecordQueueResult Pop(ConsumerClaim &out) noexcept {
            if (out.IsActive())
                return RecordQueueResult::OutputHandleOccupied;
            Ticket ticket{};
            const auto result = Pop(ticket);
            if (ticket.IsValid()) {
                out._owner = this;
                out._ticket = ticket;
                out._borrowedLease = &_records[static_cast<std::size_t>(ticket.Slot.Value())].Lease;
            }
            return result;
        }

        /// RAII bounded claim binds its stable lease view while the generation is exclusive.
        [[nodiscard]] RecordQueueResult PopUntil(
            ConsumerClaim &out, ESPressio::Clock::MonotonicTimestamp deadline) noexcept {
            if (out.IsActive()) {
                return RecordQueueResult::OutputHandleOccupied;
            }
            Ticket ticket{};
            const auto result = PopUntil(
                ticket,
                deadline);
            if (ticket.IsValid()) {
                out._owner = this;
                out._ticket = ticket;
                out._borrowedLease = &_records[static_cast<std::size_t>(ticket.Slot.Value())].Lease;
            }
            return result;
        }

        /// Borrow the source's Memory lease while holding the exclusive
        /// Reserved or InFlight ticket. Never transfer this reference to a
        /// concurrent actor without an explicit ownership transfer protocol.
        [[nodiscard]] TLease *Borrow(Ticket ticket) noexcept {
            if (Lock() != RecordQueueResult::Succeeded) {
                return nullptr;
            }
            TLease *borrowed = nullptr;
            if (IsMatchingTicket(ticket)) {
                auto &record = _records[static_cast<std::size_t>(ticket.Slot.Value())];
                if (record.State == Phase::Reserved || record.State == Phase::InFlight) {
                    borrowed = &record.Lease;
                }
            }
            return Unlock() == UnlockStatus::Released ? borrowed : nullptr;
        }

        /// Completes consumption after the last source-record reader is done.
        [[nodiscard]] RecordQueueResult Complete(Ticket ticket) noexcept {
            return Retire(
                ticket,
                Phase::InFlight);
        }

        /// Bounded completion preserves release-once retirement if its final lock times out.
        [[nodiscard]] RecordQueueResult CompleteUntil(
            Ticket ticket, ESPressio::Clock::MonotonicTimestamp deadline) noexcept {
            return Retire(
                ticket,
                Phase::InFlight,
                deadline);
        }

        [[nodiscard]] bool IsIntegrityFailed() const noexcept {
            return _integrityFailed.load(std::memory_order_acquire);
        }
    };

} // namespace ESPressio::Logging
