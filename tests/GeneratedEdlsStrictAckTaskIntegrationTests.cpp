// One real generated 15-field record through existing EDP-Memory pooling,
// owner-generated canonical CBOR, EDP-Threading Task and FileStorage,
// now with a separately PowerLoss-atomic KeyValueStorage ACK watermark.
// Reuse actual existing foundation fixtures rather than parallel implementations.
#define EDP_LOGGING_STRICT_TASK_REUSE
#include "GeneratedEdlsAsyncTaskIntegrationTests.cpp"
#undef EDP_LOGGING_STRICT_TASK_REUSE
#include "StrictAckKeyValueFake.hpp"

#include <logging/EdlsPowerLossAcknowledgement.hpp>

namespace {
    namespace PK=ESPressio::Persistence;
    constexpr auto AckKey=PK::KeyView::Validate("a");
    static_assert(AckKey.IsValuePresent);
    using StrictArchive=L::PowerLossAcknowledgedEdls<
        3U,2U,1024U,1200U,Store,
        L::StrictAckTests::TestAckKeyValueStorage>;
    using StrictDelivery=L::TypedCborProtectedEdlsTaskDelivery<
        Schema,L::AttemptSequenceWidth::Bits16,3U,2U,
        1024U,1200U,StrictArchive,
        S::Identity::RuntimeIncarnationId,C::ClockEra>;

    void RunStrictTypedTask() {
        char dir[]="/tmp/edp-typed-strict-nvs-XXXXXX";
        char* folder=::mkdtemp(dir);
        assert(folder!=nullptr);
        Resource resource{};
        TestSupport::TestMutexProvider native{};
        MemoryComposition::Select<
            M::SharedReserveAllocationRequirement,
            ESPressio::System::CompositionFramework::SelectUnique
        > allocator{};
        TypedTaskRuntime memory(native,allocator,resource);
        M::MemoryTopologyInitializationFailure failure{};
        assert(memory.Initialize(failure)==
            M::MemoryTopologyInitializationResult::Succeeded);
        auto& pool=memory.ObjectPoolFor<Value>();
        using Pool=std::remove_reference_t<decltype(pool)>;
        Pool::LeaseType sourceLease{},copyLease{};
        std::optional<T::Task<L::RecordSinkSubmission>> task{};
        DeferredTaskExecutor executor{};
        PortableBytes bytes{};
        Store storage(folder,bytes);
        assert(storage.IsFileStorageReady());
        L::StrictAckTests::TestAckKeyValueStorage ledger{};
        S::Identity::DeviceIdentifier::Storage rawDevice{};
        rawDevice.fill(0xA6U);
        S::Identity::DeviceIdentifier device{rawDevice};
        std::array<std::byte,16U> deviceBytes{};
        deviceBytes.fill(std::byte{0xA6U});

        StrictArchive oldBoot(storage,ledger,Paths,AckKey.Value,
                              deviceBytes,7U);
        assert(oldBoot.InitializeEmpty().Status==
            L::EdlsAcknowledgedStatus::Initialized);
        assert(oldBoot.Read().AcknowledgedGeneration==0U);

        auto runBoot=[&](StrictArchive& archive,
                         std::uint32_t incarnation,
                         std::uint32_t first,
                         std::uint32_t last) {
            Projection encoderProjection{},decodedProjection{};
            Scratch positionalScratch{};
            std::array<std::byte,1200U> frame{},archiveScratch{};
            L::GeneratedLogRecordCborEncoder<
                Schema,L::AttemptSequenceWidth::Bits16,2U,12U,
                S::Identity::RuntimeIncarnationId,C::ClockEra
            > encoder(encoderProjection,positionalScratch);
            StrictDelivery delivered(archive,decodedProjection,frame,archiveScratch,
                               Paths,deviceBytes,incarnation);
            L::AsyncCopyingTaskSink<
                L::EdlgCodec::Cbor,1024U,Pool,PortableBytes,
                DeferredTaskExecutor,StrictDelivery
            > sink(pool,copyLease,task,bytes,executor,delivered);
            for(std::uint32_t g=first;g<=last;++g) {
                Record source=make_record(device,g,incarnation);
                const auto state=L::EncodeOnceAndSubmitToSinks<
                    L::EdlgCodec::Cbor,1024U
                >(pool,sourceLease,source,encoder,sink);
                assert(state.Status==
                    L::OwnedRepresentationStatus::SubmittedAndReleased);
                assert(state.Accepted==1U && state.EncoderInvocations==1U);
                assert(sourceLease.IsEmpty() && !copyLease.IsEmpty() &&
                    task.has_value());
                // Task admission is NOT durable ACK; source may disappear.
                assert(archive.Read().AcknowledgedGeneration==g-1U);
                // The prior terminal status may legitimately remain cached
                // until this next Task executes. Its generation must NOT
                // have advanced on admission.
                assert(delivered.LastAcknowledgement.AcknowledgedGeneration!=g);
                source=Record{};
                encoderProjection=Projection{};
                assert(sink.TryFinalize().Status==
                    L::AsyncSinkCompletionStatus::Pending);

                executor.Execute(); // Genuine deferred Task completion.
                const auto terminal=sink.TryFinalize();
                assert(terminal.Status==
                    L::AsyncSinkCompletionStatus::AcceptedAndReleased);
                assert(terminal.Delivery==L::RecordSinkSubmission::Accepted);
                assert(delivered.LastCompletion==
                    L::TypedEdlsTaskCompletion::DurablyAcknowledged);
                assert(delivered.LastAcknowledgement.AcknowledgedGeneration==g);
                assert(delivered.LastAcknowledgement.Status==
                    (g<3U?L::EdlsAcknowledgedStatus::AcknowledgedWithinCapacity:
                          L::EdlsAcknowledgedStatus::AcknowledgedAndRetired));
                assert(archive.Read().AcknowledgedGeneration==g);
                assert(archive.Read().AcknowledgedIncarnation==incarnation);
                assert(copyLease.IsEmpty() && !task.has_value());
                // Independent physical re-open: exactly last two genuine
                // header+EDLG CRC-qualified EDLS generations retained.
                Store reopened(folder,bytes);
                const auto history=L::InspectEdlsArchive<3U,1024U>(
                    reopened,Paths);
                assert(history.Condition==L::EdlsArchiveCondition::Unambiguous);
                assert(history.NextGeneration==g+1U);
                std::size_t complete=0U;
                for(const auto& slot:history.Slots) {
                    if(slot.Condition==L::EdlsSlotCondition::Clean) {
                        assert(slot.VerifiedFrames==1U);
                        assert(slot.Header.Generation<=g);
                        assert(slot.Header.Generation>=
                            (g>2U?g-1U:1U));
                        ++complete;
                    }
                }
                assert(complete==(g==1U?1U:2U));
            }
        };
        runBoot(oldBoot,7U,1U,8U);
        assert(oldBoot.Read().AcknowledgedGeneration==8U);
        assert(oldBoot.Read().AcknowledgedIncarnation==7U);

        // Real System identity increments incarnation on a new boot; both
        // g7/g8 remain protected while g9 is durably ACKed under incarnation8.
        StrictArchive newBoot(storage,ledger,Paths,AckKey.Value,
                              deviceBytes,8U);
        std::array<std::byte,1200U> rebootScratch{};
        assert(newBoot.Recover(rebootScratch).Status==
            L::EdlsAcknowledgedStatus::Ready);
        assert(newBoot.Read().AcknowledgedGeneration==8U);
        assert(newBoot.Read().AcknowledgedIncarnation==7U);
        runBoot(newBoot,8U,9U,16U);
        assert(newBoot.Read().AcknowledgedGeneration==16U);
        assert(newBoot.Read().AcknowledgedIncarnation==8U);

        // A complete EDLS successor is NOT an ACK if NVS StoreValue fails.
        // This Task terminal must yield ProviderFailure (never Accepted),
        // and both still-ACKed older physical generations remain intact.
        {
            Projection encoded{},decoded{};
            Scratch scratch{};
            std::array<std::byte,1200U> frame{},archiveScratch{};
            L::GeneratedLogRecordCborEncoder<
                Schema,L::AttemptSequenceWidth::Bits16,2U,12U,
                S::Identity::RuntimeIncarnationId,C::ClockEra
            > encoder(encoded,scratch);
            StrictDelivery delivered(newBoot,decoded,frame,archiveScratch,
                               Paths,deviceBytes,8U);
            L::AsyncCopyingTaskSink<
                L::EdlgCodec::Cbor,1024U,Pool,PortableBytes,
                DeferredTaskExecutor,StrictDelivery
            > sink(pool,copyLease,task,bytes,executor,delivered);
            ledger.failStore=true;
            Record source=make_record(device,17U,8U);
            const auto admitted=L::EncodeOnceAndSubmitToSinks<
                L::EdlgCodec::Cbor,1024U
            >(pool,sourceLease,source,encoder,sink);
            assert(admitted.Status==
                L::OwnedRepresentationStatus::SubmittedAndReleased);
            assert(newBoot.Read().AcknowledgedGeneration==16U);
            source=Record{};
            encoded=Projection{};
            executor.Execute();
            const auto failed=sink.TryFinalize();
            assert(failed.Status==
                L::AsyncSinkCompletionStatus::ProviderFailureAndReleased);
            assert(delivered.LastCompletion==
                L::TypedEdlsTaskCompletion::ArchiveFailure);
            assert(delivered.LastAcknowledgement.Status==
                L::EdlsAcknowledgedStatus::WatermarkStoreRejected);
            assert(newBoot.Read().AcknowledgedGeneration==16U);
            assert(!task.has_value() && copyLease.IsEmpty());
            ledger.failStore=false;
            std::array<std::byte,1200U> recoveryScratch{};
            assert(newBoot.Recover(recoveryScratch).Status==
                L::EdlsAcknowledgedStatus::UnacknowledgedCandidateRemoved);
            assert(newBoot.Read().AcknowledgedGeneration==16U);
        }
        runBoot(newBoot,8U,17U,17U); // retry accepts only after durable ACK17
        assert(newBoot.Read().AcknowledgedGeneration==17U);

        assert(memory.TearDown()==
            M::MemoryTopologyTeardownResult::Succeeded);
        std::size_t cleared=0U;
        for(std::size_t slot=0U;slot<Paths.size();++slot) {
            char name[256U]{};
            assert(std::snprintf(name,sizeof(name),"%s/slot%zu",
                folder,slot)>0);
            if(::unlink(name)==0)++cleared;
            else assert(errno==ENOENT);
        }
        assert(cleared==2U);
        assert(::rmdir(folder)==0);
    }
}
int main() {RunStrictTypedTask();}
