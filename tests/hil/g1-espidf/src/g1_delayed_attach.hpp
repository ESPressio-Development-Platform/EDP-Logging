#pragma once

// HIL-only mutex shim: actual native EDP-Threading mutex ownership,
// with an intentional post-acquire delay on the second timed acquire
// (first = queue reservation, second = Memory lease attachment).
struct G1DelayedAttachMutex final {
    QueueMutex Native;
    std::uint32_t TimedAcquisitions{0U};
    bool* CancelOnSecondAcquire{nullptr};

    [[nodiscard]] auto TryAcquire() noexcept {
        return Native.TryAcquire();
    }
    [[nodiscard]] auto Acquire(
        Platform::Synchronization::WaitTimeout remaining
    ) noexcept {
        const auto result=Native.Acquire(remaining);
        if(result==Threading::OrdinaryMutexAcquireResult::Acquired &&
           ++TimedAcquisitions==2U) {
            if(CancelOnSecondAcquire != nullptr) *CancelOnSecondAcquire=true;
            vTaskDelay(pdMS_TO_TICKS(80U)+1U);
        }
        return result;
    }
    [[nodiscard]] auto Release() noexcept {
        return Native.Release();
    }
};

bool VerifyInjectedLateQueueAttachmentRollback(Pool& pool) noexcept {
    Wakes wakes;
    Lifecycle lifecycle(wakes);
    G1DelayedAttachMutex mutex;
    using LateQueue=Logging::TransactionalRecordQueue<
        2U,Lease,G1DelayedAttachMutex,Lifecycle
    >;
    LateQueue queue(mutex,lifecycle);
    Lease scratch;
    LateQueue::Reservation output;
    const auto neverStop=[]() noexcept -> bool {return false;};
    const auto began=esp_timer_get_time();
    const auto result=Logging::AcquireRecordUntil<8U,16U>(
        queue,lifecycle,wakes,0U,pool,scratch,output,
        Deadline(40U),neverStop,1002
    );
    const auto elapsed=esp_timer_get_time()-began;
    const bool correct =
        mutex.TimedAcquisitions==2U &&
        result.Status==Logging::RecordAdmissionStatus::DeadlineElapsed &&
        result.Memory==Logging::AdmissionWaitResult::Admitted &&
        result.MemoryAttempted &&
        result.WakePublication==Logging::AdmissionWakePublicationResult::Published &&
        scratch.IsEmpty() && !output.IsActive() &&
        !queue.IsIntegrityFailed() &&
        Record::Live.load()==0 && lifecycle.ActiveAdmissions()==0U &&
        elapsed>=40000LL && elapsed<300000LL;
    std::printf(
        "EDP_LOGGING_HIL|G1_DELAYED_ATTACH|locks=%u|status=%u|memory=%u|queue=%u|live=%d|elapsed_us=%lld|valid=%u\n",
        mutex.TimedAcquisitions,
        static_cast<unsigned>(result.Status),
        static_cast<unsigned>(result.Memory),
        static_cast<unsigned>(result.Queue),
        Record::Live.load(),static_cast<long long>(elapsed),
        static_cast<unsigned>(correct)
    );
    return Check(correct,"g1-native-late-queue-attachment-rollback");
}

bool VerifyCancellationAtFinalQueueAttachment(Pool& pool) noexcept {
    Wakes wakes;
    Lifecycle lifecycle(wakes);
    bool cancelRequested=false;
    G1DelayedAttachMutex mutex;
    mutex.CancelOnSecondAcquire=&cancelRequested;
    using CancelQueue=Logging::TransactionalRecordQueue<
        2U,Lease,G1DelayedAttachMutex,Lifecycle
    >;
    CancelQueue queue(mutex,lifecycle);
    Lease scratch;
    CancelQueue::Reservation output;
    const auto isStopped=[&]() noexcept -> bool {return cancelRequested;};
    const auto began=esp_timer_get_time();
    const auto result=Logging::AcquireRecordUntil<8U,16U>(
        queue,lifecycle,wakes,0U,pool,scratch,output,
        Deadline(400U),isStopped,1003
    );
    const auto elapsed=esp_timer_get_time()-began;
    const bool correct=
        mutex.TimedAcquisitions==2U && cancelRequested &&
        result.Status==Logging::RecordAdmissionStatus::ShutdownRequested &&
        result.Queue==Logging::RecordQueueResult::Succeeded &&
        result.Memory==Logging::AdmissionWaitResult::Admitted &&
        result.MemoryAttempted && scratch.IsEmpty() &&
        !output.IsActive() && !queue.IsIntegrityFailed() &&
        Record::Live.load()==0 && lifecycle.ActiveAdmissions()==0U &&
        elapsed>=80000LL && elapsed<300000LL;
    std::printf(
        "EDP_LOGGING_HIL|G1_CANCEL_DURING_ATTACH|locks=%u|status=%u|memory=%u|"
        "queue=%u|live=%d|elapsed_us=%lld|valid=%u\n",
        mutex.TimedAcquisitions,
        static_cast<unsigned>(result.Status),
        static_cast<unsigned>(result.Memory),
        static_cast<unsigned>(result.Queue),
        Record::Live.load(),static_cast<long long>(elapsed),
        static_cast<unsigned>(correct)
    );
    return Check(correct,"g1-native-cancel-during-queue-attachment");
}
