#include <array>
#include <cassert>
#include <cstddef>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main ExistingPowerLossTestsMain
#include "PowerLossEdlsSnapshotsTests.cpp"
#undef main
#pragma GCC diagnostic pop

#include <VfsFileStorage.hpp>
#include <memory/ByteOperationsProvider.hpp>
#include <logging/EdlsPowerLossAcknowledgement.hpp>

namespace {
    namespace Logging=ESPressio::Logging;
    namespace Persist=ESPressio::Persistence;
    namespace Portable=ESPressio::Platform::Portable::Memory;
    struct AckBinding final {};
    using Profile=Persist::EspIdf::VfsBindingProfile<
        Persist::RetentionLevel::Restart,
        Persist::TextCaseSensitivity::CaseSensitive,
        Persist::MediaRemovability::Fixed,64U,64U,4096U>;
    using Vfs=Persist::EspIdf::VfsFileStorage<
        AckBinding,Profile,Portable::ByteOperationsProvider>;
    constexpr auto path0=Persist::FilePathView::Validate("slot0");
    constexpr auto path1=Persist::FilePathView::Validate("slot1");
    constexpr auto path2=Persist::FilePathView::Validate("slot2");
    constexpr auto ledgerKey=Persist::KeyView::Validate("a");
    static_assert(path0.IsValuePresent && path1.IsValuePresent &&
        path2.IsValuePresent && ledgerKey.IsValuePresent);
    const std::array<Persist::FilePathView,3U> paths{
        path0.Value,path1.Value,path2.Value};
    using Strict=Logging::PowerLossAcknowledgedEdls<
        3U,2U,16U,128U,Vfs,FakeStorage>;
    using S=Logging::EdlsAcknowledgedStatus;

    void RunStrictAckTests() {
        char directory[]="/tmp/edls-strict-ack-XXXXXX";
        auto* root=::mkdtemp(directory);
        assert(root!=nullptr);
        Portable::ByteOperationsProvider bytes{};
        Vfs files(root,bytes);
        assert(files.IsFileStorageReady());
        FakeStorage ledger{};
        std::array<std::byte,16U> id{};
        id.fill(std::byte{0x5AU});
        Strict writer(files,ledger,paths,ledgerKey.Value,id,7U);
        std::array<std::byte,64U> buffer{};
        std::array<std::byte,128U> scratch{};
        constexpr std::array<std::byte,3U> payload{
            std::byte{0x82U},std::byte{0x01U},std::byte{0x02U}};
        std::size_t used=0U;
        assert(Logging::WriteEdlgFrame(Logging::EdlgCodec::Cbor,
            payload,buffer,used,16U)==Logging::EdlgFrameWriteResult::Succeeded);
        const auto frame=std::span<const std::byte>(buffer.data(),used);

        assert(writer.Read().Status==S::MissingWatermark);
        assert(writer.InitializeEmpty().Status==S::Initialized);
        assert(writer.Read().AcknowledgedGeneration==0U);
        assert(writer.Publish(frame,scratch).Status==S::AcknowledgedWithinCapacity);
        assert(writer.Read().AcknowledgedGeneration==1U);
        assert(writer.Publish(frame,scratch).Status==S::AcknowledgedWithinCapacity);
        assert(writer.Read().AcknowledgedGeneration==2U);
        assert(files.ReplaceFile(path2.Value,Persist::SourceBufferView{
            nullptr,0U})==Persist::FileReplaceStatus::Succeeded);
        assert(writer.Recover(scratch).Status==S::UnacknowledgedCandidateRemoved);
        assert(files.GetFileSize(path2.Value).Status==Persist::FileSizeStatus::NotFound);
        assert(writer.Read().AcknowledgedGeneration==2U);
        const auto third=writer.Publish(frame,scratch);
        assert(third.Status==S::AcknowledgedAndRetired);
        assert(third.AcknowledgedGeneration==3U);
        assert(writer.Read().AcknowledgedGeneration==3U);
        assert(files.GetFileSize(path0.Value).Status==Persist::FileSizeStatus::NotFound);

        // A failed durable ACK must NEVER retire an older acknowledged file.
        ledger.failStore=true;
        const auto failed=writer.Publish(frame,scratch);
        assert(failed.Status==S::WatermarkStoreRejected);
        assert(writer.Read().AcknowledgedGeneration==3U);
        assert(files.GetFileSize(path1.Value).Status==Persist::FileSizeStatus::Succeeded);
        assert(files.GetFileSize(path2.Value).Status==Persist::FileSizeStatus::Succeeded);
        ledger.failStore=false;
        assert(writer.Recover(scratch).Status==S::UnacknowledgedCandidateRemoved);
        const auto fourth=writer.Publish(frame,scratch);
        assert(fourth.Status==S::AcknowledgedAndRetired);
        assert(fourth.AcknowledgedGeneration==4U);
        assert(writer.Read().AcknowledgedGeneration==4U);

        ledger.failRead=true;
        const auto priorWrites=files.GetFileSize(path0.Value);
        assert(writer.Recover(scratch).Status==S::ProviderFailure);
        assert(files.GetFileSize(path0.Value).Status==priorWrites.Status);
        ledger.failRead=false;

        // SystemIdentity provides a NEW durable incarnation per boot;
        // the old ACK ledger and EDLS history remain valid across boots.
        Strict rebooted(files,ledger,paths,ledgerKey.Value,id,8U);
        const auto previousBootAck=rebooted.Read();
        assert(previousBootAck.Status==S::Ready &&
            previousBootAck.AcknowledgedGeneration==4U &&
            previousBootAck.AcknowledgedIncarnation==7U);
        assert(rebooted.Recover(scratch).Status==S::Ready);
        const auto fifth=rebooted.Publish(frame,scratch);
        assert(fifth.Status==S::AcknowledgedAndRetired &&
            fifth.AcknowledgedGeneration==5U);
        const auto currentBootAck=rebooted.Read();
        assert(currentBootAck.Status==S::Ready &&
            currentBootAck.AcknowledgedGeneration==5U &&
            currentBootAck.AcknowledgedIncarnation==8U);
        assert(rebooted.Recover(scratch).Status==S::Ready);

        // Even a checksum-valid replayed ledger with a falsely changed
        // incumbent incarnation must NOT authorize deleting any file.
        auto ledgerCopy=ledger.bytes[0U];
        Logging::EdlsHeader fraudulent{};
        fraudulent.DeviceIdentifier=id;
        fraudulent.RuntimeIncarnation=7U;
        fraudulent.Generation=6U;
        fraudulent.Slot=0U;
        assert(Logging::WriteEdlsHeader(fraudulent,
            std::span<std::byte>(ledger.bytes[0U]))==
            Logging::EdlsHeaderStatus::Succeeded);
        assert(rebooted.Recover(scratch).Status==S::InvalidWatermark);
        ledger.bytes[0U]=ledgerCopy;
        assert(rebooted.Recover(scratch).Status==S::Ready);

        // Corrupt an acknowledged physical segment: never repair a spare
        // or retire any older record with protected CRC corruption present.
        std::array<std::byte,128U> saved{};
        const auto read=files.ReadFileAt(path0.Value,Persist::StorageOffset{},
            Persist::DestinationBufferView{saved.data(),saved.size()});
        assert(read.Status==Persist::FileReadStatus::Succeeded &&
            read.BytesTransferred==63U);
        const std::byte original=saved[0U];
        const std::byte bad=original^std::byte{0x10U};
        assert(files.WriteFileAt(path0.Value,Persist::StorageOffset{},
            Persist::SourceBufferView{&bad,1U})==
            Persist::FileWriteAtStatus::Succeeded);
        assert(writer.Recover(scratch).Status==S::ArchiveAmbiguous);
        assert(files.WriteFileAt(path0.Value,Persist::StorageOffset{},
            Persist::SourceBufferView{&original,1U})==
            Persist::FileWriteAtStatus::Succeeded);
        assert(writer.Recover(scratch).Status==S::Ready);

        // Without a PowerLoss watermark, legacy EDLS contents are NOT
        // retroactively assigned durable ACK provenance.
        FakeStorage orphanLedger{};
        Strict orphan(files,orphanLedger,paths,ledgerKey.Value,id,7U);
        assert(orphan.InitializeEmpty().Status==S::ArchiveAmbiguous);
        assert(orphanLedger.stores==0U);

        // A separate reader reopens the same physical filesystem and must
        // observe only the two most recent ACKed generations.
        Vfs reopened(root,bytes);
        const auto state=Logging::InspectEdlsArchive<3U,16U>(
            reopened,paths);
        assert(state.Condition==Logging::EdlsArchiveCondition::Unambiguous);
        assert(state.NextGeneration==6U);
        std::size_t clean=0U;
        for(const auto& slot:state.Slots) {
            if(slot.Condition==Logging::EdlsSlotCondition::Clean) {
                assert(slot.Header.Generation>=4U &&
                    slot.Header.Generation<=5U);
                ++clean;
            }
        }
        assert(clean==2U);

        // Explicit cleanup belongs exclusively to this test fixture.
        for(std::size_t i=0U;i<3U;++i) {
            char name[256U]{};
            assert(std::snprintf(name,sizeof(name),"%s/slot%zu",
                root,i)>0);
            const auto removed=::unlink(name);
            assert(removed==0 || errno==ENOENT);
        }
        assert(::rmdir(root)==0);
    }
}
int main() {
    RunStrictAckTests();
}
