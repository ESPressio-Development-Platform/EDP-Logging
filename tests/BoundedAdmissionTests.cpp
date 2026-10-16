#include <cassert>
#include <cstdint>
#include <type_traits>

#include <logging/BoundedAdmission.hpp>

namespace Test {

    struct ClockSource final {
        mutable std::uint64_t Value = 1U;

        ESPressio::Clock::MonotonicTimestamp Now() const noexcept {
            return ESPressio::Clock::MonotonicTimestamp::FromNanoseconds(Value);
        }
    };

    ClockSource Source{};

    struct Lease final {
        bool Occupied = false;
    };

    struct Pool final {
        using Result = ESPressio::Memory::ObjectPoolAcquisitionResult;
        Result Next = Result::Succeeded;
        Result AfterFirst = Result::Succeeded;
        unsigned Calls = 0U;
        bool ObservedNoWait = true;
        bool ExpireDuringAcquire = false;

        Result Acquire(
            Lease& lease,
            ESPressio::Platform::Synchronization::WaitTimeout timeout,
            int /* marker */
        ) noexcept {
            ++Calls;
            ObservedNoWait &= timeout.IsNoWait();
            if (ExpireDuringAcquire) {
                Source.Value = 101U;
            }
            const auto result = Calls == 1U ? Next : AfterFirst;
            if (result == Result::Succeeded) lease.Occupied = true;
            return result;
        }
    };

    struct WaitPoint final {
        ESPressio::Threading::BoundedWaitResult Outcome =
            ESPressio::Threading::BoundedWaitResult::Woken;
        unsigned Calls = 0U;
        std::uint64_t LastDeadline = 0U;
        bool ExpireOnWait = false;

        ESPressio::Threading::BoundedWaitResult WaitUntil(
            ESPressio::Clock::MonotonicTimestamp deadline
        ) noexcept {
            ++Calls;
            LastDeadline = deadline.Nanoseconds();
            if (ExpireOnWait) Source.Value = LastDeadline;
            return Outcome;
        }
    };

    void TestImmediateAndWake() {
        using namespace ESPressio;
        Source.Value = 1U;
        Lease lease;
        Pool pool;
        WaitPoint wait;

        assert(
            Logging::AcquirePoolUntil<5U>(
                pool, wait, lease, Clock::MonotonicTimestamp::FromNanoseconds(100U), 5
            ) == Logging::AdmissionWaitResult::Admitted
        );
        assert(lease.Occupied && pool.ObservedNoWait);
        assert(pool.Calls == 1U && wait.Calls == 0U);

        lease.Occupied = false;
        pool = Pool{};
        pool.Next = Memory::ObjectPoolAcquisitionResult::CapacityUnavailable;
        assert(
            Logging::AcquirePoolUntil<5U>(
                pool, wait, lease, Clock::MonotonicTimestamp::FromNanoseconds(100U), 5
            ) == Logging::AdmissionWaitResult::Admitted
        );
        assert(lease.Occupied && pool.Calls == 2U && pool.ObservedNoWait);
        assert(wait.Calls == 1U && wait.LastDeadline == 100U);
    }

    void TestExpiryAndFailure() {
        using namespace ESPressio;
        Pool pool;
        Lease lease;
        WaitPoint wait;
        Source.Value = 100U;
        assert(
            Logging::AcquirePoolUntil<3U>(
                pool, wait, lease, Clock::MonotonicTimestamp::FromNanoseconds(100U), 5
            ) == Logging::AdmissionWaitResult::DeadlineElapsed
        );
        assert(pool.Calls == 0U);

        Source.Value = 1U;
        pool.Next = Memory::ObjectPoolAcquisitionResult::CapacityUnavailable;
        wait.Outcome = Threading::BoundedWaitResult::TimedOut;
        assert(
            Logging::AcquirePoolUntil<3U>(
                pool, wait, lease, Clock::MonotonicTimestamp::FromNanoseconds(100U), 5
            ) == Logging::AdmissionWaitResult::DeadlineElapsed
        );
        assert(!lease.Occupied);

        pool = Pool{};
        pool.Next = Memory::ObjectPoolAcquisitionResult::CapacityUnavailable;
        wait.Outcome = Threading::BoundedWaitResult::ProviderFailure;
        assert(
            Logging::AcquirePoolUntil<3U>(
                pool, wait, lease, Clock::MonotonicTimestamp::FromNanoseconds(100U), 5
            ) == Logging::AdmissionWaitResult::WaitProviderFailure
        );

        pool = Pool{};
        pool.Next = Memory::ObjectPoolAcquisitionResult::ProviderFailure;
        assert(
            Logging::AcquirePoolUntil<3U>(
                pool, wait, lease, Clock::MonotonicTimestamp::FromNanoseconds(100U), 5
            ) == Logging::AdmissionWaitResult::MemoryProviderFailure
        );
    }

    void TestLateGrantIsNeverHidden() {
        using namespace ESPressio;
        Source.Value = 1U;
        Pool pool;
        pool.ExpireDuringAcquire = true;
        Lease lease;
        WaitPoint wait;
        assert(
            Logging::AcquirePoolUntil<3U>(
                pool, wait, lease, Clock::MonotonicTimestamp::FromNanoseconds(100U), 5
            ) == Logging::AdmissionWaitResult::GrantedAfterDeadline
        );
        assert(lease.Occupied);
        assert(wait.Calls == 0U);
    }

    void TestFiniteSpuriousWakeBudget() {
        using namespace ESPressio;
        Source.Value = 1U;
        Pool pool;
        pool.Next = Memory::ObjectPoolAcquisitionResult::CapacityUnavailable;
        pool.AfterFirst = Memory::ObjectPoolAcquisitionResult::CapacityUnavailable;
        Lease lease;
        WaitPoint wait;
        assert(
            Logging::AcquirePoolUntil<3U>(
                pool, wait, lease, Clock::MonotonicTimestamp::FromNanoseconds(100U), 5
            ) == Logging::AdmissionWaitResult::WakeRetryLimitReached
        );
        assert(pool.Calls == 3U && wait.Calls == 3U);
        assert(!lease.Occupied);
    }

} // Test

int main() {
    const bool clockBound = ESPressio::Clock::BindMonotonicClock(Test::Source);
    assert(clockBound);
    Test::TestImmediateAndWake();
    Test::TestExpiryAndFailure();
    Test::TestLateGrantIsNeverHidden();
    Test::TestFiniteSpuriousWakeBudget();
}
