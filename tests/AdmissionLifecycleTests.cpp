#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

#define EDP_LOGGING_G1_LIFECYCLE_TEST_INCLUDE
#include "AdmissionWakeCoordinatorTests.cpp"

#include <logging/AdmissionLifecycleCoordinator.hpp>

namespace {

    using WakeOwner = ESPressio::Threading::BoundedWaitWakeProvider<
        TestSupport::TestSignalProvider, 3U
    >;
    using Lifecycle = ESPressio::Logging::AdmissionLifecycleCoordinator<2U, WakeOwner>;

    void TestConcurrentShutdownReleasesBlockedAdmission() {
        namespace Logging = ESPressio::Logging;
        Resource resource;
        TestSupport::TestMutexProvider mutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement,
            Framework::SelectUnique
        > allocator;
        Runtime runtime(mutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(
            runtime.Initialize(failure) ==
            Memory::MemoryTopologyInitializationResult::Succeeded
        );
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();
        PooledLease held0;
        PooledLease held1;
        assert(
            pool.Acquire(
                held0, Platform::Synchronization::WaitTimeout::NoWait(), 11
            ) == Memory::ObjectPoolAcquisitionResult::Succeeded
        );
        assert(
            pool.Acquire(
                held1, Platform::Synchronization::WaitTimeout::NoWait(), 12
            ) == Memory::ObjectPoolAcquisitionResult::Succeeded
        );
        WakeOwner wakes;
        Lifecycle lifecycle(wakes);

        std::atomic<unsigned> entered{0U};
        std::atomic<unsigned> completed{0U};
        std::atomic<unsigned> unexpected{0U};
        const auto run = [&](std::size_t slot) {
            PooledLease lease;
            ++entered;
            const auto result = lifecycle.AcquireUntil<32U>(
                slot, pool, lease, DeadlineAfter(std::chrono::seconds(5)),
                static_cast<int>(30U + slot)
            );
            if (
                result.Admission != Logging::AdmissionWaitResult::ShutdownRequested ||
                !lease.IsEmpty()
            ) {
                ++unexpected;
                if (!lease.IsEmpty()) {
                    static_cast<void>(lifecycle.ReleaseAndNotify(lease));
                }
            }
            ++completed;
        };
        std::thread a(run, 0U);
        std::thread b(run, 1U);
        while (entered.load() != 2U || lifecycle.ActiveAdmissions() != 2U) {
            std::this_thread::yield();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const auto drain = lifecycle.ShutdownUntil<24U>(
            DeadlineAfter(std::chrono::milliseconds(1000))
        );
        a.join();
        b.join();
        assert(drain.Status == Logging::AdmissionShutdownStatus::Quiescent);
        assert(drain.WakePublication == Logging::AdmissionWakePublicationResult::Published);
        assert(drain.ActiveAdmissions == 0U);
        assert(completed.load() == 2U);
        assert(unexpected.load() == 0U);
        assert(lifecycle.ActiveAdmissions() == 0U);
        assert(lifecycle.ShutdownRequested());
        // Quiescence means no in-flight admission calls: both previously granted
        // leases are still OWNED and must be released before Memory teardown.
        assert(!held0.IsEmpty() && !held1.IsEmpty());
        assert(
            lifecycle.ReleaseAndNotify(held0) ==
            Logging::CoordinatedReleaseResult::ReleasedAndNotified
        );
        assert(
            lifecycle.ReleaseAndNotify(held1) ==
            Logging::CoordinatedReleaseResult::ReleasedAndNotified
        );
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestRequestShutdownRejectsFutureAdmission() {
        namespace Logging = ESPressio::Logging;
        WakeOwner wakes;
        Lifecycle lifecycle(wakes);
        assert(
            lifecycle.RequestShutdown() ==
            Logging::AdmissionWakePublicationResult::Published
        );
        Resource resource;
        TestSupport::TestMutexProvider mutex;
        MemoryComposition::Select<
            Memory::SharedReserveAllocationRequirement,
            Framework::SelectUnique
        > allocator;
        Runtime runtime(mutex, allocator, resource);
        Memory::MemoryTopologyInitializationFailure failure;
        assert(runtime.Initialize(failure) == Memory::MemoryTopologyInitializationResult::Succeeded);
        auto& pool = runtime.ObjectPoolFor<TestSupport::PooledObject>();
        PooledLease lease;
        const auto result = lifecycle.AcquireUntil<2U>(
            0U, pool, lease, DeadlineAfter(std::chrono::seconds(1)), 30
        );
        assert(result.Admission == Logging::AdmissionWaitResult::ShutdownRequested);
        assert(result.Notification == Logging::AdmissionWakePublicationResult::NotAttempted);
        assert(lease.IsEmpty());
        const auto invalid = lifecycle.AcquireUntil<2U>(
            3U, pool, lease, DeadlineAfter(std::chrono::seconds(1)), 31
        );
        assert(invalid.Admission == Logging::AdmissionWaitResult::InvalidProducerSlot);
        assert(lifecycle.ActiveAdmissions() == 0U);
        const auto drain = lifecycle.ShutdownUntil<2U>(
            DeadlineAfter(std::chrono::milliseconds(100))
        );
        assert(drain.Status == Logging::AdmissionShutdownStatus::Quiescent);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestGrantRacingShutdownRetainsOwnership() {
        namespace Logging = ESPressio::Logging;
        WakeOwner wakes;
        Lifecycle lifecycle(wakes);

        struct Lease {
            bool Occupied{false};
        };

        struct Pool {
            Lifecycle* Owner;
            Memory::ObjectPoolAcquisitionResult Acquire(
                Lease& lease,
                Platform::Synchronization::WaitTimeout timeout,
                int
            ) noexcept {
                assert(timeout.IsNoWait());
                lease.Occupied = true;
                static_cast<void>(Owner->RequestShutdown());
                return Memory::ObjectPoolAcquisitionResult::Succeeded;
            }
        } pool{&lifecycle};

        Lease lease;
        const auto result = lifecycle.AcquireUntil<2U>(
            0U, pool, lease, DeadlineAfter(std::chrono::seconds(1)), 15
        );
        assert(result.Admission == Logging::AdmissionWaitResult::GrantedDuringShutdown);
        assert(result.Notification == Logging::AdmissionWakePublicationResult::Published);
        assert(lease.Occupied);
        assert(lifecycle.ActiveAdmissions() == 0U);
    }

    void TestFailedShutdownWakeCannotFalselyQuiesce() {
        namespace Logging = ESPressio::Logging;
        namespace Threading = ESPressio::Threading;

        struct FaultedWaitWake final {
            enum : std::size_t { Capacity = 3U };
            std::atomic<bool> InProducerWait{false};

            Threading::BoundedWaitWakeResult Wake(std::size_t) noexcept {
                return Threading::BoundedWaitWakeResult::ProviderFailure;
            }
            Threading::BoundedWaitWakeResult WaitUntil(
                std::size_t slot, ESPressio::Clock::MonotonicTimestamp
            ) noexcept {
                if (slot == 2U) {
                    return Threading::BoundedWaitWakeResult::TimedOut;
                }
                InProducerWait.store(true);
                std::this_thread::sleep_for(std::chrono::milliseconds(85));
                return Threading::BoundedWaitWakeResult::TimedOut;
            }
        } owner;
        Logging::AdmissionLifecycleCoordinator<2U, FaultedWaitWake> lifecycle(owner);

        struct Lease { bool Occupied{false}; } lease;
        struct Pool {
            Memory::ObjectPoolAcquisitionResult Acquire(
                Lease&, Platform::Synchronization::WaitTimeout timeout, int
            ) noexcept {
                assert(timeout.IsNoWait());
                return Memory::ObjectPoolAcquisitionResult::CapacityUnavailable;
            }
        } pool;

        std::atomic<bool> completed{false};
        std::thread worker([&] {
            const auto admission = lifecycle.AcquireUntil<5U>(
                0U, pool, lease, DeadlineAfter(std::chrono::seconds(1)), 7
            );
            assert(
                admission.Admission == Logging::AdmissionWaitResult::ShutdownRequested ||
                admission.Admission == Logging::AdmissionWaitResult::DeadlineElapsed
            );
            completed.store(true);
        });
        while (!owner.InProducerWait.load() || lifecycle.ActiveAdmissions() != 1U) {
            std::this_thread::yield();
        }

        const auto first = lifecycle.ShutdownUntil<3U>(
            DeadlineAfter(std::chrono::milliseconds(10))
        );
        assert(first.WakePublication == Logging::AdmissionWakePublicationResult::ProviderFailure);
        assert(first.Status == Logging::AdmissionShutdownStatus::DeadlineElapsed);
        assert(first.ActiveAdmissions == 1U);
        assert(!completed.load());
        worker.join();
        assert(completed.load());
        assert(lifecycle.ActiveAdmissions() == 0U);

        const auto retried = lifecycle.ShutdownUntil<3U>(
            DeadlineAfter(std::chrono::milliseconds(100))
        );
        assert(retried.Status == Logging::AdmissionShutdownStatus::Quiescent);
        assert(retried.WakePublication == Logging::AdmissionWakePublicationResult::ProviderFailure);
        assert(!lease.Occupied);
    }

}

int main() {
    const CanonicalClock clock;
    assert(ESPressio::Clock::BindMonotonicClock(clock));
    // Reuse all original G1 tests so the included real Memory owner fixture
    // cannot silently omit prior owner/negative coverage.
    TestLatchedWakeBeforeWait();
    TestRealMemoryTopologyConcurrentReleases();
    TestRealTopologyExpiredDeadlineDoesNotClaimCapacity();
    TestFailedWakeCannotConcealReleasedCapacity();
    TestContendedMemoryMutexIsDeadlineBoundedEvenWithoutNotification();
    for (unsigned i=0U;i<15U;++i) {
        TestConcurrentShutdownReleasesBlockedAdmission();
    }
    TestRequestShutdownRejectsFutureAdmission();
    TestGrantRacingShutdownRetainsOwnership();
    TestFailedShutdownWakeCannotFalselyQuiesce();
}
