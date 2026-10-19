#pragma once
// Dedicated actual-FreeRTOS Core1 producer / Core0 Memory-capacity races.
// This header is included inside the HIL Test namespace after its owner types.
struct NativeGrantBoundaryWork final {
    Pool* PoolPtr{};
    Queue* QueuePtr{};
    Lifecycle* LifecyclePtr{};
    Wakes* WakesPtr{};
    TaskHandle_t Parent{};
    std::uint32_t DeadlineMs{0U};
    bool ExpectGrant{false};
    std::atomic<bool> Entered{false};
    std::atomic<bool> Completed{false};
    std::atomic<bool> Success{false};
    std::atomic<unsigned> Status{255U};
    std::atomic<bool> MemoryAttempted{false};
};

void NativeGrantBoundaryWorker(void* input) noexcept {
    auto& work=*static_cast<NativeGrantBoundaryWork*>(input);
    Lease scratch;
    Queue::Reservation reservation;
    const auto neverStop=[]() noexcept -> bool {return false;};
    work.Entered.store(true);
    const auto result=Logging::AcquireRecordUntil<8U,16U>(
        *work.QueuePtr,*work.LifecyclePtr,*work.WakesPtr,0U,
        *work.PoolPtr,scratch,reservation,Deadline(work.DeadlineMs),
        neverStop,919);
    work.Status.store(static_cast<unsigned>(result.Status));
    work.MemoryAttempted.store(result.MemoryAttempted);
    bool good=work.ExpectGrant
        ? result.Status==Logging::RecordAdmissionStatus::Admitted &&
          reservation.IsActive() && scratch.IsEmpty() &&
          reservation.Borrow()!=nullptr &&
          reservation.Borrow()->Get()->Value()==919
        : result.Status==Logging::RecordAdmissionStatus::DeadlineElapsed &&
          scratch.IsEmpty() && !reservation.IsActive();
    if(reservation.IsActive()) {
        const auto abort=reservation.Abort();
        good=good && abort==Logging::RecordQueueResult::Succeeded;
    }
    work.Success.store(good && scratch.IsEmpty() && !reservation.IsActive());
    work.Completed.store(true);
    xTaskNotifyGive(work.Parent);
    vTaskDelete(nullptr);
}

bool VerifyCrossCoreMemoryGrantBoundary(Pool& pool,bool releaseBeforeDeadline) noexcept {
    Wakes wakes;
    Lifecycle lifecycle(wakes);
    QueueMutex mutex;
    Queue queue(mutex,lifecycle);
    Lease heldA,heldB;
    const auto noWait=Platform::Synchronization::WaitTimeout::NoWait();
    const auto a=pool.Acquire(heldA,noWait,911);
    const auto b=pool.Acquire(heldB,noWait,912);
    if(!Check(a==Memory::ObjectPoolAcquisitionResult::Succeeded &&
              b==Memory::ObjectPoolAcquisitionResult::Succeeded &&
              Record::Live.load()==2,
              "g1-race-native-memory-two-held")) return false;

    NativeGrantBoundaryWork work{};
    work.PoolPtr=&pool;
    work.QueuePtr=&queue;
    work.LifecyclePtr=&lifecycle;
    work.WakesPtr=&wakes;
    work.Parent=xTaskGetCurrentTaskHandle();
    work.DeadlineMs=releaseBeforeDeadline ? 500U : 40U;
    work.ExpectGrant=releaseBeforeDeadline;
    if(xTaskCreatePinnedToCore(&NativeGrantBoundaryWorker,
        "logging_mem_boundary",6144U,&work,3U,nullptr,1)!=pdPASS)
        return Check(false,"g1-race-worker-create");

    for(unsigned n=0U;n<200U &&
        (!work.Entered.load() || lifecycle.ActiveAdmissions()==0U);++n)
        vTaskDelay(1U);
    const bool definitelyWaiting=work.Entered.load() &&
        lifecycle.ActiveAdmissions()>0U && Record::Live.load()==2;
    if(releaseBeforeDeadline) {
        // One actual core-to-core Memory grant well within the 500ms
        // deadline. Memory's own wait provider must wake the producer.
        vTaskDelay(2U);
    }
    const auto began=esp_timer_get_time();
    Logging::CoordinatedReleaseResult releasedA{};
    if(releaseBeforeDeadline) {
        releasedA=lifecycle.ReleaseAndNotify(heldA);
    }
    const auto completed=ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1200U));
    if(!releaseBeforeDeadline) {
        // Memory becomes available only after the actual 40ms timeout
        // and the producer has exited. No post-deadline owner may leak.
        releasedA=lifecycle.ReleaseAndNotify(heldA);
    }
    const auto releasedB=lifecycle.ReleaseAndNotify(heldB);
    const auto elapsed=esp_timer_get_time()-began;
    const bool good=definitelyWaiting && completed>0U &&
        work.Completed.load() && work.Success.load() &&
        work.MemoryAttempted.load() &&
        releasedA==Logging::CoordinatedReleaseResult::ReleasedAndNotified &&
        releasedB==Logging::CoordinatedReleaseResult::ReleasedAndNotified &&
        heldA.IsEmpty() && heldB.IsEmpty() &&
        Record::Live.load()==0 && lifecycle.ActiveAdmissions()==0U;
    std::printf("EDP_LOGGING_HIL|G1_NATIVE_GRANT_BOUNDARY|before=%u|waiting=%u|status=%u|done=%u|good=%u|memory=%u|live=%d|elapsed_us=%lld\n",
                static_cast<unsigned>(releaseBeforeDeadline),
                static_cast<unsigned>(definitelyWaiting),
                work.Status.load(),
                static_cast<unsigned>(completed>0U),
                static_cast<unsigned>(good),
                static_cast<unsigned>(work.MemoryAttempted.load()),
                static_cast<int>(Record::Live.load()),
                static_cast<long long>(elapsed));
    return Check(good,"g1-core1-memory-grant-before-after-deadline");
}
