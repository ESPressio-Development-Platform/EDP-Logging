#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <array>
#include <span>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <ESPressio_Clock.hpp>
#include <ESPressio_Memory.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>
#include <ESPressio_Threading.hpp>
#include <memory/MemoryResourceProvider.hpp>
#include <logging/BoundedRecordAdmission.hpp>
#include <logging/RecordProduction.hpp>
#include <logging/CoordinatorQuantum.hpp>
#include <logging/DedicatedCoordinator.hpp>
#include <logging/OwnedRepresentationFanout.hpp>
#include <logging/Instrumentation.hpp>
#include <execution/ExecutionContextProvider.hpp>
#include <synchronization/SpinLockProvider.hpp>
#include <logging/LogRecord.hpp>
#include <logging/EdlgFrame.hpp>
#include <logging/EdlgFrameReader.hpp>
#include <serialisation/PositionalValueEncoding.hpp>
#include <logging/EdlgCheckpoint.hpp>
#include <logging/EdlsArchiveSuccessor.hpp>
#include <VfsFileStorage.hpp>
#include <esp_spiffs.h>
#include <cstring>
#include <ESPressio_Persistence_ESP_IDF.hpp>
#include <memory/ByteOperationsProvider.hpp>
#include <nvs_flash.h>
#include <esp_system.h>
#include <driver/uart.h>
#include <serial/BoundedEdlgFrameQueue.hpp>
#include <serial_esp_idf/UartFifoByteStreamWriter.hpp>

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

// G11 test-only observation, written by the actual EDP-Threading managed
// worker at its final callable boundary, read by app_main after quiescence.
std::atomic<unsigned> DedicatedWorkerStackMarginBytes{0U};


// G11 application-level codegen and resource test: these counters are
// test instrumentation only and exist in both full-firmware variants.
#ifndef EDP_LOGGING_G11_APP_SITE_ENABLED
#define EDP_LOGGING_G11_APP_SITE_ENABLED 0
#endif
#if EDP_LOGGING_G11_APP_SITE_ENABLED != 0 && EDP_LOGGING_G11_APP_SITE_ENABLED != 1
#error "G11 whole-app instrumentation selection must be 0 or 1"
#endif
#if EDP_LOGGING_G11_APP_SITE_ENABLED == 1
inline constexpr Logging::SeverityThreshold G11SiteThreshold =
    Logging::SeverityThreshold::Info;
#else
inline constexpr Logging::SeverityThreshold G11SiteThreshold =
    Logging::SeverityThreshold::Off;
#endif
std::atomic<std::uint32_t> G11ClockReads{0U};
std::atomic<std::uint32_t> G11IdentityReads{0U};
std::atomic<std::uint32_t> G11ArgumentReads{0U};
[[gnu::noinline]] void G11ClockRead() noexcept {
    G11ClockReads.fetch_add(1U, std::memory_order_relaxed);
}
[[gnu::noinline]] void G11IdentityRead() noexcept {
    G11IdentityReads.fetch_add(1U, std::memory_order_relaxed);
}
[[gnu::noinline]] void G11ArgumentRead() noexcept {
    G11ArgumentReads.fetch_add(1U, std::memory_order_relaxed);
}

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

// Only Bootstrap selects actual native Persistence and Serial providers.
struct NvsTestIdentity final {};
using ByteOps = Platform::Portable::Memory::ByteOperationsProvider;
using Nvs = ESPressio::Persistence::EspIdf::NvsKeyValueStorage<NvsTestIdentity,ByteOps>;
struct SerialMutexIdentity final {};
using SerialMutex = Threading::OrdinaryMutexProvider<SerialMutexIdentity,Mutex>;
using SerialQueue = ESPressio::Serial::BoundedEdlgFrameQueue<2U,32U,SerialMutex>;
bool Check(bool value,const char* label) noexcept;

// Exercises the real governed in-memory validation on the WROOM MCU.
 // This does not claim owner-typed CBOR/JSON LogRecord serialisation.
bool TestGovernedLogRecordValidation() noexcept {
    Logging::LogRecord record{};
    auto bytes=ESPressio::System::Identity::DeviceIdentifier::Storage{};
    bytes[0U]=0xABU;
    record.Device=ESPressio::System::Identity::DeviceIdentifier{bytes};
    record.Incarnation=ESPressio::System::Identity::RuntimeIncarnationId{1U};
    record.Attempt=Logging::AttemptSequence<Logging::AttemptSequenceWidth::Bits32>::
        FromValidatedValue(1U);
    record.Message.String=3U;
    if (!Check(Logging::ValidateLogRecord(record)==
        Logging::LogRecordValidation::Valid,"record-monotonic-only-valid")) return false;
    record.Provenance.OpaqueSourceFacts=1U;
    if (!Check(Logging::ValidateLogRecord(record)==
        Logging::LogRecordValidation::UnapprovedTemporalProvenance,
        "record-unissued-provenance-rejected")) return false;
    record.Provenance.OpaqueSourceFacts=0U;
    record.Facts.OpaqueSourceFacts=1U;
    if (!Check(Logging::ValidateLogRecord(record)==
        Logging::LogRecordValidation::UnapprovedRecordFacts,
        "record-unissued-facts-rejected")) return false;
    record.Facts.OpaqueSourceFacts=0U;
    record.Uncertainty=Clock::SynchronizationUncertainty::Zero();
    if (!Check(Logging::ValidateLogRecord(record)==
        Logging::LogRecordValidation::InvalidSyncEvidence,
        "record-missing-sync-no-false-precision")) return false;
    record.Uncertainty=Clock::SynchronizationUncertainty::Maximum();
    if (!Check(Logging::ValidateLogRecord(record)==
        Logging::LogRecordValidation::Valid,"record-unknown-uncertainty-accepted")) return false;
    std::printf("EDP_LOGGING_HIL|PASS|GOVERNED_LOG_RECORD_VALIDATION\n");
    return true;
}

bool TestNvsCheckpointAcrossReboot() noexcept {
    namespace Persistence=ESPressio::Persistence;
    if(!Check(nvs_flash_init()==ESP_OK,"nvs-initialize-no-erase")) return false;
    static ByteOps byteOps;
    Nvs native(byteOps);
    if(!Check(native.Open("edp_log_hil")==
        Persistence::EspIdf::NvsOpenStatus::Succeeded,"nvs-provider-open"))
        return false;
    constexpr auto key=Persistence::KeyView::Validate("frame");
    static_assert(key.IsValuePresent);
    std::array<std::byte,64U> loaded{};
    Logging::EdlgFrameView view{};
    const auto read=Logging::ReadEdlgCheckpoint(native,key.Value,loaded,32U,view);
    if(read.Status==Logging::EdlgCheckpointReadStatus::NotFound) {
        const std::array<std::byte,4U> cbor{
            std::byte{0x83U},std::byte{0x01U},std::byte{0x02U},std::byte{0x03U}
        };
        std::array<std::byte,64U> frame{};
        std::size_t written{0U};
        if(!Check(Logging::WriteEdlgFrame(
            Logging::EdlgCodec::Cbor,cbor,frame,written,32U
        )==Logging::EdlgFrameWriteResult::Succeeded,"nvs-frame"))
            return false;
        const auto stored=Logging::WriteEdlgCheckpoint(
            native,key.Value,std::span<const std::byte>(frame.data(),written),32U
        );
        if(!Check(stored.Status==Logging::EdlgCheckpointWriteStatus::Committed &&
                  stored.Store==Persistence::KeyValueStoreStatus::Succeeded,
                  "real-nvs-persistence-commit")) return false;
        native.Close();
        std::printf("EDP_LOGGING_HIL|PASS|PERSISTENCE_COMMITTED_BEFORE_RESTART\n");
        std::fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(70));
        esp_restart();
        return false;
    }
    const auto verified=
        read.Status==Logging::EdlgCheckpointReadStatus::Recovered &&
        read.FrameBytes==24U &&
        view.Codec==Logging::EdlgCodec::Cbor &&
        view.Payload.size()==4U &&
        view.Payload[0U]==std::byte{0x83U} &&
        view.Payload[1U]==std::byte{0x01U} &&
        view.Payload[2U]==std::byte{0x02U} &&
        view.Payload[3U]==std::byte{0x03U};
    if(!Check(verified,"nvs-reboot-checkpoint-verified")) return false;
    if(!Check(native.RemoveKey(key.Value)==
        Persistence::KeyValueRemoveStatus::Succeeded,"nvs-cleanup")) return false;
    native.Close();
    std::printf("EDP_LOGGING_HIL|PASS|PERSISTENCE_RESTART_RECOVERY\n");
    return true;
}

// Native app Bootstrap selects concrete ESP-IDF VFS; Logging consumes only
// abstract EDP-Persistence FileStorage and generic bounded wire inspection.
struct ArchiveMountIdentity final {};
using ArchiveBinding = ESPressio::Persistence::EspIdf::VfsBindingProfile<
 ESPressio::Persistence::RetentionLevel::Restart,
 ESPressio::Persistence::TextCaseSensitivity::CaseSensitive,
 ESPressio::Persistence::MediaRemovability::Fixed,
 32U,32U,4096U>;
using ArchiveNativeVfs = ESPressio::Persistence::EspIdf::VfsFileStorage<
 ArchiveMountIdentity,ArchiveBinding,ByteOps>;

bool TestConcreteEdlsVfsSuccessor() noexcept {
 namespace P=ESPressio::Persistence;
 const esp_vfs_spiffs_conf_t config{
    .base_path="/edpfs",
    .partition_label="edpfs",
    .max_files=4,
    .format_if_mount_failed=true
 };
 if(!Check(esp_vfs_spiffs_register(&config)==ESP_OK,"edls-vfs-spiffs-mount"))
    return false;
 static ByteOps bytes;
 ArchiveNativeVfs storage("/edpfs",bytes);
 if(!Check(storage.IsFileStorageReady(),"edls-owner-vfs-ready"))return false;
 constexpr auto path0=P::FilePathView::Validate("slot0");
 constexpr auto path1=P::FilePathView::Validate("slot1");
 static_assert(path0.IsValuePresent&&path1.IsValuePresent);
 const std::array<P::FilePathView,2U> paths{path0.Value,path1.Value};
 std::array<std::byte,16U> identity{};
 identity.fill(std::byte{0x5AU});
 constexpr std::array<std::byte,4U> payload{std::byte{0x83U},
  std::byte{0x01U},std::byte{0x02U},std::byte{0x03U}};
 std::array<std::byte,48U> serialized{};
 std::size_t size=0U;
 if(!Check(Logging::WriteEdlgFrame(Logging::EdlgCodec::Cbor,payload,serialized,size,32U)==
           Logging::EdlgFrameWriteResult::Succeeded,"edls-successor-source-frame"))
    return false;
 const auto frame=std::span<const std::byte>(serialized.data(),size);
 const auto first=Logging::PublishEdlsSuccessorIntoEmptySlot<2U,32U,1024U>(
     storage,paths,identity,1U,frame
 );
 if(!Check(first.Status==Logging::EdlsSuccessorStatus::PublishedAndVerified &&
           first.Slot==0U && first.Generation==1U,"edls-vfs-first-published"))
    return false;
 // Recreate the concrete FileStorage provider, using a new owner handle,
 // to prove a real on-flash SPIFFS read rather than an in-process cache.
 ArchiveNativeVfs opened("/edpfs",bytes);
 const auto verified=Logging::InspectEdlsArchive<2U,32U>(opened,paths);
 if(!Check(verified.Condition==Logging::EdlsArchiveCondition::Unambiguous &&
           verified.Slots[0U].VerifiedFrames==1U &&
           verified.NextGeneration==2U,"edls-vfs-reopen-verified"))return false;
 const auto second=Logging::PublishEdlsSuccessorIntoEmptySlot<2U,32U,1024U>(
     opened,paths,identity,2U,frame
 );
 if(!Check(second.Status==Logging::EdlsSuccessorStatus::PublishedAndVerified &&
           second.Slot==1U && second.Generation==2U,"edls-vfs-next-generation"))
    return false;
 const auto full=Logging::PublishEdlsSuccessorIntoEmptySlot<2U,32U,1024U>(
     opened,paths,identity,3U,frame
 );
 if(!Check(full.Status==Logging::EdlsSuccessorStatus::NoEmptySlot,
           "edls-vfs-no-destructive-rotation"))return false;
 // Controlled test-only direct corruption through native VFS, never called
 // by Logging source, and never presented as an external power-cut test.
 auto* tamper=std::fopen("/edpfs/slot1","r+b");
 if(!Check(tamper!=nullptr,"edls-vfs-corrupt-fixture-open"))return false;
 const auto position=std::fseek(tamper,Logging::EdlsHeaderBytes+16U,SEEK_SET);
 const std::uint8_t damaged=0xFEU;
 const auto written=position==0?std::fwrite(&damaged,1U,1U,tamper):0U;
 const auto flushed=std::fflush(tamper);
 const auto closed=std::fclose(tamper);
 if(!Check(position==0 && written==1U && flushed==0 && closed==0,
           "edls-vfs-corruption-committed"))return false;
 const auto broken=Logging::InspectEdlsArchive<2U,32U>(opened,paths);
 if(!Check(broken.Slots[0U].Condition==Logging::EdlsSlotCondition::Clean &&
           broken.Slots[1U].Condition==Logging::EdlsSlotCondition::DamagedTail &&
           broken.Slots[0U].VerifiedFrames==1U,
           "edls-vfs-prefix-failure-isolation"))return false;
 if(!Check(opened.RemoveFile(path0.Value)==P::FileRemoveStatus::Succeeded &&
           opened.RemoveFile(path1.Value)==P::FileRemoveStatus::Succeeded,
           "edls-vfs-test-cleanup"))return false;
 esp_vfs_spiffs_unregister("edpfs");
 std::printf("EDP_LOGGING_HIL|PASS|NATIVE_EDLS_VFS_SUCCESSOR_GUARD\n");
 return true;
}

// All competing native UART writers are quiescent while FIFO is in flight.
bool TestComposedSerialHardwareEndpoint() noexcept {
    using namespace ESPressio::Serial;
    if(!Check(!uart_is_driver_installed(UART_NUM_0),"uart0-exclusive-owner")) return false;
    if(!Check(uart_driver_install(UART_NUM_0,256,0,0,nullptr,0)==ESP_OK,
              "serial-install-unbuffered-tx")) return false;
    ESP_IDF::UartFifoByteStreamWriter writer(UART_NUM_0);
    static SerialMutex mutex;
    SerialQueue fifo(mutex);
    constexpr std::array<std::byte,4U> payload{
        std::byte{0x83U},std::byte{0x01U},
        std::byte{0x02U},std::byte{0x03U}
    };
    const auto offered=fifo.TryEnqueue(Logging::EdlgCodec::Cbor,payload);
    if(!Check(offered.FrameWasQueued &&
              offered.Status==EdlgQueueOfferStatus::Queued,"serial-owner-enqueue"))
        return false;
    std::printf("EDP_LOGGING_HIL|SERIAL_BINARY_START|24\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(40));
    bool done=false;
    for(unsigned attempts=0U;attempts<200U;++attempts) {
        const auto progress=fifo.PumpNext(writer);
        if(progress.Status==EdlgQueuePumpStatus::Complete) {
            done=progress.FrameCompleted&&progress.TotalFrameBytes==24U &&
                 fifo.Depth()==0U;
            break;
        }
        if(progress.Status!=EdlgQueuePumpStatus::WouldBlock)
            return Check(false,"serial-native-progress-failure");
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    if(!Check(done,"serial-native-frame-fully-accepted")) return false;
    if(!Check(uart_wait_tx_done(UART_NUM_0,pdMS_TO_TICKS(500))==ESP_OK,
              "serial-native-uart-tx-drained")) return false;
    std::printf("EDP_LOGGING_HIL|PASS|SERIAL_OWNER_NATIVE_UART_EDLG\n");
    std::fflush(stdout);
    return true;
}

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
    // Real ESP32 pooled source record, 3 ordered app-composed sinks, one
    // nonduplicated native lease reclamation even with partial fanout.
    {
        Lease scratch;
        Queue::Reservation reservation;
        const auto acquired=Logging::AcquireRecordUntil<8U,8U>(
            queue,lifecycle,wakes,0U,pool,scratch,reservation,
            Deadline(2000U),neverStop,917);
        if(!Check(acquired.Status==Logging::RecordAdmissionStatus::Admitted,
                  "fanout-native-memory-acquisition")) return false;
        if(!Check(reservation.Commit()==Logging::RecordQueueResult::Succeeded,
                  "fanout-queue-commit")) return false;
        unsigned called[3U]{};
        const auto first=[&](const Record& record) noexcept ->
            Logging::RecordSinkSubmission {
            ++called[0]; return record.Value()==917
                ? Logging::RecordSinkSubmission::Accepted
                : Logging::RecordSinkSubmission::Rejected;
        };
        const auto second=[&](const Record& record) noexcept ->
            Logging::RecordSinkSubmission {
            ++called[1]; return record.Value()==917
                ? Logging::RecordSinkSubmission::Rejected
                : Logging::RecordSinkSubmission::ProviderFailure;
        };
        const auto third=[&](const Record& record) noexcept ->
            Logging::RecordSinkSubmission {
            ++called[2]; return record.Value()==917
                ? Logging::RecordSinkSubmission::ProviderFailure
                : Logging::RecordSinkSubmission::Rejected;
        };
        Queue::ConsumerClaim claim;
        const auto report=Logging::DrainOneRecordToSinks(
            queue,claim,first,second,third
        );
        if(!Check(report.Source.Status==
                      Logging::RecordDrainStatus::FailedAndReclaimed &&
                  report.Source.Submission==Logging::RecordSinkSubmission::Partial &&
                  report.Attempted==3U && report.Accepted==1U &&
                  report.Unaccepted==2U &&
                  report.Submissions[0]==Logging::RecordSinkSubmission::Accepted &&
                  report.Submissions[1]==Logging::RecordSinkSubmission::Rejected &&
                  report.Submissions[2]==Logging::RecordSinkSubmission::ProviderFailure &&
                  called[0]==1U && called[1]==1U && called[2]==1U &&
                  Record::Live.load()==0 && !claim.IsActive(),
                  "real-memory-multisink-source-retired")) return false;
        std::printf("EDP_LOGGING_HIL|PASS|MULTI_SINK_NATIVE_RECLAMATION\n");
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
    // Actual ESP32 Memory-pool queue through two statically composed sinks.
    // A quantum budget is not a queue-empty or durable-delivery assertion.
    {
        for (int value : {921,922}) {
            Lease scratch;
            Queue::Reservation reservation;
            const auto admission=Logging::AcquireRecordUntil<8U,8U>(
                queue,lifecycle,wakes,0U,pool,scratch,reservation,
                Deadline(2000U),neverStop,value);
            if (!Check(admission.Status==Logging::RecordAdmissionStatus::Admitted,
                        "quantum-hardware-admission")) return false;
            if (!Check(reservation.Commit()==Logging::RecordQueueResult::Succeeded,
                        "quantum-hardware-publication")) return false;
        }
        unsigned sinkA=0U,sinkB=0U;
        const auto a=[&sinkA](const Record& record) noexcept ->
            Logging::RecordSinkSubmission {
            ++sinkA;
            return record.Value()==921 ? Logging::RecordSinkSubmission::Accepted :
                                         Logging::RecordSinkSubmission::Rejected;
        };
        const auto b=[&sinkB](const Record&) noexcept ->
            Logging::RecordSinkSubmission {
            ++sinkB;
            return Logging::RecordSinkSubmission::Accepted;
        };
        Queue::ConsumerClaim claim;
        const auto q1=Logging::DrainCoordinatorQuantum<1U>(queue,claim,a,b);
        if (!Check(q1.Status==Logging::CoordinatorQuantumStatus::BudgetConsumed &&
            q1.Processed==1U && q1.AcceptedByAll==1U &&
            Record::Live.load()==1,"quantum-first-budget")) return false;
        const auto q2=Logging::DrainCoordinatorQuantum<1U>(queue,claim,a,b);
        if (!Check(q2.Status==Logging::CoordinatorQuantumStatus::BudgetConsumed &&
            q2.Processed==1U && q2.FailedSubmissionRecords==1U &&
            q2.PerSinkUnaccepted[0U]==1U && Record::Live.load()==0,
            "quantum-partial-and-reclaim")) return false;
        const auto q3=Logging::DrainCoordinatorQuantum<1U>(queue,claim,a,b);
        if (!Check(q3.Status==Logging::CoordinatorQuantumStatus::QueueEmpty &&
            q3.Processed==0U && sinkA==2U && sinkB==2U,
            "quantum-queue-empty")) return false;
        std::printf("EDP_LOGGING_HIL|PASS|BOUNDED_COORDINATOR_QUANTUM\n");
    }
    // G10: actual topology-owned EDP-Threading Dedicated Thread
    // (not an application-created native FreeRTOS worker) runs the
    // bounded Logging coordinator across real Memory/FIFO records.
    {
        struct CoordinatorThreadIdentity final {};
        struct TestSinkA {
            std::atomic<unsigned>* Count{};
            Logging::RecordSinkSubmission operator()(const Record& record) noexcept {
                Count->fetch_add(1U);
                return record.Value()==951 ? Logging::RecordSinkSubmission::Accepted
                                           : Logging::RecordSinkSubmission::Rejected;
            }
        };
        struct TestSinkB {
            std::atomic<unsigned>* Count{};
            Logging::RecordSinkSubmission operator()(const Record&) noexcept {
                Count->fetch_add(1U);
                return Logging::RecordSinkSubmission::Accepted;
            }
        };
        using Callable=Logging::DedicatedCoordinator<1U,5'000'000LL,
            Queue,TestSinkA,TestSinkB>;
        using Topology=Threading::ThreadingTopology<
            Threading::DedicatedThread<
                CoordinatorThreadIdentity,
                Threading::StackCapacity<6144U>,
                Threading::Priority<Threading::ThreadPriority::Normal>,
                Threading::AnyAffinity
            >
        >;
        struct MeasuredCallable final {
            Callable Inner;
            std::atomic<unsigned>* Margin;
            explicit MeasuredCallable(
                Callable&& inner, std::atomic<unsigned>& margin
            ) noexcept : Inner(std::move(inner)), Margin(&margin) {}
            MeasuredCallable(MeasuredCallable&&) noexcept = default;
            MeasuredCallable(const MeasuredCallable&) = delete;
            MeasuredCallable& operator=(const MeasuredCallable&) = delete;
            void operator()(Threading::ThreadContext& context) noexcept {
                Inner(context);
                // One snapshot *after* the final worker callback, in the
                // actual native thread, before its EDP-Threading exit path.
                Margin->store(
                    static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                    std::memory_order_release
                );
            }
        };
        using Binding=decltype(Threading::BindDedicatedThread<CoordinatorThreadIdentity>(
            std::declval<MeasuredCallable>()
        ));
        using NativeThreading=Threading::StaticThreadingRuntime<
            Topology,std::tuple<Binding>,Signal,
            Platform::ESPIDF::Execution::ExecutionContextProvider,
            Platform::ESPIDF::Synchronization::SpinLockProvider,Mutex
        >;
        static std::atomic<unsigned> callsA{0U},callsB{0U};
        static TestSinkA sinkA{&callsA};
        static TestSinkB sinkB{&callsB};
        static Logging::DedicatedCoordinatorReport report{};
        static NativeThreading dedicated(std::make_tuple(
            Threading::BindDedicatedThread<CoordinatorThreadIdentity>(
                MeasuredCallable(
                    Callable(queue,report,sinkA,sinkB),
                    DedicatedWorkerStackMarginBytes
                )
            )
        ));
        if(!Check(dedicated.Initialize()==
            Threading::ThreadingInitializationResult::Succeeded,
            "dedicated-thread-initialize")) return false;
        if(!Check(dedicated.Start()==
            Threading::ThreadingStartResult::Succeeded,
            "dedicated-thread-infrastructure")) return false;
        auto worker=dedicated.ThreadHandle<CoordinatorThreadIdentity>();
        if(!Check(worker.Start()==Threading::ThreadStartResult::Started,
            "dedicated-thread-activation")) return false;
        for(int value: {951,952}) {
            Lease scratch;
            Queue::Reservation owner;
            const auto acquired=Logging::AcquireRecordUntil<8U,8U>(
                queue,lifecycle,wakes,0U,pool,scratch,owner,
                Deadline(2000U),neverStop,value);
            if(!Check(acquired.Status==Logging::RecordAdmissionStatus::Admitted,
                "dedicated-thread-record-admit")) return false;
            if(!Check(owner.Commit()==Logging::RecordQueueResult::Succeeded,
                "dedicated-thread-record-commit")) return false;
            if(!Check(worker.Wake()==Threading::ThreadWakeResult::Woken,
                "dedicated-thread-native-wake")) return false;
        }
        for(unsigned wait=0U;wait<200U &&
            report.RecordsRetired.load()<2U;++wait) {
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
        if(!Check(report.RecordsRetired.load()==2U &&
            callsA.load()==2U && callsB.load()==2U &&
            report.AllSinksAccepted.load()==1U &&
            report.PartialSubmissions.load()==1U &&
            Record::Live.load()==0,
            "dedicated-thread-two-source-fanout")) return false;
        if(!Check(worker.RequestStop()==
            Threading::ThreadStopRequestResult::Accepted,
            "dedicated-thread-stop-request")) return false;
        if(!Check(worker.Wake()==Threading::ThreadWakeResult::Woken,
            "dedicated-thread-stop-wake")) return false;
        // EDP-Threading::JoinFor is only legal from an EDP-managed
        // execution context; this app_main Bootstrap is intentionally
        // unmanaged and must NOT register as a synthetic managed waiter.
        if(!Check(worker.JoinFor(Clock::Duration::FromNanoseconds(1'000'000LL))==
            Threading::ThreadJoinResult::Interrupted,
            "dedicated-thread-unmanaged-join-refused")) return false;
        // Bootstrap uses the authoritative topology-wide quiescence
        // predicate with one absolute physical-time budget.
        const std::int64_t stopDeadlineUs=esp_timer_get_time()+2'000'000LL;
        while(!dedicated.IsExecutionQuiescent() &&
              esp_timer_get_time()<stopDeadlineUs) {
            vTaskDelay(pdMS_TO_TICKS(5U));
        }
        if(!Check(dedicated.IsExecutionQuiescent() &&
            report.Exit.load()==Logging::DedicatedCoordinatorExit::CooperativeStop &&
            DedicatedWorkerStackMarginBytes.load(std::memory_order_acquire)>0U &&
            DedicatedWorkerStackMarginBytes.load(std::memory_order_acquire)<=6144U,
            "dedicated-thread-finite-quiescence")) return false;
        if(!Check(dedicated.BeginShutdown()==
            Threading::ThreadingShutdownResult::Accepted,
            "dedicated-thread-begin-shutdown")) return false;
        if(!Check(dedicated.FinalizeShutdown()==
            Threading::ThreadingFinalizationResult::Completed,
            "dedicated-thread-finalize-shutdown")) return false;
        std::printf("EDP_LOGGING_HIL|PASS|EDP_THREADING_DEDICATED_COORDINATOR\n");
    }
    // Actual WROOM EDP-Memory OWNED encoded payload survives Serial FIFO
    // copy-on-enqueue; acceptance is not UART delivery or flash durability.
    {
        using Encoded=Logging::OwnedEncodedPayload<32U>;
        using EncodedTopology=Memory::MemoryTopology<
            Resource,Memory::SharedReserve<0U,Resource>,
            Memory::ObjectPoolSpec<Encoded,Memory::DedicatedInstances<1U>>
        >;
        using EncodedRuntime=Memory::MemoryRuntime<
            EncodedTopology,Composition,Mutex,Signal
        >;
        Resource encodedResource;
        Mutex nativeMutex;
        Allocator encodedAllocator;
        EncodedRuntime encodedRuntime(nativeMutex,encodedAllocator,encodedResource);
        Memory::MemoryTopologyInitializationFailure failure{};
        if(!Check(encodedRuntime.Initialize(failure)==
            Memory::MemoryTopologyInitializationResult::Succeeded,
            "encoded-owner-pool-initialize")) return false;
        auto& encodedPool=encodedRuntime.ObjectPoolFor<Encoded>();
        using EncodedLease=std::remove_reference_t<decltype(encodedPool)>::LeaseType;
        EncodedLease encodedLease;
        SerialMutex copiedSerialMutex;
        SerialQueue copiedSerial(copiedSerialMutex);
        struct ArchiveSink {
            std::array<std::byte,32U> Copied{};
            std::size_t Used{0U};
            unsigned Calls{0U};
            Logging::RecordSinkSubmission operator()(
                Logging::EdlgCodec, std::span<const std::byte> payload
            ) noexcept {
                ++Calls;
                Used=payload.size();
                for(std::size_t i=0U;i<Used;++i) Copied[i]=payload[i];
                return Logging::RecordSinkSubmission::Accepted;
            }
        } archiveSink;
        struct Encoder {
            unsigned Calls{0U};
            Logging::RepresentationEncodeResult operator()(
                const std::uint32_t& source, std::span<std::byte> output
            ) noexcept {
                ++Calls;
                const auto result=
                    ESPressio::Serialisation::SerialisePositionalCbor<1U>(
                        output,source
                    );
                return {
                    result.IsSuccessful()
                        ? Logging::RepresentationEncodeStatus::Succeeded
                        : Logging::RepresentationEncodeStatus::Rejected,
                    result.BytesWritten
                };
            }
        } encoder;
        auto serialSink=[&copiedSerial](
            Logging::EdlgCodec codec,std::span<const std::byte> payload
        ) noexcept -> Logging::RecordSinkSubmission {
            const auto offered=copiedSerial.TryEnqueue(codec,payload);
            return offered.Status==ESPressio::Serial::EdlgQueueOfferStatus::Queued
                ? Logging::RecordSinkSubmission::Accepted
                : offered.FrameWasQueued
                    ? Logging::RecordSinkSubmission::Partial
                    : Logging::RecordSinkSubmission::Rejected;
        };
        for(std::uint32_t source=11U;source<14U;++source) {
            const auto delivered=Logging::EncodeOnceAndSubmitToSinks<
                Logging::EdlgCodec::Cbor,32U>(
                encodedPool,encodedLease,source,encoder,serialSink,archiveSink
            );
            if(!Check(encodedLease.IsEmpty() &&
                delivered.EncoderInvocations==1U &&
                delivered.SinkInvocations==2U &&
                delivered.EncodedBytes!=0U &&
                archiveSink.Used==delivered.EncodedBytes &&
                ((source<13U &&
                  delivered.Status==Logging::OwnedRepresentationStatus::SubmittedAndReleased &&
                  delivered.Accepted==2U) ||
                 (source==13U &&
                  delivered.Status==Logging::OwnedRepresentationStatus::SubmissionNotAccepted &&
                  delivered.Accepted==1U)),
                "native-encoded-per-format-fanout")) return false;
        }
        struct Endpoint final : ESPressio::Serial::Framework::Provider<
            ESPressio::Serial::Domain,
            ESPressio::Serial::Framework::Offers<
                ESPressio::Serial::Framework::Offer<ESPressio::Serial::ByteStreamWriter>
            >
        > {
            std::array<std::byte,128U> Bytes{};
            std::size_t Used{0U};
            ESPressio::Serial::ByteWriteResult TryWrite(
                std::span<const std::byte> payload
            ) noexcept {
                if(payload.size()>Bytes.size()-Used)
                    return {ESPressio::Serial::ByteWriteStatus::ProviderFailure,0U};
                for(const auto item:payload) Bytes[Used++]=item;
                return {ESPressio::Serial::ByteWriteStatus::Accepted,payload.size()};
            }
        } endpoint;
        for(std::uint32_t source=11U;source<13U;++source) {
            const auto sent=copiedSerial.PumpNext(endpoint);
            if(!Check(sent.Status==ESPressio::Serial::EdlgQueuePumpStatus::Complete &&
                sent.FrameCompleted,"native-encoded-serial-complete")) return false;
        }
        std::size_t offset=0U;
        for(std::uint32_t source=11U;source<13U;++source) {
            Logging::EdlgFrameView frame{};
            if(!Check(Logging::ReadEdlgFrame(
                std::span<const std::byte>(endpoint.Bytes).subspan(offset),
                frame,32U)==Logging::EdlgFrameReadResult::Succeeded &&
                frame.Codec==Logging::EdlgCodec::Cbor &&
                !frame.Payload.empty(),
                "native-encoded-edlg-crc")) return false;
            offset+=frame.Consumed;
        }
        // G10: one-sink output is a genuine application profile; there is no
        // requirement to allocate a dummy second sink. The same real
        // EDP-Memory-owned payload is encoded once, copied by its only
        // consumer, and released before the consumer transmits later.
        const std::uint32_t oneSerialSource=21U;
        const auto serialOnly=Logging::EncodeOnceAndSubmitToSinks<
            Logging::EdlgCodec::Cbor,32U>(
                encodedPool,encodedLease,oneSerialSource,encoder,serialSink
            );
        if(!Check(serialOnly.Status==
                Logging::OwnedRepresentationStatus::SubmittedAndReleased &&
                serialOnly.EncoderInvocations==1U &&
                serialOnly.SinkInvocations==1U &&
                serialOnly.Accepted==1U &&
                serialOnly.Submissions[0]==Logging::RecordSinkSubmission::Accepted &&
                copiedSerial.Depth()==1U && encodedLease.IsEmpty(),
                "native-g10-single-serial-owned")) return false;
        const auto lonePumped=copiedSerial.PumpNext(endpoint);
        if(!Check(lonePumped.Status==
                ESPressio::Serial::EdlgQueuePumpStatus::Complete &&
                lonePumped.FrameCompleted && copiedSerial.Depth()==0U,
                "native-g10-single-serial-transmitted")) return false;
        Logging::EdlgFrameView loneFrame{};
        if(!Check(Logging::ReadEdlgFrame(
                std::span<const std::byte>(endpoint.Bytes).subspan(offset),
                loneFrame,32U)==Logging::EdlgFrameReadResult::Succeeded &&
                loneFrame.Codec==Logging::EdlgCodec::Cbor &&
                loneFrame.Consumed==endpoint.Used-offset &&
                !loneFrame.Payload.empty(),
                "native-g10-single-serial-crc")) return false;
        const auto archiveBefore=archiveSink.Calls;
        const std::uint32_t oneArchiveSource=22U;
        const auto archiveOnly=Logging::EncodeOnceAndSubmitToSinks<
            Logging::EdlgCodec::Cbor,32U>(
                encodedPool,encodedLease,oneArchiveSource,encoder,archiveSink
            );
        if(!Check(archiveOnly.Status==
                Logging::OwnedRepresentationStatus::SubmittedAndReleased &&
                archiveOnly.EncoderInvocations==1U &&
                archiveOnly.SinkInvocations==1U &&
                archiveOnly.Accepted==1U &&
                archiveOnly.Submissions[0]==Logging::RecordSinkSubmission::Accepted &&
                archiveSink.Calls==archiveBefore+1U &&
                archiveSink.Used==archiveOnly.EncodedBytes &&
                copiedSerial.Depth()==0U && encodedLease.IsEmpty(),
                "native-g10-single-archive-owned")) return false;
        if(!Check(encoder.Calls==5U && archiveSink.Calls==4U &&
            encodedRuntime.TearDown()==Memory::MemoryTopologyTeardownResult::Succeeded,
            "native-encoded-memory-reclaimed")) return false;
        std::printf("EDP_LOGGING_HIL|PASS|MEMORY_OWNED_ENCODING_SERIAL_COPY\n");
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
    // Whole-test-app envelope, not an attribution to the Logging library:
    // platform/Flash/VFS/Threading and the startup task also consume resources.
    const auto timeBeforeUs=esp_timer_get_time();
    const auto freeBefore=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    std::printf("EDP_LOGGING_HIL|START|G1_WROOM_REAL_EDP_PROVIDERS\n");
    EDP_LOG_WHEN_ENABLED(
        Test::G11SiteThreshold, Test::Logging::Severity::Info,
        Test::G11ClockRead();
        Test::G11IdentityRead();
        Test::G11ArgumentRead()
    );
    const auto g11Clock = Test::G11ClockReads.load(std::memory_order_relaxed);
    const auto g11Identity = Test::G11IdentityReads.load(std::memory_order_relaxed);
    const auto g11Arguments = Test::G11ArgumentReads.load(std::memory_order_relaxed);
    const bool g11SiteMatches = g11Clock == EDP_LOGGING_G11_APP_SITE_ENABLED &&
        g11Identity == EDP_LOGGING_G11_APP_SITE_ENABLED &&
        g11Arguments == EDP_LOGGING_G11_APP_SITE_ENABLED;
    std::printf(
        "EDP_LOGGING_HIL|G11_WHOLE_FIRMWARE_SITE|selected=%d|clock=%u|identity=%u|arguments=%u|result=%s\n",
        EDP_LOGGING_G11_APP_SITE_ENABLED,
        static_cast<unsigned>(g11Clock),
        static_cast<unsigned>(g11Identity),
        static_cast<unsigned>(g11Arguments),
        g11SiteMatches ? "PASS" : "FAIL"
    );

    const bool nvs=Test::TestNvsCheckpointAcrossReboot();
    const bool vfs=nvs && Test::TestConcreteEdlsVfsSuccessor();
    const bool governed=vfs && Test::TestGovernedLogRecordValidation();
    const bool core=governed && Test::Run();
    const bool success=core && Test::TestComposedSerialHardwareEndpoint();
    const auto elapsedUs=esp_timer_get_time()-timeBeforeUs;
    const auto freeAfter=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const auto minimumFree=heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    // ESP-IDF FreeRTOS reports the high-water stack margin in bytes on
    // ESP32. It is specifically app_main task margin, NOT every worker.
    const auto stackMargin=uxTaskGetStackHighWaterMark(nullptr);
    const auto dedicatedWorkerMargin=
        Test::DedicatedWorkerStackMarginBytes.load(std::memory_order_acquire);
    const bool resourceMeasurementValid=
        g11SiteMatches && success && elapsedUs>0 && freeBefore>0 && freeAfter>0 &&
        minimumFree>0 && minimumFree<=freeAfter && minimumFree<=freeBefore &&
        stackMargin>0 && dedicatedWorkerMargin>0 &&
        dedicatedWorkerMargin<=6144U;
    std::printf("EDP_LOGGING_HIL|RESOURCE|app_scope=whole_test_bundle|"
                "free8_before=%llu|free8_after=%llu|free8_min=%llu|"
                "app_main_stack_margin_bytes=%llu|"
                "dedicated_worker_stack_margin_bytes=%llu|elapsed_us=%lld|"
                "static_record_bytes=%llu|static_record_queue_bytes=%llu\n",
                static_cast<unsigned long long>(freeBefore),
                static_cast<unsigned long long>(freeAfter),
                static_cast<unsigned long long>(minimumFree),
                static_cast<unsigned long long>(stackMargin),
                static_cast<unsigned long long>(dedicatedWorkerMargin),
                static_cast<long long>(elapsedUs),
                static_cast<unsigned long long>(sizeof(Test::Record)),
                static_cast<unsigned long long>(sizeof(Test::Queue)));
    if(resourceMeasurementValid)
        std::printf("EDP_LOGGING_HIL|PASS|WROOM_WHOLE_APP_RESOURCE_SNAPSHOT\n");
    std::printf("EDP_LOGGING_HIL|RESULT|%s\n",
                resourceMeasurementValid?"PASS":"FAIL");
    std::fflush(stdout);
    for(;;) vTaskDelay(pdMS_TO_TICKS(1000U));
}
