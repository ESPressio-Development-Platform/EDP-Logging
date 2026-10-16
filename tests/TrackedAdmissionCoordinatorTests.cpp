#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

#define EDP_LOGGING_G1_LIFECYCLE_TEST_INCLUDE
#include "AdmissionWakeCoordinatorTests.cpp"

#include <logging/TrackedAdmissionCoordinator.hpp>

namespace {

    using ThreadingOwner = ESPressio::Threading::BoundedWaitWakeProvider<
        TestSupport::TestSignalProvider, 4U
    >;
    using Tracked = ESPressio::Logging::TrackedAdmissionCoordinator<3U, ThreadingOwner>;
    using TrackedLease = Tracked::TrackedLease<PooledLease>;

    void TestScopeReleaseNotifiesAndPreservesAdmissionOwnership() {
        namespace Logging = ESPressio::Logging;
        Resource resource;
        TestSupport::TestMutexProvider mutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement,
            Framework::SelectUnique
        > allocator;
        Runtime runtime(mutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) ==
               Memory::MemoryTopologyInitializationResult::Succeeded);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();

        ThreadingOwner wakes;
        Tracked coordinator(wakes);
        std::atomic<bool> started{false};
        std::atomic<bool> finished{false};
        std::atomic<bool> wrongOutcome{false};

        {
            TrackedLease first(coordinator);
            TrackedLease second(coordinator);

            const auto one = coordinator.AcquireUntil<32U>(
                0U, pool, first, DeadlineAfter(std::chrono::seconds(1)), 10
            );
            const auto two = coordinator.AcquireUntil<32U>(
                1U, pool, second, DeadlineAfter(std::chrono::seconds(1)), 20
            );
            assert(one.Validation == Logging::TrackedAdmissionValidation::Valid);
            assert(one.Admission.Admission == Logging::AdmissionWaitResult::Admitted);
            assert(two.Admission.Admission == Logging::AdmissionWaitResult::Admitted);
            assert(coordinator.OutstandingLeases() == 2U);

            const auto occupied = coordinator.AcquireUntil<32U>(
                0U, pool, first, DeadlineAfter(std::chrono::seconds(1)), 50
            );
            assert(occupied.Validation == Logging::TrackedAdmissionValidation::LeaseOccupied);
            assert(coordinator.OutstandingLeases() == 2U);

            std::thread consumer([&] {
                TrackedLease third(coordinator);
                started.store(true);
                const auto outcome = coordinator.AcquireUntil<32U>(
                    2U, pool, third, DeadlineAfter(std::chrono::milliseconds(750)), 30
                );
                if (
                    outcome.Admission.Admission != Logging::AdmissionWaitResult::Admitted ||
                    third.IsEmpty()
                ) {
                    wrongOutcome.store(true);
                }
                // RAII closure of third returns its real Memory capacity;
                // downstream consumer never sees the native lease.
                finished.store(true);
            });

            while (!started.load()) std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            // The previously held lease has NO manual release: RAII ensures
            // coordinated native Memory release and wakes the blocked producer.
            assert(first.Release() == Logging::CoordinatedReleaseResult::ReleasedAndNotified);
            consumer.join();
            assert(finished.load() && !wrongOutcome.load());
            assert(coordinator.OutstandingLeases() == 1U);
            assert(!second.IsEmpty());
            assert(TestSupport::PooledObject::LiveCount.load() == 1);

            const auto blocked = coordinator.ShutdownTrackedUntil<32U>(
                DeadlineAfter(std::chrono::milliseconds(30))
            );
            assert(blocked.Status == Logging::TrackedDrainStatus::DeadlineElapsed);
            assert(blocked.OutstandingLeases == 1U);
            assert(blocked.ActiveAcquisitions == 0U);
            // second destructor must automatically return and notify.
        }

        const auto drained = coordinator.ShutdownTrackedUntil<32U>(
            DeadlineAfter(std::chrono::milliseconds(100))
        );
        assert(drained.Status == Logging::TrackedDrainStatus::Quiescent);
        assert(drained.OutstandingLeases == 0U);
        assert(drained.UnconfirmedReleases == 0U);
        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestDestructorNotifiesSleepingProducer() {
        namespace Logging = ESPressio::Logging;
        Resource resource;
        TestSupport::TestMutexProvider mutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement,
            Framework::SelectUnique
        > allocator;
        Runtime runtime(mutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) ==
               Memory::MemoryTopologyInitializationResult::Succeeded);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();
        ThreadingOwner wakes;
        Tracked coordinator(wakes);
        TrackedLease persistent(coordinator);
        assert(coordinator.AcquireUntil<32U>(
            0U, pool, persistent, DeadlineAfter(std::chrono::seconds(1)), 10
        ).Admission.Admission == Logging::AdmissionWaitResult::Admitted);
        std::atomic<bool> entered{false};
        std::atomic<bool> received{false};
        std::thread consumer;
        {
            TrackedLease scopeLease(coordinator);
            assert(coordinator.AcquireUntil<32U>(
                1U, pool, scopeLease, DeadlineAfter(std::chrono::seconds(1)), 20
            ).Admission.Admission == Logging::AdmissionWaitResult::Admitted);

            consumer = std::thread([&] {
                TrackedLease acquired(coordinator);
                entered.store(true);
                const auto result = coordinator.AcquireUntil<32U>(
                    2U, pool, acquired, DeadlineAfter(std::chrono::milliseconds(750)), 30
                );
                received.store(
                    result.Admission.Admission == Logging::AdmissionWaitResult::Admitted &&
                    !acquired.IsEmpty()
                );
            });
            while (!entered.load()) std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            assert(coordinator.OutstandingLeases() == 2U);
            // The scopeLease destructor is the ONLY capacity-release path here.
        }
        consumer.join();
        assert(received.load());
        assert(coordinator.OutstandingLeases() == 1U);
        assert(
            persistent.Release() ==
            Logging::CoordinatedReleaseResult::ReleasedAndNotified
        );
        const auto shutdown = coordinator.ShutdownTrackedUntil<32U>(
            DeadlineAfter(std::chrono::milliseconds(100))
        );
        assert(shutdown.Status == Logging::TrackedDrainStatus::Quiescent);
        assert(shutdown.UnconfirmedReleases == 0U);
        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestFailedNativeReleaseIsNeverCountedAsCleanShutdown() {
        namespace Logging = ESPressio::Logging;
        Resource resource;
        TestSupport::TestMutexProvider mutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement,
            Framework::SelectUnique
        > allocator;
        Runtime runtime(mutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) ==
               Memory::MemoryTopologyInitializationResult::Succeeded);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();
        ThreadingOwner wakes;
        Tracked coordinator(wakes);
        {
            TrackedLease guard(coordinator);
            const auto admitted = coordinator.AcquireUntil<8U>(
                0U, pool, guard, DeadlineAfter(std::chrono::milliseconds(100)), 45
            );
            assert(admitted.Admission.Admission == Logging::AdmissionWaitResult::Admitted);
            assert(coordinator.OutstandingLeases() == 1U);
            mutex.FailNextAcquire();
            // The guard's release fails; native EDP-Memory lease destructor
            // MAY silently release after it. This uncertainty cannot be hidden.
        }
        assert(coordinator.UnconfirmedReleases() == 1U);
        assert(coordinator.OutstandingLeases() == 1U);
        const auto shutdown = coordinator.ShutdownTrackedUntil<8U>(
            DeadlineAfter(std::chrono::milliseconds(100))
        );
        assert(shutdown.Status == Logging::TrackedDrainStatus::UnconfirmedNativeRelease);
        assert(shutdown.UnconfirmedReleases == 1U);
        assert(shutdown.OutstandingLeases == 1U);
        // Native RAII retry can succeed, but we correctly retain a false-
        // teardown prevention marker until the entire runtime is replaced.
        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestWrongOwnerNeverTouchesPool() {
        namespace Logging = ESPressio::Logging;
        ThreadingOwner wake1;
        ThreadingOwner wake2;
        Tracked owner1(wake1);
        Tracked owner2(wake2);
        TrackedLease wrong(owner1);
        struct FakePool {
            unsigned Calls{0U};
            Memory::ObjectPoolAcquisitionResult Acquire(
                PooledLease&, Platform::Synchronization::WaitTimeout, int
            ) noexcept {
                ++Calls;
                return Memory::ObjectPoolAcquisitionResult::CapacityUnavailable;
            }
        } pool;
        const auto result = owner2.AcquireUntil<2U>(
            0U, pool, wrong, DeadlineAfter(std::chrono::milliseconds(100)), 1
        );
        assert(result.Validation == Logging::TrackedAdmissionValidation::WrongCoordinator);
        assert(pool.Calls == 0U);
    }
}

int main() {
    const CanonicalClock clock;
    assert(ESPressio::Clock::BindMonotonicClock(clock));
    TestLatchedWakeBeforeWait();
    TestRealMemoryTopologyConcurrentReleases();
    TestRealTopologyExpiredDeadlineDoesNotClaimCapacity();
    TestFailedWakeCannotConcealReleasedCapacity();
    TestContendedMemoryMutexIsDeadlineBoundedEvenWithoutNotification();
    for (unsigned iteration=0; iteration<10U; ++iteration) {
        TestScopeReleaseNotifiesAndPreservesAdmissionOwnership();
    }
    for (unsigned iteration=0; iteration<10U; ++iteration) {
        TestDestructorNotifiesSleepingProducer();
    }
    TestFailedNativeReleaseIsNeverCountedAsCleanShutdown();
    TestWrongOwnerNeverTouchesPool();
}
