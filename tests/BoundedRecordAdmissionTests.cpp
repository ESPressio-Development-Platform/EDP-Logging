#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

#define EDP_LOGGING_G1_LIFECYCLE_TEST_INCLUDE
#include "AdmissionWakeCoordinatorTests.cpp"

#include <logging/BoundedRecordAdmission.hpp>

namespace {
    struct RecordMutexIdentity final {};

    /// Inject notification failure without changing the genuine Memory pool.
    /// The third slot is reserved for the lifecycle shutdown-drainer.
    struct FaultedRecordWakeProvider final {
        static constexpr std::size_t Capacity = 3U;
        std::size_t WakeCalls{0U};

        ESPressio::Threading::BoundedWaitWakeResult Wake(
            std::size_t slot
        ) noexcept {
            ++WakeCalls;
            return slot == 1U
                ? ESPressio::Threading::BoundedWaitWakeResult::ProviderFailure
                : ESPressio::Threading::BoundedWaitWakeResult::Woken;
        }

        ESPressio::Threading::BoundedWaitWakeResult WaitUntil(
            std::size_t, ESPressio::Clock::MonotonicTimestamp
        ) noexcept {
            return ESPressio::Threading::BoundedWaitWakeResult::TimedOut;
        }
    };

    using WakeProvider = ESPressio::Threading::BoundedWaitWakeProvider<
        TestSupport::TestSignalProvider, 3U>;
    using Lifecycle = ESPressio::Logging::AdmissionLifecycleCoordinator<2U, WakeProvider>;
    using QueueMutex = ESPressio::Threading::OrdinaryMutexProvider<
        RecordMutexIdentity, TestSupport::TestMutexProvider>;
    using Queue = ESPressio::Logging::TransactionalRecordQueue<
        2U, PooledLease, QueueMutex, Lifecycle>;

    void TestImmediateRecordAdmission() {
        namespace L = ESPressio::Logging;
        Resource resource;
        TestSupport::TestMutexProvider nativeMutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement, Framework::SelectUnique
        > allocator;
        Runtime runtime(nativeMutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) ==
            Memory::MemoryTopologyInitializationResult::Succeeded);

        WakeProvider wakes;
        Lifecycle lifecycle(wakes);
        QueueMutex queueMutex;
        Queue queue(queueMutex, lifecycle);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();
        PooledLease scratch;
        Queue::Reservation reservation;
        const auto neverStop = []() noexcept -> bool { return false; };

        const auto status = L::AcquireRecordUntil<4U, 8U>(
            queue, lifecycle, wakes, 0U, pool, scratch, reservation,
            DeadlineAfter(std::chrono::seconds(2)), neverStop, 203
        );
        assert(status.Status == L::RecordAdmissionStatus::Admitted);
        assert(status.MemoryAttempted);
        assert(status.Memory == L::AdmissionWaitResult::Admitted);
        assert(status.WakePublication == L::AdmissionWakePublicationResult::Published);
        assert(reservation.IsActive() && scratch.IsEmpty());
        assert(reservation.Borrow()->Get()->Value() == 203);
        assert(reservation.Commit() == L::RecordQueueResult::Succeeded);
        Queue::ConsumerClaim consumer;
        assert(queue.Pop(consumer) == L::RecordQueueResult::Succeeded);
        assert(consumer.Borrow()->Get()->Value() == 203);
        assert(consumer.Complete() == L::RecordQueueResult::Succeeded);
        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestQueueFullDoesNotConsumeMemoryAndUsesExistingWake() {
        namespace L = ESPressio::Logging;
        Resource resource;
        TestSupport::TestMutexProvider nativeMutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement, Framework::SelectUnique
        > allocator;
        Runtime runtime(nativeMutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) ==
            Memory::MemoryTopologyInitializationResult::Succeeded);
        WakeProvider wakes;
        Lifecycle lifecycle(wakes);
        QueueMutex queueMutex;
        Queue queue(queueMutex, lifecycle);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();
        Queue::Reservation held0, held1;
        using Timeout = Platform::Synchronization::WaitTimeout;
        assert(queue.ReserveSlot(held0, Timeout::NoWait()) == L::RecordQueueResult::Succeeded);
        assert(queue.ReserveSlot(held1, Timeout::NoWait()) == L::RecordQueueResult::Succeeded);
        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        std::atomic<bool> entered{false};
        std::atomic<bool> success{false};
        std::thread producer([&] {
            PooledLease scratch;
            Queue::Reservation output;
            const auto neverStop = []() noexcept -> bool { return false; };
            entered.store(true);
            const auto result = L::AcquireRecordUntil<3U, 8U>(
                queue, lifecycle, wakes, 0U, pool, scratch, output,
                DeadlineAfter(std::chrono::seconds(2)), neverStop, 308
            );
            success.store(result.Status == L::RecordAdmissionStatus::Admitted &&
                output.IsActive() && scratch.IsEmpty() &&
                output.Borrow()->Get()->Value() == 308);
            // Automatic abort on scope exit is a legitimate transaction rollback.
        });
        while (!entered.load()) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        assert(held0.Abort() == L::RecordQueueResult::Succeeded);
        producer.join();
        assert(success.load());
        assert(held1.Abort() == L::RecordQueueResult::Succeeded);
        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestShutdownTracksQueueWaitingProducer() {
        namespace L = ESPressio::Logging;
        Resource resource;
        TestSupport::TestMutexProvider nativeMutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement, Framework::SelectUnique
        > allocator;
        Runtime runtime(nativeMutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) ==
            Memory::MemoryTopologyInitializationResult::Succeeded);

        WakeProvider wakes;
        Lifecycle lifecycle(wakes);
        QueueMutex queueMutex;
        Queue queue(queueMutex, lifecycle);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();
        using Timeout = Platform::Synchronization::WaitTimeout;
        Queue::Reservation held0, held1;
        assert(queue.ReserveSlot(held0, Timeout::NoWait()) == L::RecordQueueResult::Succeeded);
        assert(queue.ReserveSlot(held1, Timeout::NoWait()) == L::RecordQueueResult::Succeeded);

        std::atomic<bool> entered{false};
        std::atomic<L::RecordAdmissionStatus> result{L::RecordAdmissionStatus::QueueFailure};
        std::thread producer([&]() {
            PooledLease scratch;
            Queue::Reservation reservation;
            const auto neverStop = []() noexcept -> bool { return false; };
            entered.store(true);
            const auto outcome = L::AcquireRecordUntil<4U, 8U>(
                queue, lifecycle, wakes, 0U, pool, scratch, reservation,
                DeadlineAfter(std::chrono::seconds(2)), neverStop, 377
            );
            result.store(outcome.Status);
        });

        while (!entered.load()) std::this_thread::yield();
        for (std::size_t i = 0U;
             i < 1000U && lifecycle.ActiveAdmissions() == 0U; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // A queue-only wait must be visible to ShutdownUntil's drain counter.
        assert(lifecycle.ActiveAdmissions() == 1U);
        const auto stopped = lifecycle.ShutdownUntil<8U>(
            DeadlineAfter(std::chrono::seconds(1))
        );
        producer.join();
        assert(stopped.Status == L::AdmissionShutdownStatus::Quiescent);
        assert(result.load() == L::RecordAdmissionStatus::ShutdownRequested);
        assert(lifecycle.ActiveAdmissions() == 0U);
        assert(held0.Abort() == L::RecordQueueResult::Succeeded);
        assert(held1.Abort() == L::RecordQueueResult::Succeeded);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestInvalidProducerSlotCannotBorrowDrainSlot() {
        namespace L = ESPressio::Logging;
        Resource resource;
        TestSupport::TestMutexProvider nativeMutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement, Framework::SelectUnique
        > allocator;
        Runtime runtime(nativeMutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) ==
            Memory::MemoryTopologyInitializationResult::Succeeded);

        WakeProvider wakes;
        Lifecycle lifecycle(wakes);
        QueueMutex queueMutex;
        Queue queue(queueMutex, lifecycle);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();
        using Timeout = Platform::Synchronization::WaitTimeout;
        Queue::Reservation held0, held1, output;
        assert(queue.ReserveSlot(held0, Timeout::NoWait()) ==
            L::RecordQueueResult::Succeeded);
        assert(queue.ReserveSlot(held1, Timeout::NoWait()) ==
            L::RecordQueueResult::Succeeded);
        PooledLease scratch;
        const auto neverStop = []() noexcept -> bool { return false; };
        // Slot 2 belongs EXCLUSIVELY to ShutdownUntil, not a third producer.
        const auto denied = L::AcquireRecordUntil<2U, 4U>(
            queue, lifecycle, wakes, 2U, pool, scratch, output,
            DeadlineAfter(std::chrono::seconds(1)), neverStop, 404
        );
        assert(denied.Status == L::RecordAdmissionStatus::InvalidProducerSlot);
        assert(!denied.MemoryAttempted);
        assert(denied.WakePublication == L::AdmissionWakePublicationResult::NotAttempted);
        assert(!output.IsActive() && scratch.IsEmpty());
        assert(lifecycle.ActiveAdmissions() == 0U);
        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        assert(held0.Abort() == L::RecordQueueResult::Succeeded);
        assert(held1.Abort() == L::RecordQueueResult::Succeeded);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestSuccessfulGrantPreservesFailedWakeEvidence() {
        namespace L = ESPressio::Logging;
        Resource resource;
        TestSupport::TestMutexProvider nativeMutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement, Framework::SelectUnique
        > allocator;
        Runtime runtime(nativeMutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) ==
            Memory::MemoryTopologyInitializationResult::Succeeded);

        FaultedRecordWakeProvider wakes;
        using FaultLifecycle = L::AdmissionLifecycleCoordinator<
            2U, FaultedRecordWakeProvider>;
        using FaultQueue = L::TransactionalRecordQueue<
            2U, PooledLease, QueueMutex, FaultLifecycle>;
        FaultLifecycle lifecycle(wakes);
        QueueMutex queueMutex;
        FaultQueue queue(queueMutex, lifecycle);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();
        PooledLease scratch;
        FaultQueue::Reservation output;
        const auto neverStop = []() noexcept -> bool { return false; };
        const auto status = L::AcquireRecordUntil<2U, 4U>(
            queue, lifecycle, wakes, 0U, pool, scratch, output,
            DeadlineAfter(std::chrono::seconds(1)), neverStop, 509
        );
        // A Memory grant must NOT conceal the independently failed producer
        // wake. Ownership nevertheless transfers to the accepted Reservation.
        assert(status.Status == L::RecordAdmissionStatus::Admitted);
        assert(status.WakePublication ==
            L::AdmissionWakePublicationResult::ProviderFailure);
        assert(output.IsActive() && scratch.IsEmpty());
        assert(output.Borrow()->Get()->Value() == 509);
        assert(wakes.WakeCalls == 2U);
        assert(output.Commit() == L::RecordQueueResult::Succeeded);
        FaultQueue::ConsumerClaim consumer;
        assert(queue.Pop(consumer) == L::RecordQueueResult::Succeeded);
        assert(consumer.Complete() == L::RecordQueueResult::ReleasedNotificationFailed);
        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestFullQueueDeadlineAndCancellation() {
        namespace L = ESPressio::Logging;
        Resource resource;
        TestSupport::TestMutexProvider nativeMutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement, Framework::SelectUnique
        > allocator;
        Runtime runtime(nativeMutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) ==
            Memory::MemoryTopologyInitializationResult::Succeeded);
        WakeProvider wakes;
        Lifecycle lifecycle(wakes);
        QueueMutex queueMutex;
        Queue queue(queueMutex, lifecycle);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();
        using Timeout = Platform::Synchronization::WaitTimeout;
        Queue::Reservation held0, held1, output;
        assert(queue.ReserveSlot(held0, Timeout::NoWait()) == L::RecordQueueResult::Succeeded);
        assert(queue.ReserveSlot(held1, Timeout::NoWait()) == L::RecordQueueResult::Succeeded);
        PooledLease scratch;
        const auto neverStop = []() noexcept -> bool { return false; };
        const auto expired = L::AcquireRecordUntil<1U, 2U>(
            queue, lifecycle, wakes, 0U, pool, scratch, output,
            DeadlineAfter(std::chrono::milliseconds(5)), neverStop, 2
        );
        assert(expired.Status == L::RecordAdmissionStatus::DeadlineElapsed);
        assert(!expired.MemoryAttempted && scratch.IsEmpty() && !output.IsActive());
        const auto alwaysStop = []() noexcept -> bool { return true; };
        const auto cancelled = L::AcquireRecordUntil<1U, 2U>(
            queue, lifecycle, wakes, 0U, pool, scratch, output,
            DeadlineAfter(std::chrono::seconds(1)), alwaysStop, 3
        );
        assert(cancelled.Status == L::RecordAdmissionStatus::ShutdownRequested);
        assert(!cancelled.MemoryAttempted);
        assert(held0.Abort() == L::RecordQueueResult::Succeeded);
        assert(held1.Abort() == L::RecordQueueResult::Succeeded);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

#include "G1DelayedAttachHost.hpp"
}

int main() {
    const CanonicalClock clock;
    assert(ESPressio::Clock::BindMonotonicClock(clock));
    TestLatchedWakeBeforeWait();
    TestRealMemoryTopologyConcurrentReleases();
    TestRealTopologyExpiredDeadlineDoesNotClaimCapacity();
    TestFailedWakeCannotConcealReleasedCapacity();
    TestContendedMemoryMutexIsDeadlineBoundedEvenWithoutNotification();
    TestImmediateRecordAdmission();
    TestQueueFullDoesNotConsumeMemoryAndUsesExistingWake();
    TestShutdownTracksQueueWaitingProducer();
    TestInvalidProducerSlotCannotBorrowDrainSlot();
    TestSuccessfulGrantPreservesFailedWakeEvidence();
    TestFullQueueDeadlineAndCancellation();
    TestDeadlineCannotBeOverrunByFinalQueueAttachment();
    TestCancellationAtFinalQueueAttachment();
}
