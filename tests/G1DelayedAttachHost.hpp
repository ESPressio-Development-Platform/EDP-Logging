// Included only by BoundedRecordAdmissionTests.cpp inside its test namespace.
// Wrapper holds the existing real EDP-Threading mutex while deliberately
// delaying after the second timed Acquire (queue AttachLease).
struct TestDelayedAttachMutex final {
    QueueMutex Native;
    unsigned TimedAcquires{0U};
    bool* CancelOnSecondAcquire{nullptr};
    [[nodiscard]] auto TryAcquire() noexcept {
        return Native.TryAcquire();
    }
    [[nodiscard]] auto Acquire(
        ESPressio::Platform::Synchronization::WaitTimeout deadline
    ) noexcept {
        const auto status=Native.Acquire(deadline);
        if(status==ESPressio::Threading::OrdinaryMutexAcquireResult::Acquired &&
           ++TimedAcquires==2U) {
            if(CancelOnSecondAcquire != nullptr) *CancelOnSecondAcquire=true;
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
        return status;
    }
    [[nodiscard]] auto Release() noexcept {
        return Native.Release();
    }
};

void TestDeadlineCannotBeOverrunByFinalQueueAttachment() {
    namespace L=ESPressio::Logging;
    Resource resource;
    TestSupport::TestMutexProvider nativeMutex;
    MemoryComposition::Select<
        Memory::SharedReserveAllocationRequirement, Framework::SelectUnique
    > allocator;
    Runtime runtime(nativeMutex,allocator,resource);
    Memory::MemoryTopologyInitializationFailure failure{};
    assert(runtime.Initialize(failure)==
        Memory::MemoryTopologyInitializationResult::Succeeded);
    auto& pool=runtime.ObjectPoolFor<TestSupport::PooledObject>();
    WakeProvider wakes;
    Lifecycle lifecycle(wakes);
    TestDelayedAttachMutex mutex;
    using DelayedQueue=L::TransactionalRecordQueue<
        2U,PooledLease,TestDelayedAttachMutex,Lifecycle
    >;
    DelayedQueue queue(mutex,lifecycle);
    PooledLease scratch;
    DelayedQueue::Reservation output;
    const auto neverStop=[]() noexcept -> bool {return false;};
    const auto result=L::AcquireRecordUntil<8U,16U>(
        queue,lifecycle,wakes,0U,pool,scratch,output,
        DeadlineAfter(std::chrono::milliseconds(40)),neverStop,701
    );
    assert(mutex.TimedAcquires==2U);
    assert(result.MemoryAttempted);
    assert(result.Memory==L::AdmissionWaitResult::Admitted);
    assert(result.Status==L::RecordAdmissionStatus::DeadlineElapsed);
    assert(result.Queue==L::RecordQueueResult::Succeeded);
    assert(result.WakePublication==
        L::AdmissionWakePublicationResult::Published);
    assert(scratch.IsEmpty() && !output.IsActive());
    assert(!queue.IsIntegrityFailed());
    assert(TestSupport::PooledObject::LiveCount.load()==0);
    assert(lifecycle.ActiveAdmissions()==0U);
    assert(runtime.TearDown()==
        Memory::MemoryTopologyTeardownResult::Succeeded);
}

void TestCancellationAtFinalQueueAttachment() {
    namespace L=ESPressio::Logging;
    Resource resource;
    TestSupport::TestMutexProvider nativeMutex;
    MemoryComposition::Select<
        Memory::SharedReserveAllocationRequirement, Framework::SelectUnique
    > allocator;
    Runtime runtime(nativeMutex,allocator,resource);
    Memory::MemoryTopologyInitializationFailure failure{};
    assert(runtime.Initialize(failure)==
        Memory::MemoryTopologyInitializationResult::Succeeded);
    auto& pool=runtime.ObjectPoolFor<TestSupport::PooledObject>();
    WakeProvider wakes;
    Lifecycle lifecycle(wakes);
    bool cancelRequested=false;
    TestDelayedAttachMutex mutex;
    mutex.CancelOnSecondAcquire=&cancelRequested;
    using Queue=L::TransactionalRecordQueue<
        2U,PooledLease,TestDelayedAttachMutex,Lifecycle
    >;
    Queue queue(mutex,lifecycle);
    PooledLease scratch;
    Queue::Reservation output;
    const auto isStopped=[&]() noexcept -> bool { return cancelRequested; };
    const auto result=L::AcquireRecordUntil<8U,16U>(
        queue,lifecycle,wakes,0U,pool,scratch,output,
        DeadlineAfter(std::chrono::milliseconds(400)),isStopped,702
    );
    assert(cancelRequested && mutex.TimedAcquires==2U);
    assert(result.Status==L::RecordAdmissionStatus::ShutdownRequested);
    assert(result.Queue==L::RecordQueueResult::Succeeded);
    assert(result.MemoryAttempted);
    assert(result.Memory==L::AdmissionWaitResult::Admitted);
    assert(scratch.IsEmpty() && !output.IsActive());
    assert(!queue.IsIntegrityFailed());
    assert(TestSupport::PooledObject::LiveCount.load()==0);
    assert(lifecycle.ActiveAdmissions()==0U);
    assert(runtime.TearDown()==Memory::MemoryTopologyTeardownResult::Succeeded);
}
