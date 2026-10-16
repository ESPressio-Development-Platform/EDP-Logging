#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <thread>

// Reuse the authoritative EDP-Memory host providers, exact topology and
// platform latching-signal fixtures rather than maintaining a parallel copy.
#define main edp_memory_fixture_unused_main
#include <tests/host/main.cpp>
#undef main

#include <ESPressio_Clock.hpp>
#include <ESPressio_Threading.hpp>
#include <logging/AdmissionWakeCoordinator.hpp>

namespace {

    struct CanonicalClock final {
        ESPressio::Clock::MonotonicTimestamp Now() const noexcept {
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            return ESPressio::Clock::MonotonicTimestamp::FromNanoseconds(
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()
                )
            );
        }
    };

    ESPressio::Clock::MonotonicTimestamp DeadlineAfter(
        std::chrono::milliseconds duration
    ) {
        const auto now = ESPressio::Clock::MonotonicNow().Nanoseconds();
        return ESPressio::Clock::MonotonicTimestamp::FromNanoseconds(
            now + static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()
            )
        );
    }

    void TestRealMemoryTopologyConcurrentReleases() {
        namespace Logging = ESPressio::Logging;
        namespace Threading = ESPressio::Threading;
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
        PooledLease initial0;
        PooledLease initial1;
        assert(pool.Acquire(
            initial0, Platform::Synchronization::WaitTimeout::NoWait(), 11
        ) == Memory::ObjectPoolAcquisitionResult::Succeeded);
        assert(pool.Acquire(
            initial1, Platform::Synchronization::WaitTimeout::NoWait(), 12
        ) == Memory::ObjectPoolAcquisitionResult::Succeeded);
        assert(TestSupport::PooledObject::LiveCount.load() == 2);

        using WakeOwner = Threading::BoundedWaitWakeProvider<TestSupport::TestSignalProvider, 2U>;
        WakeOwner wakes;
        Logging::AdmissionWakeCoordinator<2U, WakeOwner> coordinator(wakes);

        std::atomic<int> entered{0};
        std::atomic<int> acquired{0};
        std::atomic<int> errors{0};

        auto producer = [&](std::size_t slot) {
            PooledLease owned;
            ++entered;
            const auto admission = coordinator.AcquireUntil<32U>(
                slot, pool, owned, DeadlineAfter(std::chrono::milliseconds(1000)),
                30 + static_cast<int>(slot)
            );
            if (
                admission.Admission != Logging::AdmissionWaitResult::Admitted ||
                admission.Notification != Logging::AdmissionWakePublicationResult::Published ||
                owned.IsEmpty()
            ) {
                ++errors;
                return;
            }
            ++acquired;
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
            if (coordinator.ReleaseAndNotify(owned) !=
                Logging::CoordinatedReleaseResult::ReleasedAndNotified) {
                ++errors;
            }
        };
        std::thread one(producer, 0U);
        std::thread two(producer, 1U);
        while (entered.load() != 2) { std::this_thread::yield(); }

        // Release while both producers may already be asleep. A latched signal
        // must also cover release-before-the-actual WaitUntil.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        assert(
            coordinator.ReleaseAndNotify(initial0) ==
            Logging::CoordinatedReleaseResult::ReleasedAndNotified
        );
        assert(
            coordinator.ReleaseAndNotify(initial1) ==
            Logging::CoordinatedReleaseResult::ReleasedAndNotified
        );
        one.join();
        two.join();
        assert(errors.load() == 0);
        assert(acquired.load() == 2);
        assert(TestSupport::PooledObject::LiveCount.load() == 0);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestRealTopologyExpiredDeadlineDoesNotClaimCapacity() {
        namespace Logging = ESPressio::Logging;
        namespace Threading = ESPressio::Threading;
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
        using WakeOwner = Threading::BoundedWaitWakeProvider<TestSupport::TestSignalProvider, 2U>;
        WakeOwner wakes;
        Logging::AdmissionWakeCoordinator<2U, WakeOwner> coordinator(wakes);
        PooledLease expired;
        const auto outcome = coordinator.AcquireUntil<2U>(
            0U, pool, expired, DeadlineAfter(std::chrono::milliseconds(0)), 99
        );
        assert(outcome.Admission == Logging::AdmissionWaitResult::DeadlineElapsed);
        assert(expired.IsEmpty());
        assert(outcome.Notification == Logging::AdmissionWakePublicationResult::NotAttempted);
        const auto invalidSlot = coordinator.AcquireUntil<2U>(
            3U, pool, expired, DeadlineAfter(std::chrono::milliseconds(50)), 100
        );
        assert(invalidSlot.Admission == Logging::AdmissionWaitResult::InvalidProducerSlot);
        assert(invalidSlot.Notification == Logging::AdmissionWakePublicationResult::NotAttempted);
        assert(expired.IsEmpty());
        assert(
            coordinator.ReleaseAndNotify(expired) ==
            Logging::CoordinatedReleaseResult::AlreadyEmpty
        );
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestLatchedWakeBeforeWait() {
        namespace Threading = ESPressio::Threading;
        using WakeOwner = Threading::BoundedWaitWakeProvider<TestSupport::TestSignalProvider, 2U>;
        WakeOwner wakes;
        assert(wakes.Wake(1U) == Threading::BoundedWaitWakeResult::Woken);
        assert(wakes.WaitUntil(
            1U, DeadlineAfter(std::chrono::milliseconds(200))
        ) == Threading::BoundedWaitWakeResult::Woken);
    }


    struct FailingWakeOwner final {
        static constexpr std::size_t Capacity = 2U;
        bool FailSlotOne = false;
        unsigned Attempts = 0U;

        ESPressio::Threading::BoundedWaitWakeResult Wake(std::size_t slot) noexcept {
            ++Attempts;
            if (slot == 1U && FailSlotOne) {
                return ESPressio::Threading::BoundedWaitWakeResult::ProviderFailure;
            }
            return ESPressio::Threading::BoundedWaitWakeResult::Woken;
        }

        ESPressio::Threading::BoundedWaitWakeResult WaitUntil(
            std::size_t, ESPressio::Clock::MonotonicTimestamp
        ) noexcept {
            return ESPressio::Threading::BoundedWaitWakeResult::TimedOut;
        }
    };

    void TestFailedWakeCannotConcealReleasedCapacity() {
        namespace Logging = ESPressio::Logging;
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
        PooledLease owned;
        assert(pool.Acquire(
            owned, Platform::Synchronization::WaitTimeout::NoWait(), 91
        ) == Memory::ObjectPoolAcquisitionResult::Succeeded);

        FailingWakeOwner wakeOwner;
        wakeOwner.FailSlotOne = true;
        Logging::AdmissionWakeCoordinator<2U, FailingWakeOwner> coordinator(wakeOwner);
        const auto result = coordinator.ReleaseAndNotify(owned);
        assert(result == Logging::CoordinatedReleaseResult::ReleasedNotificationFailed);
        assert(owned.IsEmpty());
        assert(wakeOwner.Attempts == 2U);

        PooledLease retryable;
        assert(pool.Acquire(
            retryable, Platform::Synchronization::WaitTimeout::NoWait(), 92
        ) == Memory::ObjectPoolAcquisitionResult::Succeeded);
        mutex.FailNextAcquire();
        const auto attemptsBeforeFailedRelease = wakeOwner.Attempts;
        assert(
            coordinator.ReleaseAndNotify(retryable) ==
            Logging::CoordinatedReleaseResult::MemoryProviderFailure
        );
        assert(!retryable.IsEmpty());
        assert(wakeOwner.Attempts == attemptsBeforeFailedRelease);
        assert(
            coordinator.ReleaseAndNotify(retryable) ==
            Logging::CoordinatedReleaseResult::ReleasedNotificationFailed
        );
        assert(retryable.IsEmpty());
        assert(wakeOwner.Attempts == attemptsBeforeFailedRelease + 2U);
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestContendedMemoryMutexIsDeadlineBoundedEvenWithoutNotification() {
        namespace Logging = ESPressio::Logging;
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
        FailingWakeOwner wakeOwner;
        Logging::AdmissionWakeCoordinator<2U, FailingWakeOwner> coordinator(wakeOwner);

        std::atomic<bool> held{false};
        std::thread owner([&] {
            assert(mutex.Acquire(Platform::Synchronization::WaitTimeout::Forever())
                == Platform::Synchronization::LockAcquireResult::Acquired);
            held.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(65));
            assert(mutex.Release() == Platform::Synchronization::LockReleaseResult::Released);
        });

        while (!held.load()) { std::this_thread::yield(); }
        PooledLease denied;
        const auto deadline = DeadlineAfter(std::chrono::milliseconds(15));
        const auto result = coordinator.AcquireUntil<3U>(0U, pool, denied, deadline, 1);
        assert(result.Admission == Logging::AdmissionWaitResult::DeadlineElapsed);
        assert(denied.IsEmpty());
        owner.join();
        assert(runtime.TearDown() == Memory::MemoryTopologyTeardownResult::Succeeded);
    }

} // anonymous

int main() {
    const CanonicalClock clock;
    assert(ESPressio::Clock::BindMonotonicClock(clock));
    TestLatchedWakeBeforeWait();
    for (unsigned iteration = 0U; iteration < 12U; ++iteration) {
        TestRealMemoryTopologyConcurrentReleases();
    }
    TestRealTopologyExpiredDeadlineDoesNotClaimCapacity();
    TestFailedWakeCannotConcealReleasedCapacity();
    TestContendedMemoryMutexIsDeadlineBoundedEvenWithoutNotification();
}
