#include <cassert>
#include <cstdint>
#include <utility>

#include <logging/TransactionalRecordQueue.hpp>

namespace {
    using namespace ESPressio;
    using Result = Logging::RecordQueueResult;

    struct Lease final {
        bool Owned{false};
        int Value{0};

        Lease() = default;
        explicit Lease(int value) noexcept : Owned(true), Value(value) {}
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&& other) noexcept : Owned(other.Owned), Value(other.Value) {
            other.Owned = false;
        }
        Lease& operator=(Lease&& other) noexcept {
            assert(!Owned);
            Owned = other.Owned;
            Value = other.Value;
            other.Owned = false;
            return *this;
        }
        [[nodiscard]] bool IsEmpty() const noexcept { return !Owned; }
        [[nodiscard]] bool Release() noexcept {
            if (!Owned) return false;
            Owned = false;
            return true;
        }
    };

    struct Mutex final {
        bool Busy{false};
        bool FailAcquire{false};
        bool FailRelease{false};
        bool Held{false};

        [[nodiscard]] Threading::OrdinaryMutexAcquireResult Acquire(
            Platform::Synchronization::WaitTimeout
        ) noexcept { return TryAcquire(); }

        [[nodiscard]] Threading::OrdinaryMutexAcquireResult TryAcquire() noexcept {
            if (FailAcquire) return Threading::OrdinaryMutexAcquireResult::ProviderFailure;
            if (Busy || Held) return Threading::OrdinaryMutexAcquireResult::TimedOut;
            Held = true;
            return Threading::OrdinaryMutexAcquireResult::Acquired;
        }
        [[nodiscard]] Threading::OrdinaryMutexReleaseResult Release() noexcept {
            assert(Held);
            Held = false;
            if (FailRelease) return Threading::OrdinaryMutexReleaseResult::ProviderFailure;
            return Threading::OrdinaryMutexReleaseResult::Released;
        }
    };

    struct Coordinator final {
        unsigned Releases{0};
        unsigned Publications{0};
        bool FailRelease{false};
        bool FailWake{false};

        [[nodiscard]] Logging::CoordinatedReleaseResult ReleaseAndNotify(Lease& lease) noexcept {
            ++Releases;
            if (FailRelease) {
                return Logging::CoordinatedReleaseResult::MemoryProviderFailure;
            }
            assert(lease.Release());
            ++Publications;
            return FailWake ? Logging::CoordinatedReleaseResult::ReleasedNotificationFailed
                            : Logging::CoordinatedReleaseResult::ReleasedAndNotified;
        }

        [[nodiscard]] Logging::AdmissionWakePublicationResult PublishCapacityAvailable() noexcept {
            ++Publications;
            return FailWake ? Logging::AdmissionWakePublicationResult::ProviderFailure
                            : Logging::AdmissionWakePublicationResult::Published;
        }
    };

    using TestQueue = Logging::TransactionalRecordQueue<2U, Lease, Mutex, Coordinator>;

    void TestTransactionCommitOrderAndLifetime() {
        Mutex mutex;
        Coordinator callbacks;
        TestQueue queue(mutex, callbacks);
        Lease first(11), second(22);
        TestQueue::Ticket a, b, consumed;
        assert(queue.Begin(first, a) == Result::Succeeded && first.IsEmpty());
        assert(queue.Begin(second, b) == Result::Succeeded && second.IsEmpty());
        assert(queue.Borrow(a) && queue.Borrow(a)->Value == 11);
        assert(queue.Commit(b) == Result::Succeeded);
        assert(queue.Commit(a) == Result::Succeeded);
        assert(queue.Commit(a) == Result::WrongPhase);
        assert(queue.Pop(consumed) == Result::Succeeded);
        assert(consumed.Slot == b.Slot && consumed.Generation == b.Generation);
        assert(queue.Borrow(consumed)->Value == 22);
        assert(queue.Complete(consumed) == Result::Succeeded);
        assert(callbacks.Releases == 1 && callbacks.Publications == 2);
        assert(queue.Pop(consumed) == Result::Succeeded);
        assert(consumed.Slot == a.Slot && queue.Borrow(consumed)->Value == 11);
        assert(queue.Complete(consumed) == Result::Succeeded);
        assert(callbacks.Releases == 2 && callbacks.Publications == 4);
        assert(queue.Pop(consumed) == Result::Empty);
    }

    void TestAbortReclaimAndStaleGeneration() {
        Mutex mutex;
        Coordinator callbacks;
        TestQueue queue(mutex, callbacks);
        Lease first(1), second(2), third(3);
        TestQueue::Ticket original, other, later, out;
        assert(queue.Begin(first, original) == Result::Succeeded);
        assert(queue.Begin(second, other) == Result::Succeeded);
        assert(queue.Begin(third, later) == Result::CapacityUnavailable);
        assert(!third.IsEmpty());
        assert(queue.Abort(original) == Result::Succeeded);
        assert(queue.Pop(out) == Result::Empty);
        assert(queue.Begin(third, later) == Result::Succeeded);
        assert(later.Slot == original.Slot);
        assert(later.Generation != original.Generation);
        assert(queue.Commit(original) == Result::InvalidTicket);
        assert(queue.Commit(later) == Result::Succeeded);
        assert(queue.Pop(out) == Result::Succeeded && out.Generation == later.Generation);
        assert(queue.Complete(out) == Result::Succeeded);
        assert(queue.Abort(other) == Result::Succeeded);
    }

    void TestFailedMemoryReleaseRetryAndWakeReport() {
        Mutex mutex;
        Coordinator callbacks;
        TestQueue queue(mutex, callbacks);
        Lease lease(7);
        TestQueue::Ticket ticket;
        assert(queue.Begin(lease, ticket) == Result::Succeeded);
        callbacks.FailRelease = true;
        assert(queue.Abort(ticket) == Result::ReleaseUnconfirmed);
        assert(callbacks.Releases == 1);
        assert(queue.Borrow(ticket) == nullptr); // quarantined Retiring
        callbacks.FailRelease = false;
        callbacks.FailWake = true;
        assert(queue.Abort(ticket) == Result::ReleasedNotificationFailed);
        assert(callbacks.Releases == 2);
        assert(queue.Abort(ticket) == Result::InvalidTicket);
        assert(!queue.IntegrityFailed());
    }

    void TestMutexBusyAndFailure() {
        Mutex mutex;
        Coordinator callbacks;
        TestQueue queue(mutex, callbacks);
        Lease lease(5);
        TestQueue::Ticket ticket;
        mutex.Busy = true;
        assert(queue.Begin(lease, ticket) == Result::Busy && !lease.IsEmpty());
        mutex.Busy = false;
        assert(queue.Begin(lease, ticket) == Result::Succeeded);
        mutex.Busy = true;
        assert(queue.Commit(ticket) == Result::Busy);
        mutex.Busy = false;
        assert(queue.Commit(ticket) == Result::Succeeded);
        TestQueue::Ticket out;
        assert(queue.Pop(out) == Result::Succeeded);
        assert(queue.Complete(out) == Result::Succeeded);
        mutex.FailAcquire = true;
        assert(queue.Pop(out) == Result::IntegrityFailure);
        assert(queue.IntegrityFailed());
    }

    void TestNativeMutexReleaseFailureIsNotHidden() {
        Mutex mutex;
        Coordinator callbacks;
        TestQueue queue(mutex, callbacks);
        TestQueue::Ticket empty;
        mutex.FailRelease = true;
        assert(queue.Pop(empty) == Result::IntegrityFailure);
        assert(queue.IntegrityFailed());
    }

    void TestMoveOnlyRaiiTransactionAndConsumerClaim() {
        using Reservation = TestQueue::Reservation;
        using Consumer = TestQueue::ConsumerClaim;
        static_assert(!std::is_copy_constructible_v<Reservation>);
        static_assert(!std::is_copy_assignable_v<Reservation>);
        static_assert(std::is_nothrow_move_constructible_v<Reservation>);
        static_assert(!std::is_copy_constructible_v<Consumer>);
        static_assert(!std::is_copy_assignable_v<Consumer>);
        static_assert(std::is_nothrow_move_constructible_v<Consumer>);

        Mutex mutex;
        Coordinator coordinator;
        TestQueue queue(mutex, coordinator);
        Lease first(41);
        {
            Reservation unpublished;
            assert(queue.Begin(first, unpublished) == Result::Succeeded);
            assert(first.IsEmpty() && unpublished.IsActive());
            assert(unpublished.Borrow() && unpublished.Borrow()->Value == 41);
            Reservation moved(std::move(unpublished));
            assert(!unpublished.IsActive() && moved.IsActive());
            Lease competing(42);
            assert(queue.Begin(competing, moved) == Result::OutputHandleOccupied);
            assert(!competing.IsEmpty());
            assert(moved.Commit() == Result::Succeeded);
            assert(!moved.IsActive());
        }
        {
            Consumer consumer;
            assert(queue.Pop(consumer) == Result::Succeeded);
            assert(consumer.IsActive() && consumer.Borrow()->Value == 41);
            Consumer moved(std::move(consumer));
            assert(!consumer.IsActive() && moved.IsActive());
            // Destructor of moved is the final reader and releases capacity.
        }
        assert(coordinator.Releases == 1U);
        assert(coordinator.Publications == 2U);

        Lease aborted(99);
        {
            Reservation forgotten;
            assert(queue.Begin(aborted, forgotten) == Result::Succeeded);
            assert(forgotten.IsActive());
            // Destructor must roll back the unpublished queue/Memory claim.
        }
        assert(coordinator.Releases == 2U);
        TestQueue::Ticket empty;
        assert(queue.Pop(empty) == Result::Empty);
        assert(!queue.IntegrityFailed());
    }

    void TestQueueSlotReservationBeforeMemoryOwnership() {
        using Timeout = Platform::Synchronization::WaitTimeout;
        Mutex mutex;
        Coordinator callbacks;
        TestQueue queue(mutex, callbacks);
        {
            TestQueue::Reservation first, second, third;
            mutex.Busy = true;
            assert(queue.ReserveSlot(first, Timeout::NoWait()) == Result::Busy);
            assert(!first.IsActive());
            mutex.Busy = false;
            assert(queue.ReserveSlot(first, Timeout::NoWait()) == Result::Succeeded);
            assert(first.IsActive() && first.Borrow() == nullptr);
            assert(queue.ReserveSlot(second, Timeout::NoWait()) == Result::Succeeded);
            assert(queue.ReserveSlot(third, Timeout::NoWait()) == Result::CapacityUnavailable);
            assert(!third.IsActive());
            // Releasing empty reserved slot needs no native Memory lease.
            assert(second.Abort() == Result::Succeeded);
            assert(callbacks.Releases == 0U && callbacks.Publications == 1U);
            assert(queue.ReserveSlot(third, Timeout::NoWait()) == Result::Succeeded);

            Lease actual(56);
            assert(queue.AttachLease(first, actual, Timeout::NoWait()) == Result::Succeeded);
            assert(actual.IsEmpty() && first.Borrow()->Value == 56);
            assert(first.Commit() == Result::Succeeded);
            assert(third.Abort() == Result::Succeeded);
            TestQueue::ConsumerClaim consuming;
            assert(queue.Pop(consuming) == Result::Succeeded);
            assert(consuming.Borrow()->Value == 56);
            assert(consuming.Complete() == Result::Succeeded);
        }
        assert(callbacks.Releases == 1U);
        assert(!queue.IntegrityFailed());
    }

    void TestCompileTimeMemoryBudgetAndEmptyLease() {
        static_assert(TestQueue::Capacity == 2U);
        static_assert(sizeof(TestQueue::Index) == 1U);
        static_assert(TestQueue::StorageBytes == sizeof(TestQueue));
        Mutex mutex;
        Coordinator callbacks;
        TestQueue queue(mutex, callbacks);
        Lease empty;
        TestQueue::Ticket ticket;
        assert(queue.Begin(empty, ticket) == Result::OutputLeaseEmpty);
    }
}

int main() {
    TestTransactionCommitOrderAndLifetime();
    TestAbortReclaimAndStaleGeneration();
    TestFailedMemoryReleaseRetryAndWakeReport();
    TestMutexBusyAndFailure();
    TestNativeMutexReleaseFailureIsNotHidden();
    TestMoveOnlyRaiiTransactionAndConsumerClaim();
    TestQueueSlotReservationBeforeMemoryOwnership();
    TestCompileTimeMemoryBudgetAndEmptyLease();
    return 0;
}
