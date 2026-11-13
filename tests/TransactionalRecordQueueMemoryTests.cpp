#include <cassert>
#include <cstdint>

#define EDP_LOGGING_G1_LIFECYCLE_TEST_INCLUDE
#include "AdmissionWakeCoordinatorTests.cpp"
#include <logging/TransactionalRecordQueue.hpp>

namespace {
    struct QueueMutexIdentity final {};

    void TestActualPoolOwnedPublicationOrderAndTeardown() {
        namespace L = ESPressio::Logging;
        namespace T = ESPressio::Threading;

        Resource resource;
        TestSupport::TestMutexProvider memoryMutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement,
            Framework::SelectUnique
        > allocator;
        Runtime runtime(memoryMutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) ==
            Memory::MemoryTopologyInitializationResult::Succeeded);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();

        using WakeOwner = T::BoundedWaitWakeProvider<
            TestSupport::TestSignalProvider, 2U
        >;
        WakeOwner wake;
        L::AdmissionWakeCoordinator<2U, WakeOwner> coordinator(wake);
        T::OrdinaryMutexProvider<
            QueueMutexIdentity, TestSupport::TestMutexProvider
        > queueMutex;
        using Queue = L::TransactionalRecordQueue<
            2U, PooledLease, decltype(queueMutex), decltype(coordinator)
        >;

        {
            Queue queue(queueMutex, coordinator);
            PooledLease first, second;
            assert(pool.Acquire(
                first, Platform::Synchronization::WaitTimeout::NoWait(), 11
            ) == Memory::ObjectPoolAcquisitionResult::Succeeded);
            assert(pool.Acquire(
                second, Platform::Synchronization::WaitTimeout::NoWait(), 22
            ) == Memory::ObjectPoolAcquisitionResult::Succeeded);
            assert(TestSupport::PooledObject::LiveCount.load() == 2);

            Queue::Ticket ticketOne, ticketTwo, current;
            assert(queue.Begin(first, ticketOne) == L::RecordQueueResult::Succeeded);
            assert(queue.Begin(second, ticketTwo) == L::RecordQueueResult::Succeeded);
            assert(first.IsEmpty() && second.IsEmpty());
            assert(queue.Commit(ticketTwo) == L::RecordQueueResult::Succeeded);
            assert(queue.Commit(ticketOne) == L::RecordQueueResult::Succeeded);
            assert(runtime.TearDown() != Memory::MemoryTopologyTeardownResult::Succeeded);

            assert(queue.Pop(current) == L::RecordQueueResult::Succeeded);
            assert(current.Slot == ticketTwo.Slot);
            const auto firstClaim = queue.Borrow(current);
            assert(firstClaim.Status() == L::RecordQueueResult::Succeeded &&
                   firstClaim.Lease()->Get()->Value() == 22);
            assert(queue.Complete(current) == L::RecordQueueResult::Succeeded);
            assert(TestSupport::PooledObject::LiveCount.load() == 1);

            assert(queue.Pop(current) == L::RecordQueueResult::Succeeded);
            assert(current.Slot == ticketOne.Slot);
            const auto secondClaim = queue.Borrow(current);
            assert(secondClaim.Status() == L::RecordQueueResult::Succeeded &&
                   secondClaim.Lease()->Get()->Value() == 11);
            assert(queue.Complete(current) == L::RecordQueueResult::Succeeded);
            assert(queue.Pop(current) == L::RecordQueueResult::Empty);

            PooledLease third;
            assert(pool.Acquire(
                third, Platform::Synchronization::WaitTimeout::NoWait(), 33
            ) == Memory::ObjectPoolAcquisitionResult::Succeeded);
            {
                Queue::Reservation reserved;
                assert(queue.Begin(third, reserved) == L::RecordQueueResult::Succeeded);
                assert(reserved.Borrow()->Get()->Value() == 33);
                assert(reserved.Commit() == L::RecordQueueResult::Succeeded);
                assert(!reserved.IsActive());
            }
            {
                Queue::ConsumerClaim consumed;
                assert(queue.Pop(consumed) == L::RecordQueueResult::Succeeded);
                assert(consumed.Borrow()->Get()->Value() == 33);
                // Consumer scope end automatically returns the real Memory lease.
            }
            assert(TestSupport::PooledObject::LiveCount.load() == 0);

            PooledLease fourth;
            assert(pool.Acquire(
                fourth, Platform::Synchronization::WaitTimeout::NoWait(), 44
            ) == Memory::ObjectPoolAcquisitionResult::Succeeded);
            {
                Queue::Reservation uncommitted;
                assert(queue.Begin(fourth, uncommitted) == L::RecordQueueResult::Succeeded);
                assert(uncommitted.Borrow()->Get()->Value() == 44);
                // Producer scope exit rolls back unpublished capacity.
            }
            assert(TestSupport::PooledObject::LiveCount.load() == 0);
        }

        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestConcurrentDuplicateTicketCannotDoubleRelease() {
        namespace L = ESPressio::Logging;
        namespace T = ESPressio::Threading;
        Resource resource;
        TestSupport::TestMutexProvider memoryMutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement,
            Framework::SelectUnique
        > allocator;
        Runtime runtime(memoryMutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) ==
            Memory::MemoryTopologyInitializationResult::Succeeded);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();

        using WakeOwner = T::BoundedWaitWakeProvider<
            TestSupport::TestSignalProvider, 2U
        >;
        WakeOwner wakes;
        L::AdmissionWakeCoordinator<2U, WakeOwner> original(wakes);
        struct PausingRelease final {
            L::AdmissionWakeCoordinator<2U, WakeOwner>* Base;
            std::atomic<bool> Entered{false};
            std::atomic<bool> Continue{false};
            L::CoordinatedReleaseResult ReleaseAndNotify(PooledLease& lease) noexcept {
                Entered.store(true);
                while (!Continue.load()) std::this_thread::yield();
                return Base->ReleaseAndNotify(lease);
            }
            L::AdmissionWakePublicationResult PublishCapacityAvailable() noexcept {
                return Base->PublishCapacityAvailable();
            }
        } release{&original};
        T::OrdinaryMutexProvider<
            QueueMutexIdentity, TestSupport::TestMutexProvider
        > queueMutex;
        using Queue = L::TransactionalRecordQueue<
            1U, PooledLease, decltype(queueMutex), PausingRelease
        >;
        Queue queue(queueMutex, release);
        PooledLease initial;
        assert(pool.Acquire(initial, Platform::Synchronization::WaitTimeout::NoWait(), 91)
            == Memory::ObjectPoolAcquisitionResult::Succeeded);
        Queue::Ticket reservation, consuming;
        assert(queue.Begin(initial, reservation) == L::RecordQueueResult::Succeeded);
        assert(queue.Commit(reservation) == L::RecordQueueResult::Succeeded);
        assert(queue.Pop(consuming) == L::RecordQueueResult::Succeeded);

        std::atomic<L::RecordQueueResult> completed{L::RecordQueueResult::Busy};
        std::thread worker([&] {
            completed.store(queue.Complete(consuming));
        });
        while (!release.Entered.load()) std::this_thread::yield();
        assert(queue.Complete(consuming) == L::RecordQueueResult::Busy);
        assert(TestSupport::PooledObject::LiveCount.load() == 1);
        release.Continue.store(true);
        worker.join();
        assert(completed.load() == L::RecordQueueResult::Succeeded);
        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        assert(queue.Complete(consuming) == L::RecordQueueResult::InvalidTicket);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }
}

int main() {
    const CanonicalClock clock;
    assert(ESPressio::Clock::BindMonotonicClock(clock));
    // Exercise every imported host fixture, not only the new integration.
    TestLatchedWakeBeforeWait();
    TestRealMemoryTopologyConcurrentReleases();
    TestRealTopologyExpiredDeadlineDoesNotClaimCapacity();
    TestFailedWakeCannotConcealReleasedCapacity();
    TestContendedMemoryMutexIsDeadlineBoundedEvenWithoutNotification();
    TestActualPoolOwnedPublicationOrderAndTeardown();
    TestConcurrentDuplicateTicketCannotDoubleRelease();
}
