#pragma once
#include <utility>

// HIL-only: real EDP-Memory acquisition with a deliberate delay after
// ownership transfer. Exercises rare post-deadline return deterministically.
struct G1DelayedGrantPool final {
    Pool& Owner;
    std::uint32_t SuccessfulGrants{0U};
    template<class... TArgs>
    [[nodiscard]] Memory::ObjectPoolAcquisitionResult Acquire(
        Lease& lease, Platform::Synchronization::WaitTimeout timeout,
        TArgs&&... args
    ) noexcept {
        const auto result=Owner.Acquire(
            lease,timeout,std::forward<TArgs>(args)...
        );
        if(result==Memory::ObjectPoolAcquisitionResult::Succeeded) {
            ++SuccessfulGrants;
            vTaskDelay(pdMS_TO_TICKS(80U)+1U);
        }
        return result;
    }
};

bool VerifyInjectedLateRealMemoryGrantRollback(Pool& realPool) noexcept {
    Wakes wakes;
    Lifecycle lifecycle(wakes);
    QueueMutex mutex;
    Queue queue(mutex,lifecycle);
    G1DelayedGrantPool gated{realPool};
    Lease scratch;
    Queue::Reservation output;
    const auto neverStop=[]() noexcept -> bool {return false;};
    const auto beginning=esp_timer_get_time();
    const auto outcome=Logging::AcquireRecordUntil<8U,16U>(
        queue,lifecycle,wakes,0U,gated,scratch,output,
        Deadline(40U),neverStop,1001
    );
    const auto elapsed=esp_timer_get_time()-beginning;
    const bool correct =
        gated.SuccessfulGrants==1U &&
        outcome.Status==Logging::RecordAdmissionStatus::LateMemoryGrantReturned &&
        outcome.Memory==Logging::AdmissionWaitResult::GrantedAfterDeadline &&
        outcome.MemoryAttempted && outcome.ReleaseAttempted &&
        outcome.Release==Logging::CoordinatedReleaseResult::ReleasedAndNotified &&
        outcome.WakePublication==Logging::AdmissionWakePublicationResult::Published &&
        scratch.IsEmpty() && !output.IsActive() &&
        Record::Live.load()==0 && lifecycle.ActiveAdmissions()==0U &&
        elapsed>=40000LL && elapsed<300000LL;
    std::printf(
       "EDP_LOGGING_HIL|G1_INJECTED_LATE_GRANT|acquired=%u|status=%u|memory=%u|release=%u|wake=%u|live=%d|elapsed_us=%lld|valid=%u\n",
       gated.SuccessfulGrants,static_cast<unsigned>(outcome.Status),
       static_cast<unsigned>(outcome.Memory),
       static_cast<unsigned>(outcome.Release),
       static_cast<unsigned>(outcome.WakePublication),Record::Live.load(),
       static_cast<long long>(elapsed),static_cast<unsigned>(correct));
    return Check(correct,"g1-real-memory-granted-after-deadline-rollback");
}
