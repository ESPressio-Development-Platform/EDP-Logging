#include <array>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <unistd.h>

#include <memory/ByteOperationsProvider.hpp>
#include <VfsFileStorage.hpp>
#include <logging/EdlsArchiveRotation.hpp>

namespace {

    namespace L=ESPressio::Logging;
    namespace P=ESPressio::Persistence;
    namespace M=ESPressio::Platform::Portable::Memory;

    struct Binding final {};

    using Profile=P::EspIdf::VfsBindingProfile<
        P::RetentionLevel::Restart,
        P::TextCaseSensitivity::CaseSensitive,
        P::MediaRemovability::Fixed,
        64U,64U,4096U
    >;

    using Owner=P::EspIdf::VfsFileStorage<
        Binding,Profile,M::ByteOperationsProvider
    >;

    constexpr auto P0=P::FilePathView::Validate("slot0");
    constexpr auto P1=P::FilePathView::Validate("slot1");
    constexpr auto P2=P::FilePathView::Validate("slot2");

    constexpr std::array<P::FilePathView,3U> Paths{
        P0.Value,P1.Value,P2.Value
    };

    void Exercise() {
        char directory[]="/tmp/edp-edls-immutable-XXXXXX";
        const auto folder=::mkdtemp(directory);
        assert(folder!=nullptr);

        M::ByteOperationsProvider bytes{};
        std::array<std::byte,16U> identity{};
        identity.fill(std::byte{0x5AU});

        std::array<std::byte,64U> encoded{};
        constexpr std::array<std::byte,3U> payload{
            std::byte{0x82U},std::byte{0x01U},std::byte{0x02U}
        };
        std::size_t used=0U;
        assert(L::WriteEdlgFrame(
            L::EdlgCodec::Cbor,payload,encoded,used,16U
        )==L::EdlgFrameWriteResult::Succeeded);
        const auto frame=std::span<const std::byte>(encoded.data(),used);

        for (std::uint32_t generation=1U;generation<=24U;++generation) {
            Owner writer(folder,bytes);
            assert(writer.IsFileStorageReady());
            const auto posted=L::PublishEdlsProtectedRollingSuccessor<
                3U,2U,16U,256U
            >(writer,Paths,identity,generation,frame);

            if (generation<3U) {
                assert(posted.Status==
                    L::EdlsProtectedRollingStatus::PublishedWithinCapacity);
            } else {
                assert(posted.Status==
                    L::EdlsProtectedRollingStatus::PublishedAndRetired);
                assert(posted.NativeRemove==P::FileRemoveStatus::Succeeded);
            }

            // Independent provider instance reopens genuine filesystem paths.
            Owner reopened(folder,bytes);
            const auto state=L::InspectEdlsArchive<3U,16U>(reopened,Paths);
            assert(state.Condition==L::EdlsArchiveCondition::Unambiguous);
            assert(state.NextGeneration==generation+1U);
            std::size_t count=0U;
            for (const auto& slot:state.Slots) {
                if (slot.Condition!=L::EdlsSlotCondition::Clean) {
                    continue;
                }
                ++count;
                assert(slot.VerifiedFrames==1U);
                assert(slot.Header.Generation<=generation);
                if(generation>2U) {
                    assert(slot.Header.Generation>=generation-1U);
                }
            }
            assert(count==(generation==1U?1U:2U));
        }

        // Separate process-like replay: 2 retained, 1 physically free.
        Owner finalRead(folder,bytes);
        auto observed=L::InspectEdlsArchive<3U,16U>(finalRead,Paths);
        assert(observed.NextGeneration==25U);

        // Create a complete-but-unretired successor through the existing
        // immutable-only owner. Simulate a reset between commit and reclaim.
        auto pending=L::PublishEdlsSuccessorIntoEmptySlot<
            3U,16U,256U
        >(finalRead,Paths,identity,25U,frame);
        assert(pending.Status==L::EdlsSuccessorStatus::PublishedAndVerified);
        const auto blocked=L::PublishEdlsProtectedRollingSuccessor<
            3U,2U,16U,256U
        >(finalRead,Paths,identity,26U,frame);
        assert(blocked.Status==
            L::EdlsProtectedRollingStatus::RecoveryRequired);

        Owner rebooted(folder,bytes);
        const auto completed=L::FinishEdlsProtectedRollingRetirement<
            3U,2U,16U
        >(rebooted,Paths);
        assert(completed.Status==
            L::EdlsProtectedRollingStatus::RecoveredPendingRetirement);
        observed=L::InspectEdlsArchive<3U,16U>(rebooted,Paths);
        assert(observed.NextGeneration==26U);

        // A zero-length file in the next physical free name is NOT a
        // proven-empty successor reserve; refuse instead of replacing it.
        for (std::size_t slot=0U;slot<3U;++slot) {
            if (observed.Slots[slot].Condition!=L::EdlsSlotCondition::Empty) {
                continue;
            }
            char name[200U]{};
            assert(std::snprintf(name,sizeof(name),"%s/slot%zu",folder,slot)>0);
            auto* zero=std::fopen(name,"wb");
            assert(zero!=nullptr);
            assert(std::fclose(zero)==0);
            const auto denied=L::PublishEdlsProtectedRollingSuccessor<
                3U,2U,16U,256U
            >(rebooted,Paths,identity,26U,frame);
            assert(denied.Status==
                L::EdlsProtectedRollingStatus::ArchiveAmbiguous);
            assert(::unlink(name)==0);
            break;
        }

        for (std::size_t slot=0U;slot<3U;++slot) {
            char name[200U]{};
            assert(std::snprintf(name,sizeof(name),"%s/slot%zu",folder,slot)>0);
            const auto result=::unlink(name);
            assert(result==0 || errno==ENOENT);
        }
        assert(::rmdir(folder)==0);
    }

    void ExerciseLargeCallerScratch() {
        using LargeProfile=P::EspIdf::VfsBindingProfile<
            P::RetentionLevel::Restart,P::TextCaseSensitivity::CaseSensitive,
            P::MediaRemovability::Fixed,64U,64U,8192U>;
        using LargeStore=P::EspIdf::VfsFileStorage<
            Binding,LargeProfile,M::ByteOperationsProvider>;
        char directory[]="/tmp/edp-edls-large-scratch-XXXXXX";
        const auto folder=::mkdtemp(directory);
        assert(folder!=nullptr);
        M::ByteOperationsProvider ops{};
        std::array<std::byte,16U> identity{};
        identity.fill(std::byte{0xA6U});
        static std::array<std::byte,4096U> cbor{};
        static std::array<std::byte,4116U> frame{};
        static std::array<std::byte,4156U> archiveScratch{};
        cbor.fill(std::byte{0xA5U});
        cbor[0U]=std::byte{0x59U};
        cbor[1U]=std::byte{0x0FU};
        cbor[2U]=std::byte{0xFDU};
        std::size_t length=0U;
        assert(L::WriteEdlgFrame(
            L::EdlgCodec::Cbor,cbor,frame,length,4096U)==
            L::EdlgFrameWriteResult::Succeeded);
        assert(length==4116U);
        for(std::uint32_t generation=1U;generation<=5U;++generation) {
            LargeStore writer(folder,ops);
            const auto posted=L::PublishEdlsProtectedRollingSuccessorWithScratch<
                3U,2U,4096U,4200U
            >(writer,Paths,identity,generation,
              std::span<const std::byte>(frame.data(),length),archiveScratch);
            assert(posted.Status==(generation<=2U
                ? L::EdlsProtectedRollingStatus::PublishedWithinCapacity
                : L::EdlsProtectedRollingStatus::PublishedAndRetired));
            assert(posted.Generation==generation);
            LargeStore reopened(folder,ops);
            const auto state=L::InspectEdlsArchiveWithScratch<
                3U,4096U
            >(reopened,Paths,archiveScratch);
            assert(state.Condition==L::EdlsArchiveCondition::Unambiguous);
            assert(state.NextGeneration==generation+1U);
            std::size_t occupied=0U;
            for(const auto& slot:state.Slots) {
                if(slot.Condition!=L::EdlsSlotCondition::Clean) continue;
                assert(slot.VerifiedFrames==1U);
                assert(slot.VerifiedPrefixBytes==4156U);
                ++occupied;
            }
            assert(occupied==(generation==1U?1U:2U));
        }
        // Reusing the same physical scratch did not destroy prior records.
        LargeStore finalStore(folder,ops);
        const auto stopped=L::FinishEdlsProtectedRollingRetirementWithScratch<
            3U,2U,4096U>(finalStore,Paths,archiveScratch);
        assert(stopped.Status==
            L::EdlsProtectedRollingStatus::NoPendingRetirement);
        for(std::size_t i=0U;i<3U;++i) {
            char name[200U]{};
            assert(std::snprintf(name,sizeof(name),"%s/slot%zu",folder,i)>0);
            const auto result=::unlink(name);
            assert(result==0 || errno==ENOENT);
        }
        assert(::rmdir(folder)==0);
    }

} // namespace

int main() {
    Exercise();
    ExerciseLargeCallerScratch();
}
