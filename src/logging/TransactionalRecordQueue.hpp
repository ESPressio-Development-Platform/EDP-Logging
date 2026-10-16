#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <type_traits>
#include <utility>

#include <ESPressio_BoundedTopology.hpp>
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
        IntegrityFailure = 10U
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
    template<
        std::size_t TCapacity,
        class TLease,
        class TMutexProvider,
        class TReleaseCoordinator,
        class TGeneration = std::uint32_t
    >
    class TransactionalRecordQueue final {
        static_assert(TCapacity > 0U, "Logging queue needs a positive static bound");
        static_assert(std::is_integral_v<TGeneration> && std::is_unsigned_v<TGeneration>);
        static_assert(std::is_default_constructible_v<TLease>);
        static_assert(std::is_nothrow_move_assignable_v<TLease>);
        static_assert(noexcept(std::declval<const TLease&>().IsEmpty()));
        static_assert(noexcept(std::declval<TLease&>().Release()));

        struct RecordIndexSpace final {};
        using Topology = ESPressio::BoundedTopology::BoundedSlotTopology<
            RecordIndexSpace, TCapacity
        >;
        using Queue = ESPressio::BoundedTopology::IntrusiveQueue<
            RecordIndexSpace, TCapacity
        >;

    public:
        using Index = typename Queue::Index;

        struct Ticket final {
            Index Slot = Index::Invalid();
            TGeneration Generation = 0U;

            [[nodiscard]] bool IsValid() const noexcept {
                return Slot.IsValid() && Generation != 0U;
            }
        };

    private:
        enum class Phase : std::uint8_t {
            Vacant, Reserved, Queued, InFlight, Retiring, Exhausted
        };

        struct Record final {
            Index Next = Index::Invalid();
            TLease Lease{};
            TGeneration Generation = 0U;
            Phase State = Phase::Vacant;
            // Only one caller may release the source Memory lease. This
            // prevents two copies of one ticket from racing the native owner.
            std::atomic<bool> RetirementClaimed{false};

            [[nodiscard]] Index QueueNext() const noexcept { return Next; }
            void SetQueueNext(Index next) noexcept { Next = next; }
        };

        TMutexProvider* _mutex;
        TReleaseCoordinator* _releases;
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
                _integrityFailed.store(true, std::memory_order_release);
                return RecordQueueResult::IntegrityFailure;
            }
            if (_integrityFailed.load(std::memory_order_acquire)) {
                static_cast<void>(Unlock());
                return RecordQueueResult::IntegrityFailure;
            }
            return RecordQueueResult::Succeeded;
        }

        [[nodiscard]] bool Unlock() noexcept {
            if (_mutex->Release() !=
                ESPressio::Threading::OrdinaryMutexReleaseResult::Released) {
                _integrityFailed.store(true, std::memory_order_release);
                return false;
            }
            return true;
        }

        [[nodiscard]] RecordQueueResult UnlockWith(
            RecordQueueResult outcome
        ) noexcept {
            return Unlock() ? outcome : RecordQueueResult::IntegrityFailure;
        }

        [[nodiscard]] bool Matches(Ticket ticket) const noexcept {
            return ticket.IsValid() &&
                _slots.IsOccupied(ticket.Slot) &&
                _records[static_cast<std::size_t>(ticket.Slot.Value())].Generation ==
                    ticket.Generation;
        }

        [[nodiscard]] static constexpr bool Live(Phase phase) noexcept {
            return phase == Phase::Reserved || phase == Phase::Queued ||
                phase == Phase::InFlight || phase == Phase::Retiring;
        }

        [[nodiscard]] RecordQueueResult Retire(
            Ticket ticket, Phase expected
        ) noexcept {
            auto outcome = Lock();
            if (outcome != RecordQueueResult::Succeeded) return outcome;

            if (!Matches(ticket)) {
                return UnlockWith(RecordQueueResult::InvalidTicket);
            }
            auto& record = _records[static_cast<std::size_t>(ticket.Slot.Value())];
            if (record.State != expected && record.State != Phase::Retiring) {
                return UnlockWith(RecordQueueResult::WrongPhase);
            }
            if (record.RetirementClaimed.exchange(true, std::memory_order_acq_rel)) {
                return UnlockWith(RecordQueueResult::Busy);
            }
            record.State = Phase::Retiring;
            if (!Unlock()) return RecordQueueResult::IntegrityFailure;

            // Never call native Memory release, user code, or Threading wake
            // while holding the queue mutex. A failed release retains this
            // stable Retiring slot, which cannot be recycled or published.
            bool notified = true;
            if (!record.Lease.IsEmpty()) {
                const auto released = _releases->ReleaseAndNotify(record.Lease);
                if (released != CoordinatedReleaseResult::ReleasedAndNotified &&
                    released != CoordinatedReleaseResult::ReleasedNotificationFailed) {
                    if (record.Lease.IsEmpty()) {
                        // The provider denied a confirmed release, yet the
                        // lease no longer proves ownership. Quarantine.
                        _integrityFailed.store(true, std::memory_order_release);
                        return RecordQueueResult::IntegrityFailure;
                    }
                    record.RetirementClaimed.store(false, std::memory_order_release);
                    return RecordQueueResult::ReleaseUnconfirmed;
                }
                notified = released == CoordinatedReleaseResult::ReleasedAndNotified;
                if (!record.Lease.IsEmpty()) {
                    _integrityFailed.store(true, std::memory_order_release);
                    return RecordQueueResult::IntegrityFailure;
                }
            }

            outcome = Lock();
            if (outcome == RecordQueueResult::Busy) {
                // Lease is empty; a later ticket holder can only finalize,
                // and must never release the native Memory object twice.
                record.RetirementClaimed.store(false, std::memory_order_release);
                return outcome;
            }
            if (outcome != RecordQueueResult::Succeeded) return outcome;
            if (!Matches(ticket) || record.State != Phase::Retiring ||
                _slots.Release(ticket.Slot) !=
                    ESPressio::BoundedTopology::BoundedSlotReleaseResult::Succeeded) {
                _integrityFailed.store(true, std::memory_order_release);
                static_cast<void>(Unlock());
                return RecordQueueResult::IntegrityFailure;
            }
            record.State = Phase::Vacant;
            record.RetirementClaimed.store(false, std::memory_order_release);
            if (!Unlock()) return RecordQueueResult::IntegrityFailure;

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

    public:
        static constexpr std::size_t Capacity = TCapacity;
        static constexpr std::size_t StorageBytes = sizeof(TransactionalRecordQueue);

        TransactionalRecordQueue(
            TMutexProvider& mutex, TReleaseCoordinator& releases
        ) noexcept : _mutex(&mutex), _releases(&releases) {}

        TransactionalRecordQueue(const TransactionalRecordQueue&) = delete;
        TransactionalRecordQueue& operator=(const TransactionalRecordQueue&) = delete;

        ~TransactionalRecordQueue() noexcept {
            // An active Memory lease must never fall through native RAII without
            // the coordinator observing its actual release. It is a Bootstrap
            // lifecycle violation to destroy a queue before explicit drain.
            for (const auto& record : _records) {
                if (Live(record.State)) std::terminate();
            }
        }

        /// Takes one already-owned lease; output ticket is established before
        /// unlocking. Failed admission without a ticket leaves lease with caller.
        [[nodiscard]] RecordQueueResult Begin(
            TLease& lease, Ticket& ticket
        ) noexcept {
            if (lease.IsEmpty()) return RecordQueueResult::OutputLeaseEmpty;
            auto outcome = Lock();
            if (outcome != RecordQueueResult::Succeeded) return outcome;

            bool exhausted = false;
            for (std::size_t i = 0U; i < TCapacity; ++i) {
                const auto claim = _slots.Acquire();
                if (!claim.IsSucceeded()) {
                    return UnlockWith(exhausted
                        ? RecordQueueResult::GenerationExhausted
                        : RecordQueueResult::CapacityUnavailable);
                }
                auto index = claim.AcquiredIndex();
                auto& record = _records[static_cast<std::size_t>(index.Value())];
                if (record.Generation == std::numeric_limits<TGeneration>::max()) {
                    record.State = Phase::Exhausted;
                    exhausted = true;
                    continue; // retired forever: never recycle stale generation
                }
                ++record.Generation;
                record.Lease = std::move(lease);
                record.State = Phase::Reserved;
                ticket = Ticket{index, record.Generation};
                return Unlock() ? RecordQueueResult::Succeeded
                                : RecordQueueResult::IntegrityFailure;
            }
            return UnlockWith(RecordQueueResult::GenerationExhausted);
        }

        /// Transaction commit publishes the already-owned record to FIFO once.
        [[nodiscard]] RecordQueueResult Commit(Ticket ticket) noexcept {
            auto outcome = Lock();
            if (outcome != RecordQueueResult::Succeeded) return outcome;
            if (!Matches(ticket)) {
                return UnlockWith(RecordQueueResult::InvalidTicket);
            }
            auto& record = _records[static_cast<std::size_t>(ticket.Slot.Value())];
            if (record.State != Phase::Reserved) {
                return UnlockWith(RecordQueueResult::WrongPhase);
            }
            if (_fifo.Push(_records, ticket.Slot) !=
                ESPressio::BoundedTopology::IntrusiveQueuePushResult::Succeeded) {
                _integrityFailed.store(true, std::memory_order_release);
                static_cast<void>(Unlock());
                return RecordQueueResult::IntegrityFailure;
            }
            record.State = Phase::Queued;
            return Unlock() ? RecordQueueResult::Succeeded
                            : RecordQueueResult::IntegrityFailure;
        }

        /// Aborts an unpublished reservation and returns its Memory capacity.
        [[nodiscard]] RecordQueueResult Abort(Ticket ticket) noexcept {
            return Retire(ticket, Phase::Reserved);
        }

        /// Claims the oldest committed record for exclusive consumer handling.
        [[nodiscard]] RecordQueueResult Pop(Ticket& ticket) noexcept {
            auto outcome = Lock();
            if (outcome != RecordQueueResult::Succeeded) return outcome;
            Index index = Index::Invalid();
            if (_fifo.Pop(_records, index) ==
                ESPressio::BoundedTopology::IntrusiveQueuePopResult::Empty) {
                return UnlockWith(RecordQueueResult::Empty);
            }
            auto& record = _records[static_cast<std::size_t>(index.Value())];
            if (record.State != Phase::Queued) {
                _integrityFailed.store(true, std::memory_order_release);
                static_cast<void>(Unlock());
                return RecordQueueResult::IntegrityFailure;
            }
            record.State = Phase::InFlight;
            ticket = Ticket{index, record.Generation};
            return Unlock() ? RecordQueueResult::Succeeded
                            : RecordQueueResult::IntegrityFailure;
        }

        /// Borrow the source's Memory lease while holding the exclusive
        /// Reserved or InFlight ticket. Never transfer this reference to a
        /// concurrent actor without an explicit ownership transfer protocol.
        [[nodiscard]] TLease* Borrow(Ticket ticket) noexcept {
            if (_integrityFailed.load(std::memory_order_acquire) ||
                !Matches(ticket)) return nullptr;
            auto& record = _records[static_cast<std::size_t>(ticket.Slot.Value())];
            return (record.State == Phase::Reserved || record.State == Phase::InFlight)
                ? &record.Lease : nullptr;
        }

        /// Completes consumption after the last source-record reader is done.
        [[nodiscard]] RecordQueueResult Complete(Ticket ticket) noexcept {
            return Retire(ticket, Phase::InFlight);
        }

        [[nodiscard]] bool IntegrityFailed() const noexcept {
            return _integrityFailed.load(std::memory_order_acquire);
        }
    };

} // namespace ESPressio::Logging
