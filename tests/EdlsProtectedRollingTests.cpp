#include <logging/EdlgFrameWriter.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <logging/EdlsArchiveRotation.hpp>

namespace {
    namespace L=ESPressio::Logging;
    namespace P=ESPressio::Persistence;
    using Status=L::EdlsProtectedRollingStatus;

    struct Store final {
        std::array<std::array<std::byte,192U>,3U> bytes{};
        std::array<std::size_t,3U> length{};
        unsigned writes{0U}, removes{0U};
        bool failWrite{false}, torn{false}, failRemove{false}, crossDamage{false};
        bool damageProtectedAfterRemove{false};
        bool failSize{false};

        static std::size_t idx(P::FilePathView p) noexcept {
            return static_cast<std::size_t>(p.Data()[4U]-'0');
        }
        bool IsFileStorageReady() const noexcept { return true; }
        P::FileSizeResult GetFileSize(P::FilePathView p) const noexcept {
            if (failSize) {
                return {P::FileSizeStatus::IoFailure, {}};
            }
            const auto i=idx(p);
            return length[i] ? P::FileSizeResult{P::FileSizeStatus::Succeeded,P::StorageSize{length[i]}}:
                P::FileSizeResult{P::FileSizeStatus::NotFound,{}};
        }
        P::FileReadResult ReadFileAt(P::FilePathView p,P::StorageOffset o,P::DestinationBufferView d) const noexcept {
            const auto i=idx(p);
            if (!length[i])return {P::FileReadStatus::NotFound,0U,0U,{}};
            if (o.RawValue>length[i])return {P::FileReadStatus::InvalidOffset,0U,0U,{}};
            const auto available=length[i]-static_cast<std::size_t>(o.RawValue);
            const auto n=available<d.Capacity?available:d.Capacity;
            for(std::size_t k=0U;k<n;++k)static_cast<std::byte*>(d.Address)[k]=bytes[i][o.RawValue+k];
            return {P::FileReadStatus::Succeeded,static_cast<std::uint8_t>(P::ReadFact::AvailableDataSizeIsKnown),n,P::StorageSize{available}};
        }
        P::FileReplaceStatus ReplaceFile(P::FilePathView p,P::SourceBufferView s) noexcept {
            ++writes;
            if(failWrite)return P::FileReplaceStatus::IoFailure;
            const auto i=idx(p);
            assert(length[i]==0U); // Never overwrite an acknowledged generation.
            assert(s.Size<=bytes[i].size());
            const auto n=torn?s.Size/2U:s.Size;
            for(std::size_t k=0U;k<n;++k)bytes[i][k]=static_cast<const std::byte*>(s.Address)[k];
            length[i]=n;
            if(crossDamage)bytes[0U][38U]^=std::byte{0x80U};
            return P::FileReplaceStatus::Succeeded;
        }
        P::FileRemoveStatus RemoveFile(P::FilePathView p) noexcept {
            ++removes;
            if(failRemove)return P::FileRemoveStatus::IoFailure;
            const auto removed = idx(p);
            length[removed]=0U;
            // Inject corruption caused by a provider's real removal side
            // effect. The mandatory POST-delete CRC scan must catch it even
            // though pre-retirement inspection was unambiguously clean.
            if (damageProtectedAfterRemove) {
                for(std::size_t slot=0U;slot<3U;++slot) {
                    if(slot!=removed && length[slot]>38U) {
                        bytes[slot][38U]^=std::byte{0x80U};
                        break;
                    }
                }
            }
            return P::FileRemoveStatus::Succeeded;
        }
        void put(std::size_t slot,std::uint64_t gen) {
            L::EdlsHeader h{};
            h.Slot=static_cast<std::uint16_t>(slot);
            h.Generation=gen;
            h.RuntimeIncarnation=1U;
            h.DeviceIdentifier.fill(std::byte{0x11U});
            assert(L::WriteEdlsHeader(h,std::span<std::byte>(bytes[slot]))==L::EdlsHeaderStatus::Succeeded);
            length[slot]=L::EdlsHeaderBytes;
            constexpr std::array<std::byte,3U> payload{std::byte{0x82U},std::byte{1U},std::byte{2U}};
            std::size_t written=0U;
            assert(L::WriteEdlgFrame(L::EdlgCodec::Cbor,payload,
                std::span<std::byte>(bytes[slot]).subspan(length[slot]),written,16U,
                ESPressio::Platform::Portable::Memory::ByteOperationsProvider{})==L::EdlgFrameWriteResult::Succeeded);
            length[slot]+=written;
        }
    };

    constexpr auto p0=P::FilePathView::Validate("slot0");
    constexpr auto p1=P::FilePathView::Validate("slot1");
    constexpr auto p2=P::FilePathView::Validate("slot2");
    static_assert(p0.IsValuePresent&&p1.IsValuePresent&&p2.IsValuePresent);
    constexpr std::array<P::FilePathView,3U> paths{p0.Value,p1.Value,p2.Value};
    void VerifyPredecessorOutcomes() {
        using Result = L::Detail::EdlsPredecessorVerificationResult;
        Store store{};
        store.put(0U, 1U);
        std::array<std::byte, 36U> scratch{};
        const auto before = L::InspectEdlsSlotWithScratch<16U>(
            store,
            paths[0U],
            0U,
            scratch);
        const auto verify = [&] {
            return L::Detail::VerifyEdlsPredecessorWithScratch<16U>(
                store,
                paths[0U],
                0U,
                before,
                scratch);
        };

        assert(verify() == Result::Verified);
        assert(L::Detail::VerifyEdlsPredecessor<16U>(
            store,
            paths[0U],
            0U,
            L::EdlsSlotInspection{}) == Result::InvalidBaseline);

        store.failSize = true;
        assert(verify() == Result::ProviderFailure);
        store.failSize = false;
        store.bytes[0U][38U] ^= std::byte{0x80U};
        assert(verify() == Result::ReadbackNotClean);

        store.put(0U, 2U);
        assert(verify() == Result::HeaderChanged);

        store.put(0U, 1U);
        const auto firstEnd = store.length[0U];
        const auto frameBytes = firstEnd - L::EdlsHeaderBytes;
        for (std::size_t offset = 0U; offset < frameBytes; ++offset) {
            store.bytes[0U][firstEnd + offset] =
                store.bytes[0U][L::EdlsHeaderBytes + offset];
        }
        store.length[0U] += frameBytes;
        assert(verify() == Result::VerifiedPrefixChanged);
    }

    std::array<std::byte,16U> id=[] {
        std::array<std::byte,16U> result{};
        result.fill(std::byte{0x11U});
        return result;
    }();
    auto publish(Store& s,std::uint32_t incarnation=1U) {
        std::array<std::byte,48U> b{};
        constexpr std::array<std::byte,3U> payload{std::byte{0x82U},std::byte{1U},std::byte{2U}};
        std::size_t n=0U;
        assert(L::WriteEdlgFrame(L::EdlgCodec::Cbor,payload,b,n,16U,
        ESPressio::Platform::Portable::Memory::ByteOperationsProvider{})==L::EdlgFrameWriteResult::Succeeded);
        return L::PublishEdlsProtectedRollingSuccessor<3U,2U,16U,128U>(
            s,paths,id,incarnation,std::span<const std::byte>(b.data(),n));
    }
    auto finish(Store& s) {
        return L::FinishEdlsProtectedRollingRetirement<3U,2U,16U>(s,paths);
    }
    void mainTests() {
        Store s{};
        assert(publish(s).Status==Status::PublishedWithinCapacity);
        assert(publish(s,2U).Status==Status::PublishedWithinCapacity);
        for(std::uint32_t g=3U;g<=42U;++g) {
            const auto prev=L::InspectEdlsArchive<3U,16U>(s,paths);
            assert(prev.NextGeneration==g);
            const auto latest=prev.NewestSlot;
            const auto persisted=prev.Slots[latest].Header.Generation;
            auto o=publish(s,g);
            assert(o.Status==Status::PublishedAndRetired&&o.Generation==g);
            assert(o.NativeRemove==P::FileRemoveStatus::Succeeded);
            assert(s.length[o.RetiredSlot]==0U);
            const auto next=L::InspectEdlsArchive<3U,16U>(s,paths);
            assert(next.Condition==L::EdlsArchiveCondition::Unambiguous&&next.NextGeneration==g+1U);
            assert(next.Slots[latest].Header.Generation==persisted);
            assert(next.Slots[latest].Condition==L::EdlsSlotCondition::Clean);
        }
        assert(s.writes==42U&&s.removes==40U);
        assert(finish(s).Status==Status::NoPendingRetirement);

        Store interrupted{};
        assert(publish(interrupted).Status==Status::PublishedWithinCapacity);
        assert(publish(interrupted).Status==Status::PublishedWithinCapacity);
        interrupted.failRemove=true;
        auto o=publish(interrupted);
        assert(o.Status==Status::RetirementRejected&&o.NativeRemove==P::FileRemoveStatus::IoFailure);
        const auto priorWrites=interrupted.writes;
        assert(publish(interrupted).Status==Status::RecoveryRequired);
        assert(interrupted.writes==priorWrites);
        interrupted.failRemove=false;
        assert(finish(interrupted).Status==Status::RecoveredPendingRetirement);
        assert(publish(interrupted,4U).Status==Status::PublishedAndRetired);

        Store refused{};
        assert(publish(refused).Status==Status::PublishedWithinCapacity);
        assert(publish(refused).Status==Status::PublishedWithinCapacity);
        const auto older=refused.bytes[0U], newer=refused.bytes[1U];
        refused.failWrite=true;
        o=publish(refused);
        assert(o.Status==Status::ProviderRejected&&refused.removes==0U);
        assert(refused.bytes[0U]==older&&refused.bytes[1U]==newer);
        refused.failWrite=false;
        refused.torn=true;
        o=publish(refused);
        assert(o.Status==Status::ReadbackUnconfirmed&&refused.removes==0U);
        refused.torn=false;
        const auto before=refused.writes;
        assert(publish(refused).Status==Status::ArchiveAmbiguous);
        assert(finish(refused).Status==Status::ArchiveAmbiguous);
        assert(refused.writes==before&&refused.removes==0U);

        Store noncontiguous{};
        noncontiguous.put(0U,1U);
        noncontiguous.put(1U,3U);
        assert(publish(noncontiguous).Status==Status::NoncontiguousHistory);
        assert(noncontiguous.removes==0U&&noncontiguous.writes==0U);
        noncontiguous.put(1U,2U);
        noncontiguous.bytes[1U][36U]^=std::byte{0x80U};
        assert(publish(noncontiguous).Status==Status::ArchiveAmbiguous);
        noncontiguous.bytes[1U][36U]^=std::byte{0x80U};
        noncontiguous.put(1U,UINT64_MAX);
        assert(publish(noncontiguous).Status==Status::GenerationExhausted);

        Store damagedAfterRemoval{};
        assert(publish(damagedAfterRemoval).Status==Status::PublishedWithinCapacity);
        assert(publish(damagedAfterRemoval).Status==Status::PublishedWithinCapacity);
        damagedAfterRemoval.damageProtectedAfterRemove=true;
        auto afterDamage=publish(damagedAfterRemoval);
        assert(afterDamage.Status==Status::RetirementUnconfirmed);
        assert(damagedAfterRemoval.removes==1U);
        assert(finish(damagedAfterRemoval).Status==Status::ArchiveAmbiguous);

        Store damaged{};
        assert(publish(damaged).Status==Status::PublishedWithinCapacity);
        assert(publish(damaged).Status==Status::PublishedWithinCapacity);
        damaged.crossDamage=true;
        assert(publish(damaged).Status==Status::ProtectedHistoryChanged);
        assert(damaged.removes==0U);
    }
}
int main() {
    VerifyPredecessorOutcomes();
    mainTests();
}
