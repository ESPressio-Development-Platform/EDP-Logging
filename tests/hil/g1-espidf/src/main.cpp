#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <array>
#include <span>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <ESPressio_Clock.hpp>
#include <ESPressio_Memory.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>
#include <ESPressio_Threading.hpp>
#include <memory/MemoryResourceProvider.hpp>
#include <logging/BoundedRecordAdmission.hpp>
#include <logging/RecordProduction.hpp>
#include <logging/EdlgFrame.hpp>
#include <logging/EdlgFrameReader.hpp>
#include <serialisation/PositionalValueEncoding.hpp>

namespace Test {
namespace Clock = ESPressio::Clock;
namespace Memory = ESPressio::Memory;
namespace Platform = ESPressio::Platform;
namespace Threading = ESPressio::Threading;
namespace Logging = ESPressio::Logging;
namespace Framework = ESPressio::System::CompositionFramework;

// Concrete 1 MHz hardware timebase is selected ONLY at application Bootstrap.
class Timebase final : public Clock::MonotonicTimebaseProvider<1000000U> {
public:
    std::uint64_t CurrentCount() const noexcept {
        return static_cast<std::uint64_t>(esp_timer_get_time());
    }
};
class Record final {
    int _value;
    std::uint32_t _attempt{0U};
    std::uint64_t _timestamp{0U};
public:
    static std::atomic<int> Live;
    explicit Record(int v) noexcept : _value(v) { ++Live; }
    Record(Logging::AttemptSequence<> attempt, Logging::QualifiedLogTime stamp, int v) noexcept :
        _value(v), _attempt(attempt.Value()), _timestamp(stamp.Monotonic.Nanoseconds()) {
        ++Live;
    }
    ~Record() noexcept { --Live; }
    int Value() const noexcept { return _value; }
    std::uint32_t Attempt() const noexcept { return _attempt; }
    std::uint64_t Timestamp() const noexcept { return _timestamp; }
};
std::atomic<int> Record::Live{0};

using Resource = Platform::Portable::Memory::MemoryResourceProvider;
using Mutex = Platform::FreeRTOS::Synchronization::MutexProvider;
using Signal = Platform::FreeRTOS::Synchronization::SignalProvider;
using Topology = Memory::MemoryTopology<
    Resource, Memory::SharedReserve<0U,Resource>,
    Memory::ObjectPoolSpec<Record,Memory::DedicatedInstances<2U>>
>;
using Composition = Memory::MemoryComposition<Resource>;
using Allocator = typename Composition::template Select<
    Memory::SharedReserveAllocationRequirement,Framework::SelectUnique
>;
using Runtime = Memory::MemoryRuntime<Topology,Composition,Mutex,Signal>;
using Pool = Runtime::ObjectPoolType<Record>;
using Lease = Pool::LeaseType;
using Wakes = Threading::BoundedWaitWakeProvider<Signal,3U>;
using Lifecycle = Logging::AdmissionLifecycleCoordinator<2U,Wakes>;
struct MutexIdentity final {};
struct SequencerIdentity final {};
using SequencerMutex = Threading::OrdinaryMutexProvider<SequencerIdentity,Mutex>;
using Sequence = Logging::AttemptSequenceCounter<>;
struct LocalClockCapture final {
    Logging::CoherentLogClockSample CaptureCoherent() const noexcept {
        return { Clock::MonotonicNow(), {
            Clock::SynchronizedTimestamp::FromNanoseconds(0U),
            Clock::SynchronizationUncertainty::Maximum(),
            Clock::SynchronizationState::NeverSynchronized
        }, Clock::ClockEra(), false };
    }
};
using QueueMutex = Threading::OrdinaryMutexProvider<MutexIdentity,Mutex>;
using Queue = Logging::TransactionalRecordQueue<2U,Lease,QueueMutex,Lifecycle>;

Clock::MonotonicTimestamp Deadline(std::uint64_t ms) noexcept {
    return Clock::MonotonicTimestamp::FromNanoseconds(
        Clock::MonotonicNow().Nanoseconds()+ms*1000000ULL
    );
}
bool Check(bool value,const char* label) noexcept {
    if(!value) std::printf("EDP_LOGGING_HIL|FAIL|%s\n",label);
    return value;
}
struct Work final {
    Pool* PoolPtr;
    Queue* QueuePtr;
    Lifecycle* LifecyclePtr;
    Wakes* WakesPtr;
    TaskHandle_t Parent{};
    std::atomic<bool> Entered{false};
    std::atomic<bool> Completed{false};
    std::atomic<bool> Success{false};
    bool ExpectShutdown{false};
};
void Worker(void* arg) {
    auto& work=*static_cast<Work*>(arg);
    Lease scratch;
    Queue::Reservation owner;
    const auto neverStop=[]() noexcept -> bool {return false;};
    work.Entered.store(true);
    auto result=Logging::AcquireRecordUntil<16U,16U>(
        *work.QueuePtr,*work.LifecyclePtr,*work.WakesPtr,0U,
        *work.PoolPtr,scratch,owner,Deadline(3500U),neverStop,202
    );
    bool good=work.ExpectShutdown
        ? (result.Status==Logging::RecordAdmissionStatus::ShutdownRequested &&
           !owner.IsActive() && scratch.IsEmpty())
        : (result.Status==Logging::RecordAdmissionStatus::Admitted &&
           result.WakePublication==Logging::AdmissionWakePublicationResult::Published &&
           owner.IsActive() && scratch.IsEmpty() &&
           owner.Borrow()->Get()->Value()==202);
    if(owner.IsActive()) {
        auto status=owner.Abort();
        good=good && status==Logging::RecordQueueResult::Succeeded;
    }
    work.Success.store(good);
    work.Completed.store(true);
    xTaskNotifyGive(work.Parent);
    vTaskDelete(nullptr);
}
bool Run() {
    static Timebase timebase;
    static Clock::MonotonicClockProvider<Timebase> clock(timebase);
    if(!Check(Clock::BindMonotonicClock(clock),"clock-bootstrap")) return false;
    std::printf("EDP_LOGGING_HIL|CLOCK|CANONICAL_1MHZ\n");
    static Resource resource;
    static Mutex nativeMutex;
    static Allocator allocator;
    static Runtime runtime(nativeMutex,allocator,resource);
    Memory::MemoryTopologyInitializationFailure failure{};
    if(!Check(runtime.Initialize(failure)==Memory::MemoryTopologyInitializationResult::Succeeded,
              "memory-initialize")) return false;
    static Wakes wakes;
    static Lifecycle lifecycle(wakes);
    static QueueMutex mutex;
    static Queue queue(mutex,lifecycle);
    auto& pool=runtime.ObjectPoolFor<Record>();
    const auto neverStop=[]() noexcept -> bool {return false;};
    {
        Lease scratch;
        Queue::Reservation owner;
        auto outcome=Logging::AcquireRecordUntil<8U,8U>(
            queue,lifecycle,wakes,0U,pool,scratch,owner,Deadline(2000U),neverStop,101);
        if(!Check(outcome.Status==Logging::RecordAdmissionStatus::Admitted,
                  "acquire-immediate")) return false;
        if(!Check(outcome.WakePublication==Logging::AdmissionWakePublicationResult::Published,
                  "memory-grant-wake")) return false;
        if(!Check(owner.IsActive()&&scratch.IsEmpty()&&owner.Borrow()->Get()->Value()==101,
                  "native-lease-ownership")) return false;
        if(!Check(owner.Commit()==Logging::RecordQueueResult::Succeeded,
                  "fifo-commit")) return false;
        Queue::ConsumerClaim consumer;
        if(!Check(queue.Pop(consumer)==Logging::RecordQueueResult::Succeeded,
                  "fifo-pop")) return false;
        if(!Check(consumer.Borrow()->Get()->Value()==101,
                  "borrow-value")) return false;
        if(!Check(consumer.Complete()==Logging::RecordQueueResult::Succeeded,
                  "complete-release")) return false;
        if(!Check(Record::Live.load()==0,"lease-not-leaked")) return false;
        std::printf("EDP_LOGGING_HIL|PASS|FIFO_MEMORY_OWNERSHIP\n");
    }
    using Timeout=Platform::Synchronization::WaitTimeout;
    {
        Queue::Reservation holdA,holdB;
        if(!Check(queue.ReserveSlot(holdA,Timeout::NoWait())==
                  Logging::RecordQueueResult::Succeeded,"reserve-a")) return false;
        if(!Check(queue.ReserveSlot(holdB,Timeout::NoWait())==
                  Logging::RecordQueueResult::Succeeded,"reserve-b")) return false;
        static Work work{&pool,&queue,&lifecycle,&wakes};
        work.Parent=xTaskGetCurrentTaskHandle();
        work.Entered.store(false); work.Completed.store(false); work.Success.store(false);
        const auto started=xTaskCreatePinnedToCore(
            &Worker,"logging_hil_producer",6144U,&work,3U,nullptr,1
        );
        if(!Check(started==pdPASS,"producer-task-start")) return false;
        for(unsigned i=0U;i<100U&&!work.Entered.load();++i) {
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
        vTaskDelay(pdMS_TO_TICKS(100U));
        if(!Check(Record::Live.load()==0,"queue-full-no-memory-borrow")) return false;
        if(!Check(holdA.Abort()==Logging::RecordQueueResult::Succeeded,
                  "slot-abort-publishes-wake")) return false;
        const auto completed=ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(4000U));
        if(!Check(completed>0U&&work.Completed.load(),"producer-completion")) return false;
        if(!Check(work.Success.load(),"real-producer-wake")) return false;
        if(!Check(holdB.Abort()==Logging::RecordQueueResult::Succeeded,
                  "reclaim-other-slot")) return false;
        if(!Check(Record::Live.load()==0,"reclaimed-after-task")) return false;
        std::printf("EDP_LOGGING_HIL|PASS|DUAL_CORE_PRODUCER_WAKE\n");
    }
    // Standalone Clock producer, genuine EDP-Serialisation CBOR, EDLG framing,
    // UART evidence and Memory lease reclamation; NO Mesh is included.
    {
        static SequencerMutex sequencer;
        static Sequence sequence;
        static LocalClockCapture standaloneClock;
        Lease scratch;
        Queue::Reservation owner;
        const auto production=Logging::ProduceRecordUntil<
            Logging::AttemptSequenceWidth::Bits32,8U,8U
        >(sequence,sequencer,standaloneClock,queue,lifecycle,wakes,0U,
          pool,scratch,owner,Deadline(2000U),neverStop,515);
        if(!Check(production.Status==Logging::RecordProductionStatus::Published &&
                  production.Attempt.Value()==1U && production.TimeCaptured &&
                  !owner.IsActive() && scratch.IsEmpty(),
                  "standalone-production-committed")) return false;
        bool verified=false;
        auto uartSink=[&verified](const Record& record) noexcept ->
            Logging::RecordSinkSubmission {
            if(record.Attempt()!=1U || record.Timestamp()==0U ||
               record.Value()!=515) return Logging::RecordSinkSubmission::Rejected;
            std::array<std::byte,96U> payload{};
            const auto encoded=ESPressio::Serialisation::SerialisePositionalCbor<3U>(
                payload,record.Attempt(),record.Timestamp(),
                static_cast<std::int32_t>(record.Value()));
            if(!encoded.IsSuccessful()) return Logging::RecordSinkSubmission::Rejected;
            std::array<std::byte,160U> frame{};
            std::size_t written=0U;
            if(Logging::WriteEdlgFrame(Logging::EdlgCodec::Cbor,
                std::span<const std::byte>(payload.data(),encoded.BytesWritten),
                frame,written,payload.size())!=Logging::EdlgFrameWriteResult::Succeeded)
                return Logging::RecordSinkSubmission::Rejected;
            Logging::EdlgFrameView view{};
            if(Logging::ReadEdlgFrame(
                std::span<const std::byte>(frame.data(),written),view,payload.size()
            )!=Logging::EdlgFrameReadResult::Succeeded)
                return Logging::RecordSinkSubmission::Rejected;
            verified=view.Codec==Logging::EdlgCodec::Cbor &&
                     view.Payload.size()==encoded.BytesWritten &&
                     view.Consumed==written;
            if(!verified) return Logging::RecordSinkSubmission::Rejected;
            std::printf("EDP_LOGGING_HIL|CBOR_EDLG_HEX|");
            for(std::size_t i=0U;i<written;++i)
                std::printf("%02X",std::to_integer<unsigned>(frame[i]));
            std::printf("\n");
            return Logging::RecordSinkSubmission::Accepted;
        };
        Queue::ConsumerClaim claim;
        const auto drained=Logging::DrainOneRecord(queue,claim,uartSink);
        if(!Check(verified &&
                  drained.Status==Logging::RecordDrainStatus::AcceptedAndReclaimed &&
                  drained.SubmissionAttempted && !claim.IsActive() &&
                  Record::Live.load()==0,
                  "real-cbor-edlg-source-drained")) return false;
        std::printf("EDP_LOGGING_HIL|PASS|STANDALONE_CLOCK_PRODUCER_CBOR_EDLG\n");
        Queue::Reservation failedOwner;
        Lease failedScratch;
        const auto second=Logging::ProduceRecordUntil<
            Logging::AttemptSequenceWidth::Bits32,8U,8U
        >(sequence,sequencer,standaloneClock,queue,lifecycle,wakes,0U,
          pool,failedScratch,failedOwner,Deadline(2000U),neverStop,516);
        if(!Check(second.Status==Logging::RecordProductionStatus::Published &&
                  second.Attempt.Value()==2U,"sequence-second-attempt")) return false;
        Queue::ConsumerClaim rejectedClaim;
        const auto reject=[](const Record&) noexcept ->
            Logging::RecordSinkSubmission { return Logging::RecordSinkSubmission::Rejected; };
        const auto dropped=Logging::DrainOneRecord(queue,rejectedClaim,reject);
        if(!Check(dropped.Status==Logging::RecordDrainStatus::FailedAndReclaimed &&
                  dropped.Submission==Logging::RecordSinkSubmission::Rejected &&
                  Record::Live.load()==0,"sink-rejection-accounted")) return false;
        std::printf("EDP_LOGGING_HIL|PASS|SINK_REJECTION_ACCOUNTING\n");
    }
    {
        Lease scratch; Queue::Reservation owner;
        auto outcome=Logging::AcquireRecordUntil<4U,4U>(
            queue,lifecycle,wakes,2U,pool,scratch,owner,Deadline(1000U),neverStop,303);
        if(!Check(outcome.Status==Logging::RecordAdmissionStatus::InvalidProducerSlot &&
                  !outcome.MemoryAttempted && !owner.IsActive() && scratch.IsEmpty(),
                  "drain-slot-isolation")) return false;
        std::printf("EDP_LOGGING_HIL|PASS|DRAIN_SLOT_ISOLATION\n");
    }
    // 4. Real task is blocked for queue capacity when shutdown starts.
    // ShutdownUntil must wait for this producer to retire, not report false
    // quiescence while it still owns an EDP-Threading wake slot.
    {
        Queue::Reservation holdA,holdB;
        if(!Check(queue.ReserveSlot(holdA,Timeout::NoWait())==
                  Logging::RecordQueueResult::Succeeded,"shutdown-hold-a")) return false;
        if(!Check(queue.ReserveSlot(holdB,Timeout::NoWait())==
                  Logging::RecordQueueResult::Succeeded,"shutdown-hold-b")) return false;
        static Work shutdownWork{&pool,&queue,&lifecycle,&wakes};
        shutdownWork.ExpectShutdown=true;
        shutdownWork.Parent=xTaskGetCurrentTaskHandle();
        shutdownWork.Entered.store(false);
        shutdownWork.Completed.store(false);
        shutdownWork.Success.store(false);
        auto started=xTaskCreatePinnedToCore(
            &Worker,"logging_hil_shutdown",6144U,&shutdownWork,3U,nullptr,1
        );
        if(!Check(started==pdPASS,"shutdown-worker-start")) return false;
        for(unsigned i=0U;i<100U&&
            (!shutdownWork.Entered.load()||lifecycle.ActiveAdmissions()==0U);++i) {
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
        if(!Check(lifecycle.ActiveAdmissions()>0U,"shutdown-worker-registered"))
            return false;
        auto result=lifecycle.ShutdownUntil<16U>(Deadline(1200U));
        const auto completed=ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(2000U));
        if(!Check(result.Status==Logging::AdmissionShutdownStatus::Quiescent &&
                  result.ActiveAdmissions==0U,"bounded-shutdown")) return false;
        if(!Check(completed>0U&&shutdownWork.Completed.load()&&
                  shutdownWork.Success.load(),"shutdown-cancelled-producer")) return false;
        if(!Check(holdA.Abort()==Logging::RecordQueueResult::Succeeded &&
                  holdB.Abort()==Logging::RecordQueueResult::Succeeded,
                  "shutdown-held-slot-retirement")) return false;
        Lease scratch; Queue::Reservation owner;
        auto denied=Logging::AcquireRecordUntil<4U,4U>(
            queue,lifecycle,wakes,0U,pool,scratch,owner,Deadline(1000U),neverStop,404);
        if(!Check(denied.Status==Logging::RecordAdmissionStatus::ShutdownRequested,
                  "reject-after-shutdown")) return false;
        if(!Check(Record::Live.load()==0,"shutdown-no-outstanding-leases"))
            return false;
        if(!Check(runtime.TearDown()==Memory::MemoryTopologyTeardownResult::Succeeded,
                  "memory-teardown")) return false;
        std::printf("EDP_LOGGING_HIL|PASS|SHUTDOWN_AND_TEARDOWN\n");
    }
    return true;
}
}
extern "C" void app_main() {
    std::printf("EDP_LOGGING_HIL|START|G1_WROOM_REAL_EDP_PROVIDERS\n");
    const bool success=Test::Run();
    std::printf("EDP_LOGGING_HIL|RESULT|%s\n",success?"PASS":"FAIL");
    std::fflush(stdout);
    for(;;) vTaskDelay(pdMS_TO_TICKS(1000U));
}
