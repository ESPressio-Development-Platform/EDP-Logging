// Genuine EDP-Memory pool + real EDP-Threading Task handle ABI +
// existing deferred test scheduler, owner-generated typed LogRecord,
// canonical CBOR, and real filesystem-backed immutable EDLS completion.
#define EDP_LOGGING_G1_LIFECYCLE_TEST_INCLUDE
#include "AdmissionWakeCoordinatorTests.cpp"
#define EDP_LOGGING_ASYNC_SKIP_ADMISSION_INCLUDE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main EDPExistingAsyncSinkTestMain
#include "AsyncCopyingTaskSinkTests.cpp"
#undef main
#pragma GCC diagnostic pop
#undef EDP_LOGGING_ASYNC_SKIP_ADMISSION_INCLUDE

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <GeneratedLocalisationContract.hpp>
#include <VfsFileStorage.hpp>
#include <logging/DiagnosticClockEraArguments.hpp>
#include <logging/LogRecordSourceAssembly.hpp>
#include <logging/GeneratedEdlsTaskAdapters.hpp>

namespace {
    namespace S=ESPressio::System;
    namespace C=ESPressio::Clock;
    namespace Serial=ESPressio::Serialisation;
    using Schema=Fixture::Diagnostic::DiagnosticMessageSchema<1U,0U,5U>;
    using Record=L::BasicLogRecord<L::AttemptSequenceWidth::Bits16,2U,12U>;
    using Projection=L::GeneratedLogRecordWireProjection<
        L::AttemptSequenceWidth::Bits16,
        S::Identity::RuntimeIncarnationId,C::ClockEra>;
    using Scratch=L::OwnedPositionalScratch<
        S::Identity::RuntimeIncarnationId,C::ClockEra>;
    using Value=L::OwnedEncodedPayload<1024U>;
    using TypedTaskMemoryTopology=M::MemoryTopology<
        Resource,M::SharedReserve<0U,Resource>,
        M::ObjectPoolSpec<Value,M::DedicatedInstances<2U>>
    >;
    using TypedTaskRuntime=M::MemoryRuntime<
        TypedTaskMemoryTopology,MemoryComposition,
        TestSupport::TestMutexProvider,TestSupport::TestSignalProvider
    >;
    struct Binding final {};
    using Profile=ESPressio::Persistence::EspIdf::VfsBindingProfile<
        ESPressio::Persistence::RetentionLevel::Restart,
        ESPressio::Persistence::TextCaseSensitivity::CaseSensitive,
        ESPressio::Persistence::MediaRemovability::Fixed,64U,64U,4096U>;
    using Store=ESPressio::Persistence::EspIdf::VfsFileStorage<
        Binding,Profile,PortableBytes>;
    constexpr auto P0=ESPressio::Persistence::FilePathView::Validate("slot0");
    constexpr auto P1=ESPressio::Persistence::FilePathView::Validate("slot1");
    constexpr auto P2=ESPressio::Persistence::FilePathView::Validate("slot2");
    static_assert(P0.IsValuePresent && P1.IsValuePresent &&
                  P2.IsValuePresent);
    constexpr std::array<ESPressio::Persistence::FilePathView,3U> Paths{
        P0.Value,P1.Value,P2.Value};
    Record make_record(
        const S::Identity::DeviceIdentifier& device,
        std::uint32_t generation,
        std::uint32_t runtimeIncarnation=7U
    ) {
        const L::QualifiedLogTime temporal{
            C::MonotonicTimestamp::FromNanoseconds(123456U+generation),
            C::SynchronizedTimestamp{},
            C::SynchronizationUncertainty::Maximum(),
            C::ClockEra{},
            C::SynchronizationState::NeverSynchronized,
            L::LogTemporalEvidenceStatus::Unavailable
        };
        Record record{};
        const auto attempt=L::AttemptSequence<
            L::AttemptSequenceWidth::Bits16>::FromValidatedValue(generation);
        assert(L::AssembleLogRecordSource<Schema>(
            record,L::Severity::Warning,device,
            S::Identity::RuntimeIncarnationId{runtimeIncarnation},attempt,temporal,
            S::Identity::RuntimeIncarnationId{0x01020304U},
            C::ClockEra::FromValue(0x0102030405060708ULL)
        )==L::LogRecordAssemblyStatus::Prepared);
        return record;
    }
    void ExerciseActualTaskLifecycle() {
        char dir[]="/tmp/edp-actual-typed-task-XXXXXX";
        const auto folder=::mkdtemp(dir);
        assert(folder!=nullptr);
        Resource resource;
        TestSupport::TestMutexProvider native{};
        MemoryComposition::Select<
            M::SharedReserveAllocationRequirement,
            ESPressio::System::CompositionFramework::SelectUnique
        > allocator;
        TypedTaskRuntime memory(native,allocator,resource);
        M::MemoryTopologyInitializationFailure failure{};
        assert(memory.Initialize(failure)==
            M::MemoryTopologyInitializationResult::Succeeded);
        auto& pool=memory.ObjectPoolFor<Value>();
        using Pool=std::remove_reference_t<decltype(pool)>;
        Pool::LeaseType sourceLease{};
        Pool::LeaseType copyLease{};
        std::optional<T::Task<L::RecordSinkSubmission>> task;
        DeferredTaskExecutor executor{};
        PortableBytes bytes{};
        Store storage(folder,bytes);
        assert(storage.IsFileStorageReady());
        Projection encoderProjection{},decodedProjection{};
        Scratch scratch{};
        std::array<std::byte,1200U> frame{},archiveScratch{};
        S::Identity::DeviceIdentifier::Storage deviceRaw{};
        deviceRaw.fill(0xA6U);
        S::Identity::DeviceIdentifier device{deviceRaw};
        std::array<std::byte,16U> deviceBytes{};
        deviceBytes.fill(std::byte{0xA6U});
        L::GeneratedLogRecordCborEncoder<
            Schema,L::AttemptSequenceWidth::Bits16,2U,12U,
            S::Identity::RuntimeIncarnationId,C::ClockEra
        > encoder(encoderProjection,scratch);
        using Delivery=L::TypedCborProtectedEdlsTaskDelivery<
            Schema,L::AttemptSequenceWidth::Bits16,3U,2U,
            1024U,1200U,Store,
            S::Identity::RuntimeIncarnationId,C::ClockEra>;
        Delivery delivered(storage,decodedProjection,frame,archiveScratch,
                           Paths,deviceBytes,7U);
        L::AsyncCopyingTaskSink<
            L::EdlgCodec::Cbor,1024U,Pool,PortableBytes,
            DeferredTaskExecutor,Delivery
        > sink(pool,copyLease,task,bytes,executor,delivered);
        for(std::uint32_t gen=1U;gen<=16U;++gen) {
            Record source=make_record(device,gen);
            const auto admission=L::EncodeOnceAndSubmitToSinks<
                L::EdlgCodec::Cbor,1024U
            >(pool,sourceLease,source,encoder,sink);
            assert(admission.Status==
                L::OwnedRepresentationStatus::SubmittedAndReleased);
            assert(admission.Accepted==1U && admission.EncoderInvocations==1U);
            assert(sourceLease.IsEmpty() && !copyLease.IsEmpty() &&
                task.has_value());
            // Task admission is never a durable archive ACK. The caller can
            // destroy/change the source and its encoder-owned projection
            // while the copied Memory lease is still in flight.
            assert(delivered.LastArchive.Generation!=gen);
            source=Record{};
            encoderProjection=Projection{};
            assert(sink.TryFinalize().Status==
                L::AsyncSinkCompletionStatus::Pending);
            // Real bounded fanout backpressure: a second different valid
            // generated record may acquire/release its own encoding lease,
            // but the already pending Task must not be overwritten and may
            // not acknowledge either generation prematurely.
            Record blocked=make_record(device,gen+1U);
            const auto concurrent=L::EncodeOnceAndSubmitToSinks<
                L::EdlgCodec::Cbor,1024U
            >(pool,sourceLease,blocked,encoder,sink);
            assert(concurrent.Status==
                L::OwnedRepresentationStatus::SubmissionNotAccepted);
            assert(concurrent.Accepted==0U && concurrent.EncoderInvocations==1U);
            assert(concurrent.Submissions[0U]==L::RecordSinkSubmission::Rejected);
            assert(sourceLease.IsEmpty() && !copyLease.IsEmpty() &&
                task.has_value());
            blocked=Record{};
            assert((L::InspectEdlsArchive<3U,1024U>(storage,Paths)
                .NextGeneration==gen));
            executor.Execute(); // actual EDP-Threading Task result callback
            const auto terminal=sink.TryFinalize();
            assert(terminal.Status==
                L::AsyncSinkCompletionStatus::AcceptedAndReleased);
            assert(terminal.Delivery==L::RecordSinkSubmission::Accepted);
            assert(delivered.LastCompletion==
                L::TypedEdlsTaskCompletion::Archived);
            assert(delivered.LastArchive.Generation==gen);
            assert(copyLease.IsEmpty() && !task.has_value());
            Store reopened(folder,bytes);
            const auto physical=L::InspectEdlsArchive<3U,1024U>(
                reopened,Paths);
            assert(physical.Condition==L::EdlsArchiveCondition::Unambiguous);
            assert(physical.NextGeneration==gen+1U);
            std::size_t retained=0U;
            for(const auto& slot:physical.Slots)
                retained+=slot.Condition==L::EdlsSlotCondition::Clean;
            assert(retained==(gen==1U?1U:2U));
        }
        assert(memory.TearDown()==M::MemoryTopologyTeardownResult::Succeeded);
        std::size_t cleared=0U;
        for(std::size_t index=0U;index<Paths.size();++index) {
            char path[200U]{};
            assert(std::snprintf(path,sizeof(path),"%s/slot%zu",
                folder,index)>0);
            if(::unlink(path)==0)++cleared;
            else assert(errno==ENOENT);
        }
        assert(cleared==2U);
        assert(::rmdir(folder)==0);
    }
}
#ifndef EDP_LOGGING_STRICT_TASK_REUSE
int main() {
    ExerciseActualTaskLifecycle();
}
#endif
