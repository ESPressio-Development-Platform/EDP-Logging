// Reuse legacy fixtures without executing their main twice in this suite.
#define main EDPExistingTransactionalQueueTestsMain
#include "TransactionalRecordQueueTests.cpp"
#undef main
#include <array>
#include <cstdio>

namespace {


    namespace C = ESPressio::Clock;
    namespace CF = ESPressio::System::CompositionFramework;

    /// Deterministic canonical coordinate used only by these transition tests.
    struct QueueTestClock final
        : CF::Provider<C::Domain, CF::Offers<CF::Offer<C::MonotonicClock>>> {
        /// Test controls the exact before-grant, after-grant and release boundary.
        std::uint64_t Tick{0U};
        /// Returns the controlled canonical coordinate without another timer.
        C::MonotonicTimestamp Now() const noexcept {
            return C::MonotonicTimestamp::FromNanoseconds(Tick);
        }
    };

    /// Strong Threading mutex ABI plus deterministic late-grant/time-out controls.
    struct DeadlineMutex final {
        /// Legacy fixture still owns all native mutual-exclusion state.
        Mutex Native{};
        /// Bound canonical test clock, never an independently sampled timebase.
        QueueTestClock* Clock{};
        /// Counts finite mutex acquisitions independently of legacy NoWait.
        unsigned Acquires{0U};
        /// Native remaining budgets prove the deadline was not restarted.
        std::array<std::uint64_t, 8U> Waits{};
        /// Optional clock coordinate reached immediately before a native grant.
        std::uint64_t AdvanceGrantTo{0U};
        /// One chosen finite acquisition returns timeout at the specified coordinate.
        unsigned TimeoutOnAcquire{0U};
        /// Test coordinate reached by the chosen timeout.
        std::uint64_t TimeoutAt{0U};

        /// Original no-wait contract delegates to the same fixture mutex.
        Threading::OrdinaryMutexAcquireResult TryAcquire() noexcept {
            return Native.TryAcquire();
        }

        /// Supplies deterministic ordinary-provider results to the real queue algorithm.
        Threading::OrdinaryMutexAcquireResult Acquire(
            Platform::Synchronization::WaitTimeout wait) noexcept {
            Waits[Acquires++] = wait.Nanoseconds();
            if (Acquires == TimeoutOnAcquire) {
                Clock->Tick = TimeoutAt;
                return Threading::OrdinaryMutexAcquireResult::TimedOut;
            }
            if (AdvanceGrantTo != 0U) {
                Clock->Tick = AdvanceGrantTo;
            }
            return Native.Acquire(wait);
        }

        /// Confirms the original mutex's actual release result.
        Threading::OrdinaryMutexReleaseResult Release() noexcept {
            return Native.Release();
        }
    };

    /// Existing release coordinator with one deterministic callback-time advance.
    struct DeadlineRelease final {
        /// Native lease release and wake evidence from the original fixture.
        Coordinator Native{};
        /// Same bound clock used by queue locks.
        QueueTestClock* Clock{};
        /// Coordinate reached after confirmed release, before retirement's second lock.
        std::uint64_t AfterRelease{0U};

        /// Records the actual Memory release before advancing the test coordinate.
        Logging::CoordinatedReleaseResult ReleaseAndNotify(Lease& lease) noexcept {
            const auto result = Native.ReleaseAndNotify(lease);
            if (AfterRelease != 0U) {
                Clock->Tick = AfterRelease;
            }
            return result;
        }

        /// Original capacity publication result remains observable.
        Logging::AdmissionWakePublicationResult PublishCapacityAvailable() noexcept {
            return Native.PublishCapacityAvailable();
        }
    };

    using DeadlineQueue =
        Logging::TransactionalRecordQueue<2U, Lease, DeadlineMutex, DeadlineRelease>;

    /// Expired and late-granted commit attempts never publish or consume the reservation.
    void TestCommitDeadlinePreservesReservation(QueueTestClock& clock) {
        clock.Tick = 100U;
        DeadlineMutex mutex{{}, &clock};
        DeadlineRelease release{{}, &clock};
        DeadlineQueue queue(mutex, release);
        Lease source(41);
        DeadlineQueue::Reservation reservation{};
        assert(queue.Begin(
            source,
            reservation
        ) == Result::Succeeded);
        assert(source.IsEmpty() && reservation.Borrow()->Value == 41);
        assert(reservation.CommitUntil(C::MonotonicTimestamp::FromNanoseconds(100U)) ==
               Result::DeadlineElapsed);
        assert(mutex.Acquires == 0U && reservation.IsActive());
        mutex.AdvanceGrantTo = 1000U;
        assert(reservation.CommitUntil(C::MonotonicTimestamp::FromNanoseconds(1000U)) ==
               Result::DeadlineElapsed);
        assert(mutex.Waits[0U] == 900U && !mutex.Native.Held);
        assert(reservation.IsActive() && reservation.Borrow()->Value == 41);
        DeadlineQueue::Ticket raw{};
        assert(queue.Pop(raw) == Result::Empty);
        mutex.AdvanceGrantTo = 0U;
        assert(reservation.CommitUntil(C::MonotonicTimestamp::FromNanoseconds(2000U)) ==
               Result::Succeeded);
        assert(!reservation.IsActive() && reservation.Borrow() == nullptr);
        DeadlineQueue::ConsumerClaim claim{};
        assert(queue.PopUntil(
            claim,
            C::MonotonicTimestamp::FromNanoseconds(1000U)
        ) ==
               Result::DeadlineElapsed);
        assert(!claim.IsActive());
        assert(queue.PopUntil(
            claim,
            C::MonotonicTimestamp::FromNanoseconds(2000U)
        ) ==
               Result::Succeeded);
        assert(claim.Borrow()->Value == 41);
        assert(claim.CompleteUntil(C::MonotonicTimestamp::FromNanoseconds(2000U)) ==
               Result::Succeeded);
        assert(!claim.IsActive() && release.Native.Releases == 1U);
    }

    /// Confirmed release followed by final-lock timeout may only finalize on explicit retry.
    void TestPostReleaseTimeoutNeverReleasesTwice(QueueTestClock& clock) {
        clock.Tick = 100U;
        DeadlineMutex mutex{{}, &clock};
        DeadlineRelease release{{}, &clock, 800U};
        DeadlineQueue queue(mutex, release);
        Lease source(51);
        DeadlineQueue::Reservation reservation{};
        assert(queue.Begin(
            source,
            reservation
        ) == Result::Succeeded);
        assert(reservation.Commit() == Result::Succeeded);
        DeadlineQueue::ConsumerClaim claim{};
        assert(queue.Pop(claim) == Result::Succeeded);
        mutex.TimeoutOnAcquire = 2U;
        mutex.TimeoutAt = 1000U;
        assert(claim.CompleteUntil(C::MonotonicTimestamp::FromNanoseconds(1000U)) ==
               Result::DeadlineElapsed);
        assert(mutex.Waits[0U] == 900U && mutex.Waits[1U] == 200U);
        assert(claim.IsActive() && claim.Borrow() == nullptr);
        assert(release.Native.Releases == 1U && !queue.IsIntegrityFailed());
        DeadlineQueue::ConsumerClaim empty{};
        assert(queue.Pop(empty) == Result::Empty);
        release.AfterRelease = 0U;
        assert(claim.CompleteUntil(C::MonotonicTimestamp::FromNanoseconds(2000U)) ==
               Result::Succeeded);
        assert(release.Native.Releases == 1U && !claim.IsActive());
    }

    /// A late Pop grant leaves FIFO order and exclusive source ownership unchanged.
    void TestLatePopAndStableOwnedView(QueueTestClock& clock) {
        clock.Tick = 100U;
        DeadlineMutex mutex{{}, &clock};
        DeadlineRelease release{{}, &clock};
        DeadlineQueue queue(mutex, release);
        Lease source(61);
        DeadlineQueue::Reservation reservation{};
        assert(queue.Begin(
            source,
            reservation
        ) == Result::Succeeded);
        mutex.Native.Busy = true;
        assert(reservation.Borrow() != nullptr && reservation.Borrow()->Value == 61);
        assert(reservation.Commit() == Result::Busy && reservation.IsActive());
        mutex.Native.Busy = false;
        assert(reservation.Commit() == Result::Succeeded);
        mutex.AdvanceGrantTo = 1000U;
        DeadlineQueue::ConsumerClaim claim{};
        assert(queue.PopUntil(
            claim,
            C::MonotonicTimestamp::FromNanoseconds(1000U)
        ) ==
               Result::DeadlineElapsed);
        assert(!claim.IsActive());
        mutex.AdvanceGrantTo = 0U;
        assert(queue.PopUntil(
            claim,
            C::MonotonicTimestamp::FromNanoseconds(2000U)
        ) ==
               Result::Succeeded);
        mutex.Native.Busy = true;
        assert(claim.Borrow() != nullptr && claim.Borrow()->Value == 61);
        mutex.Native.Busy = false;
        assert(claim.CompleteUntil(C::MonotonicTimestamp::FromNanoseconds(2000U)) ==
               Result::Succeeded);
        assert(release.Native.Releases == 1U);
    }

    /// Bounded explicit abort returns exactly the acquired Memory capacity.
    void TestBoundedAbortAndMove(QueueTestClock& clock) {
        clock.Tick = 100U;
        DeadlineMutex mutex{{}, &clock};
        DeadlineRelease release{{}, &clock};
        DeadlineQueue queue(mutex, release);
        Lease source(71);
        DeadlineQueue::Reservation first{};
        assert(queue.Begin(
            source,
            first
        ) == Result::Succeeded);
        DeadlineQueue::Reservation moved(std::move(first));
        assert(!first.IsActive() && first.Borrow() == nullptr);
        assert(moved.Borrow()->Value == 71);
        assert(moved.AbortUntil(C::MonotonicTimestamp::FromNanoseconds(100U)) ==
               Result::DeadlineElapsed);
        assert(moved.IsActive() && release.Native.Releases == 0U);
        assert(moved.AbortUntil(C::MonotonicTimestamp::FromNanoseconds(1000U)) ==
               Result::Succeeded);
        assert(!moved.IsActive() && release.Native.Releases == 1U);
    }

    /// Lost release notification cannot become success merely because metadata finalization was
    /// retried.
    void TestNotificationFailurePersistsAcrossTimeout(QueueTestClock& clock) {
        clock.Tick = 100U;
        DeadlineMutex mutex{{}, &clock};
        DeadlineRelease release{{}, &clock, 800U};
        DeadlineQueue queue(mutex, release);
        Lease source(81);
        DeadlineQueue::Reservation reservation{};
        assert(queue.Begin(
            source,
            reservation
        ) == Result::Succeeded);
        assert(reservation.Commit() == Result::Succeeded);
        DeadlineQueue::ConsumerClaim claim{};
        assert(queue.Pop(claim) == Result::Succeeded);
        release.Native.FailWake = true;
        mutex.TimeoutOnAcquire = 2U;
        mutex.TimeoutAt = 1000U;
        assert(claim.CompleteUntil(C::MonotonicTimestamp::FromNanoseconds(1000U)) ==
               Result::DeadlineElapsed);
        assert(claim.IsActive() && release.Native.Releases == 1U);
        release.Native.FailWake = false;
        release.AfterRelease = 0U;
        assert(claim.CompleteUntil(C::MonotonicTimestamp::FromNanoseconds(2000U)) ==
               Result::ReleasedNotificationFailed);
        assert(!claim.IsActive() && release.Native.Releases == 1U);
        Lease later(82);
        assert(queue.Begin(
            later,
            reservation
        ) == Result::Succeeded);
        assert(reservation.Commit() == Result::Succeeded);
        assert(queue.Pop(claim) == Result::Succeeded);
        assert(claim.Complete() == Result::Succeeded);
        assert(release.Native.Releases == 2U);
    }

} // anonymous namespace

int main() {
    static QueueTestClock clock{};
    assert(C::BindMonotonicClock(clock));
    TestCommitDeadlinePreservesReservation(clock);
    TestPostReleaseTimeoutNeverReleasesTwice(clock);
    TestLatePopAndStableOwnedView(clock);
    TestBoundedAbortAndMove(clock);
    TestNotificationFailurePersistsAcrossTimeout(clock);
    std::puts("BOUNDED_QUEUE_DEADLINES_LATE_GRANTS_RELEASE_ONCE=PASS");
    return 0;
}
