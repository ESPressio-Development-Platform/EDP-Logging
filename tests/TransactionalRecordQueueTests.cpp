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

    void TestCompileTimeMemoryBudgetAndEmptyLease() {
        static_assert(TestQueue::Capacity == 2U);
        static_assert(sizeof(TestQueue::Index) == 1U);
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
    TestCompileTimeMemoryBudgetAndEmptyLease();
}
