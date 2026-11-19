// Coordinator control-flow tests reuse the real Memory queue fixtures.
// The ThreadContext operation table is deterministic test-only time/wake evidence.
#define EDP_LOGGING_RECORD_PRODUCTION_TEST_REUSE
#include "RecordProductionTests.cpp"
#undef EDP_LOGGING_RECORD_PRODUCTION_TEST_REUSE

namespace {


    /// Existing real Memory runtime, queue and admission lifecycle owned by one test.
    struct ServiceFixture final {
        Resource ResourceOwner{};
        TestSupport::TestMutexProvider Native{};
        MemoryComposition::Select<M::SharedReserveAllocationRequirement,
                                  ESPressio::System::CompositionFramework::SelectUnique>
            Allocator{};
        Runtime2 Memory;
        Wake WakeOwner{};
        Life Lifecycle;
        Mutex QueueMutex{};
        Queue Records;
        Sequencer SequenceMutex{};
        L::AttemptSequenceCounter<> Sequence{};
        Temporal Time{};

        ServiceFixture()
            : Memory(
                Native,
                Allocator,
                ResourceOwner
            ), Lifecycle(WakeOwner),
              Records(
                  QueueMutex,
                  Lifecycle
              ) {
            M::MemoryTopologyInitializationFailure failure{};
            assert(Memory.Initialize(failure) == M::MemoryTopologyInitializationResult::Succeeded);
        }
        ~ServiceFixture() {
            assert(Rec::Live.load() == 0);
            assert(
                Lifecycle.ShutdownUntil<8U>(DeadlineAfter(std::chrono::milliseconds(300))).Status ==
                L::AdmissionShutdownStatus::Quiescent);
            assert(Memory.TearDown() == M::MemoryTopologyTeardownResult::Succeeded);
        }
        void Produce(int value) {
            Lease scratch;
            Queue::Reservation reservation;
            const auto running = []() noexcept -> bool { return false; };
            const auto produced = L::ProduceRecordUntil<L::AttemptSequenceWidth::Bits32, 4U, 4U>(
                Sequence,
                SequenceMutex,
                Time,
                Records,
                Lifecycle,
                WakeOwner,
                0U,
                Memory.ObjectPoolFor<Rec>(),
                scratch,
                reservation,
                DeadlineAfter(std::chrono::milliseconds(300)),
                running,
                value
            );
            assert(produced.Status == L::RecordProductionStatus::Published);
        }
    };

    /// Test-only owned RAM sink and controllable completion policy; no file/NVS claim.
    struct ServicedSink final {
        std::uint64_t Deadline{0U};
        std::uint64_t Pending{0U};
        std::uint64_t Accepted{0U};
        unsigned Polls{0U};
        unsigned CompleteAfter{0U};
        int LastValue{0};
        bool Due{false};
        bool Fatal{false};

        L::RecordSinkSubmission operator()(const Rec& record) noexcept {
            LastValue = record.Value;
            ++Accepted;
            ++Pending;
            return L::RecordSinkSubmission::Accepted;
        }
        L::CoordinatorSinkServiceOutcome ServiceCoordinator() noexcept {
            ++Polls;
            L::CoordinatorSinkServiceOutcome result{};
            if (Fatal) {
                result.Status = L::CoordinatorSinkServiceStatus::RecoveryRequired;
            } else if (Pending != 0U &&
                       (Due || (CompleteAfter != 0U && Accepted >= CompleteAfter))) {
                result.DurableSinkCompletions = Pending;
                Pending = 0U;
            } else if (Pending != 0U) {
                result.NextDeadline = C::MonotonicTimestamp::FromNanoseconds(Deadline);
            }
            return result;
        }
    };

    /// One operation-table context; ordinary wake and absolute-deadline paths stay distinct.
    struct ServiceContext final {
        bool Stop{false};
        bool FailAbsoluteWait{false};
        unsigned RelativeWaits{0U};
        unsigned AbsoluteWaits{0U};
        std::uint64_t ExpectedDeadline{0U};
        std::uint64_t ExpectedRetired{0U};
        std::uint64_t ExpectedDurableAtFallback{0U};
        L::DedicatedCoordinatorReport* Report{nullptr};
        ServicedSink* First{nullptr};
        ServicedSink* Second{nullptr};
    };

    const T::Detail::ThreadContextOperations ServiceOperations{
        [](const void* opaque) noexcept {
            return static_cast<const ServiceContext*>(opaque)->Stop;
        },
        [](void*) noexcept -> T::ThreadWaitResult { return T::ThreadWaitResult::ProviderFailure; },
        [](void* opaque, C::Duration duration) noexcept -> T::ThreadWaitResult {
            auto& state = *static_cast<ServiceContext*>(opaque);
            ++state.RelativeWaits;
            assert(duration.Nanoseconds() == 5000000);
            assert(state.Report->RecordsRetired.load() == state.ExpectedRetired);
            assert(state.Report->DurableSinkCompletions.load() == state.ExpectedDurableAtFallback);
            state.Stop = true;
            return T::ThreadWaitResult::TimedOut;
        },
        [](void* opaque, C::MonotonicTimestamp deadline) noexcept -> T::ThreadWaitResult {
            auto& state = *static_cast<ServiceContext*>(opaque);
            ++state.AbsoluteWaits;
            assert(deadline.Nanoseconds() == state.ExpectedDeadline);
            assert(state.Report->RecordsRetired.load() == state.ExpectedRetired);
            assert(state.Report->DurableSinkCompletions.load() == 0U);
            assert(Rec::Live.load() == 0); // RAM source lease already reclaimed.
            if (state.FailAbsoluteWait) {
                return T::ThreadWaitResult::ProviderFailure;
            }
            state.First->Due = true;
            if (state.Second != nullptr) {
                state.Second->Due = true;
            }
            return T::ThreadWaitResult::TimedOut;
        }};

    /// An idle input queue still services copied data at its anchored absolute deadline.
    void IdleDeadlineAndPerSinkCounts() {
        ServiceFixture fixture{};
        fixture.Produce(1);
        ServicedSink first{}, second{};
        first.Deadline = 400U;
        second.Deadline = 200U;
        L::DedicatedCoordinatorReport report{};
        L::DedicatedCoordinator<1U, 5000000, Queue, ServicedSink, ServicedSink> coordinator(
            fixture.Records,
            report,
            first,
            second
        );
        ServiceContext state{};
        state.ExpectedDeadline = 200U;
        state.ExpectedRetired = 1U;
        state.ExpectedDurableAtFallback = 2U; // two sink completions, one source record
        state.Report = &report;
        state.First = &first;
        state.Second = &second;
        T::ThreadContext context(
            &state,
            ServiceOperations
        );
        coordinator(context);
        assert(first.LastValue == 1 && second.LastValue == 1);
        assert(first.Pending == 0U && second.Pending == 0U);
        assert(state.AbsoluteWaits == 1U && state.RelativeWaits == 1U);
        assert(report.RecordsRetired.load() == 1U && report.AllSinksAccepted.load() == 1U);
        assert(report.DurableSinkCompletions.load() == 2U);
        assert(report.SinkServiceFailures.load() == 0U);
        assert(report.Exit.load() == L::DedicatedCoordinatorExit::CooperativeStop);
    }

    /// A consumed quantum cannot starve optional service under continuous input.
    void ServiceAfterConsumedBudget() {
        ServiceFixture fixture{};
        fixture.Produce(1);
        fixture.Produce(2);
        ServicedSink sink{};
        sink.Deadline = 500U;
        sink.CompleteAfter = 2U;
        L::DedicatedCoordinatorReport report{};
        L::DedicatedCoordinator<1U, 5000000, Queue, ServicedSink> coordinator(
            L::CoordinatorQueueLockWait<5000000>{},
            fixture.Records,
            report,
            sink
        );
        ServiceContext state{};
        state.ExpectedRetired = 2U;
        state.ExpectedDurableAtFallback = 2U;
        state.Report = &report;
        state.First = &sink;
        T::ThreadContext context(
            &state,
            ServiceOperations
        );
        coordinator(context);
        assert(sink.Polls >= 3U);
        assert(state.AbsoluteWaits == 0U && state.RelativeWaits == 1U);
        assert(report.DurableSinkCompletions.load() == 2U && sink.Pending == 0U);
    }

    /// Sticky service uncertainty stops the activation without a wait or blind retry.
    void ServiceFailureStops() {
        ServiceFixture fixture{};
        fixture.Produce(1);
        ServicedSink sink{};
        sink.Fatal = true;
        L::DedicatedCoordinatorReport report{};
        L::DedicatedCoordinator<1U, 5000000, Queue, ServicedSink> coordinator(
            fixture.Records,
            report,
            sink
        );
        ServiceContext state{};
        state.Report = &report;
        state.First = &sink;
        T::ThreadContext context(
            &state,
            ServiceOperations
        );
        coordinator(context);
        assert(sink.Polls == 1U && sink.Pending == 1U);
        assert(report.DurableSinkCompletions.load() == 0U);
        assert(report.SinkServiceFailures.load() == 1U);
        assert(report.Exit.load() == L::DedicatedCoordinatorExit::SinkServiceRecoveryRequired);
        assert(state.AbsoluteWaits == 0U && state.RelativeWaits == 0U);
    }

    /// A failed existing WaitUntil is reported, rather than spinning or pretending shutdown.
    void AbsoluteWaitFailureStops() {
        ServiceFixture fixture{};
        fixture.Produce(1);
        ServicedSink sink{};
        sink.Deadline = 200U;
        L::DedicatedCoordinatorReport report{};
        L::DedicatedCoordinator<1U, 5000000, Queue, ServicedSink> coordinator(
            fixture.Records,
            report,
            sink
        );
        ServiceContext state{};
        state.FailAbsoluteWait = true;
        state.ExpectedDeadline = 200U;
        state.ExpectedRetired = 1U;
        state.Report = &report;
        state.First = &sink;
        T::ThreadContext context(
            &state,
            ServiceOperations
        );
        coordinator(context);
        assert(state.AbsoluteWaits == 1U && sink.Pending == 1U);
        assert(report.Exit.load() == L::DedicatedCoordinatorExit::WakeProviderFailure);
        assert(report.DurableSinkCompletions.load() == 0U);
    }

    /// Finite queue contention leaves its source untouched and still services ordinary sinks.
    void BoundedQueueContentionRetainsSource() {
        ServiceFixture fixture{};
        fixture.Produce(71);
        ServicedSink sink{};
        sink.CompleteAfter = 1U;
        L::DedicatedCoordinatorReport report{};
        L::DedicatedCoordinator<1U, 5000000, Queue, ServicedSink> coordinator(
            L::CoordinatorQueueLockWait<1000000>{},
            fixture.Records,
            report,
            sink
        );
        std::atomic<bool> locked{false};
        std::atomic<bool> unlock{false};
        std::thread holder([&]() {
            assert(fixture.QueueMutex.Acquire(
                       ESPressio::Platform::Synchronization::WaitTimeout::NoWait()) ==
                   T::OrdinaryMutexAcquireResult::Acquired);
            locked.store(true);
            while (!unlock.load()) {
                std::this_thread::yield();
            }
            assert(fixture.QueueMutex.Release() == T::OrdinaryMutexReleaseResult::Released);
        });
        while (!locked.load()) {
            std::this_thread::yield();
        }
        ServiceContext state{};
        state.Report = &report;
        state.First = &sink;
        T::ThreadContext context(
            &state,
            ServiceOperations
        );
        coordinator(context);
        assert(report.QueueContentions.load() == 1U);
        assert(report.RecordsRetired.load() == 0U && sink.Accepted == 0U);
        assert(sink.Polls == 1U && state.RelativeWaits == 1U);
        assert(report.Exit.load() == L::DedicatedCoordinatorExit::CooperativeStop);
        unlock.store(true);
        holder.join();
        Queue::ConsumerClaim claim{};
        const auto drained = L::DrainOneRecord(
            fixture.Records,
            claim,
            sink
        );
        assert(drained.Status == L::RecordDrainStatus::AcceptedAndReclaimed);
        assert(sink.Accepted == 1U && sink.LastValue == 71);
        assert(sink.ServiceCoordinator().DurableSinkCompletions == 1U);
    }

    /// Forces bounded retirement contention after the source has reached its sink.
    void ReportsBoundedRetirementContention() {
        ServiceFixture fixture{};
        fixture.Produce(71);
        std::atomic<bool> acquire{false};
        std::atomic<bool> locked{false};
        std::atomic<bool> release{false};
        std::thread holder([&]() {
            while (!acquire.load()) {
                std::this_thread::yield();
            }
            assert(fixture.QueueMutex.Acquire(
                       ESPressio::Platform::Synchronization::WaitTimeout::NoWait()
                   ) == T::OrdinaryMutexAcquireResult::Acquired);
            locked.store(true);
            while (!release.load()) {
                std::this_thread::yield();
            }
            assert(fixture.QueueMutex.Release() == T::OrdinaryMutexReleaseResult::Released);
        });
        unsigned submissions = 0U;
        auto sink = [&](const Rec&) noexcept {
            ++submissions;
            acquire.store(true);
            while (!locked.load()) {
                std::this_thread::yield();
            }
            return L::RecordSinkSubmission::Accepted;
        };
        L::DedicatedCoordinatorReport report{};
        L::DedicatedCoordinator<1U, 5000000, Queue, decltype(sink)> coordinator(
            L::CoordinatorQueueLockWait<1000000>{},
            fixture.Records,
            report,
            sink
        );
        ServiceContext state{};
        T::ThreadContext context(
            &state,
            ServiceOperations
        );
        coordinator(context);
        release.store(true);
        holder.join();
        assert(submissions == 1U);
        assert(report.Exit.load() == L::DedicatedCoordinatorExit::ReclaimUnconfirmed);
        assert(report.ReclaimFailure.load() == L::RecordQueueResult::DeadlineElapsed ||
               report.ReclaimFailure.load() == L::RecordQueueResult::Busy);
        assert(report.RecordsRetired.load() == 0U);
    }

    /// Confirms external claim lifetime across a moved binding and explicit retirement retry.
    /// @tparam TBounded Selects finite queue waits instead of immediate transitions.
    template <bool TBounded>
    void BootstrapClaimRecovery() {
        ServiceFixture fixture{};
        fixture.Produce(81);
        Queue::ConsumerClaim claim{};
        std::atomic<bool> acquire{false};
        std::atomic<bool> locked{false};
        std::atomic<bool> release{false};
        std::thread holder([&]() {
            while (!acquire.load()) {
                std::this_thread::yield();
            }
            assert(fixture.QueueMutex.Acquire(
                ESPressio::Platform::Synchronization::WaitTimeout::NoWait()
            ) == T::OrdinaryMutexAcquireResult::Acquired);
            locked.store(true);
            while (!release.load()) {
                std::this_thread::yield();
            }
            assert(fixture.QueueMutex.Release() == T::OrdinaryMutexReleaseResult::Released);
        });
        unsigned submissions = 0U;
        auto sink = [&](const Rec&) noexcept {
            ++submissions;
            if (submissions == 1U) {
                acquire.store(true);
                while (!locked.load()) {
                    std::this_thread::yield();
                }
            }
            return L::RecordSinkSubmission::Accepted;
        };
        L::DedicatedCoordinatorReport report{};
        using Callable = L::DedicatedCoordinator<1U, 5000000, Queue, decltype(sink)>;
        ServiceContext state{};
        state.Report = &report;
        T::ThreadContext context(&state, ServiceOperations);
        {
            auto original = [&]() {
                if constexpr (TBounded) {
                    return Callable(
                        L::CoordinatorQueueLockWait<1000000>{},
                        fixture.Records,
                        report,
                        claim,
                        sink
                    );
                } else {
                    return Callable(fixture.Records, report, claim, sink);
                }
            }();
            Callable coordinator(std::move(original));
            coordinator(context);
            assert(report.Exit.load() == L::DedicatedCoordinatorExit::ReclaimUnconfirmed);
            assert(report.RecordsRetired.load() == 0U);
            assert(claim.IsActive() && Rec::Live.load() == 1);
            const auto blocked = claim.CompleteUntil(
                DeadlineAfter(std::chrono::milliseconds(1))
            );
            assert(blocked == L::RecordQueueResult::Busy ||
                   blocked == L::RecordQueueResult::DeadlineElapsed);
            assert(claim.IsActive() && submissions == 1U);
            release.store(true);
            holder.join();
            coordinator(context);
            assert(report.Exit.load() == L::DedicatedCoordinatorExit::PriorClaimUnresolved);
            assert(submissions == 1U);
        }
        // The binding's destruction leaves the borrowed claim in Bootstrap.
        assert(claim.IsActive() && Rec::Live.load() == 1);
        assert(claim.CompleteUntil(
            DeadlineAfter(std::chrono::milliseconds(100))
        ) == L::RecordQueueResult::Succeeded);
        assert(!claim.IsActive() && Rec::Live.load() == 0);
        // Reuse the queue only after confirmed retirement, without replaying the first sink.
        fixture.Produce(82);
        Callable resumed(fixture.Records, report, claim, sink);
        state.ExpectedRetired = 1U;
        resumed(context);
        assert(report.Exit.load() == L::DedicatedCoordinatorExit::CooperativeStop);
        assert(report.RecordsRetired.load() == 1U);
        assert(submissions == 2U && !claim.IsActive());
    }

} // namespace

int main() {
    static CanonicalClock clock;
    assert(C::BindMonotonicClock(clock));
    IdleDeadlineAndPerSinkCounts();
    ServiceAfterConsumedBudget();
    ServiceFailureStops();
    AbsoluteWaitFailureStops();
    BoundedQueueContentionRetainsSource();
    ReportsBoundedRetirementContention();
    BootstrapClaimRecovery<false>();
    BootstrapClaimRecovery<true>();
    std::puts("EXISTING_COORDINATOR_ABSOLUTE_SERVICE_AND_COMPLETION_NEGATIVES=PASS");
}
