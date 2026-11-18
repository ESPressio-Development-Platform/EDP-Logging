#include <logging/EdlgFrameWriter.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <logging/EdlsArchiveInspection.hpp>
#include <logging/EdlsPowerLossAcknowledgement.hpp>
#include <logging/PowerLossEdlsSnapshots.hpp>
#include "StrictAckKeyValueFake.hpp"
#include <logging/EdlsArchiveAppend.hpp>
#include <logging/EdlsArchiveSuccessor.hpp>
#include <logging/EdlsArchiveRotation.hpp>
namespace {
namespace L=ESPressio::Logging;
namespace P=ESPressio::Persistence;
struct Storage {
 std::array<std::array<std::byte,128>,3> bytes{};
 std::array<std::size_t,3> length{};
 bool fault=false;
 bool failAppend=false;
 bool skewSize=false;
 bool failReplace=false;
 bool tornReplace=false;
 bool badReadback=false;
 std::span<const std::byte> substituteFrame{};
 unsigned replaceCount=0U;
 mutable unsigned sizeQueries=0;
 /// Test-only physical-read witnesses and explicit large-read faults.
 mutable unsigned readCalls=0U;
 bool failWholeRead=false;
 bool shortWholeRead=false;
 std::size_t bulkFaultSlot=3U;
 unsigned removeCount=0U;
 static std::size_t index(P::FilePathView p){return static_cast<std::size_t>(p.Data()[4]-'0');}
 P::FileSizeResult GetFileSize(P::FilePathView p) const noexcept {
  if(fault)return {P::FileSizeStatus::IoFailure,{}};
  auto i=index(p);
  ++sizeQueries;
  const auto reported=length[i]+(skewSize&&sizeQueries>1?1U:0U);
  return length[i]?P::FileSizeResult{P::FileSizeStatus::Succeeded,P::StorageSize{reported}}:
   P::FileSizeResult{P::FileSizeStatus::NotFound,{}};
 }
 P::FileReadResult ReadFileAt(P::FilePathView p,P::StorageOffset o,P::DestinationBufferView d) const noexcept {
  ++readCalls;
  if(fault)return {P::FileReadStatus::IoFailure,0U,0U,{}};
  const auto i=index(p);
  const bool bulk=o.RawValue==0U && d.Capacity>40U &&
      (bulkFaultSlot==3U || bulkFaultSlot==i);
  if(failWholeRead && bulk)return {P::FileReadStatus::IoFailure,0U,0U,{}};
  if(o.RawValue>length[i])return {P::FileReadStatus::InvalidOffset,0U,0U,{}};
  auto n=length[i]-static_cast<std::size_t>(o.RawValue);
  const auto a=n;
  if(n>d.Capacity)n=d.Capacity;
  if(shortWholeRead && bulk && n>0U)--n;
  for(std::size_t k=0;k<n;++k)
   static_cast<std::byte*>(d.Address)[k]=bytes[i][o.RawValue+k];
  return {P::FileReadStatus::Succeeded,
   static_cast<std::uint8_t>(P::ReadFact::AvailableDataSizeIsKnown),n,P::StorageSize{a}};
 }
 bool IsFileStorageReady() const noexcept { return true; }
 P::FileRemoveStatus RemoveFile(P::FilePathView p) noexcept {
  ++removeCount;length[index(p)]=0U;return P::FileRemoveStatus::Succeeded;
 }
 P::FileReplaceStatus ReplaceFile(P::FilePathView p,P::SourceBufferView src) noexcept {
  ++replaceCount;
  if(failReplace)return P::FileReplaceStatus::IoFailure;
  const auto i=index(p);
  if(src.Size>bytes[i].size())return P::FileReplaceStatus::FileTooLarge;
  const auto n=tornReplace ? src.Size/2U : src.Size;
  for(std::size_t k=0U;k<n;++k)
   bytes[i][k]=static_cast<const std::byte*>(src.Address)[k];
  length[i]=n;
  if (!substituteFrame.empty()) {
   auto segment = std::span{bytes[i]}.first(n);
   auto frame = segment.subspan(L::EdlsHeaderBytes, substituteFrame.size());
   for (std::size_t k = 0U; k < substituteFrame.size(); ++k) {
    frame[k] = substituteFrame[k];
   }
   if (segment.size() == L::EdlsHeaderBytes + substituteFrame.size() + 16U) {
    const auto covered = segment.first(segment.size() - 4U);
    L::Detail::WriteEdlsLe(
     segment,
     covered.size(),
     L::Detail::EdlgFrameCrc32C(covered),
     4U
    );
   }
  }
  if(badReadback && n>=20U)bytes[i][16U]^=std::byte{1};
  return P::FileReplaceStatus::Succeeded;
 }
 P::FileAppendStatus AppendFile(P::FilePathView p,P::SourceBufferView src) noexcept {
  if(failAppend)return P::FileAppendStatus::IoFailure;
  const auto i=index(p);
  if(length[i]+src.Size>bytes[i].size())return P::FileAppendStatus::NoSpace;
  for(std::size_t k=0U;k<src.Size;++k)
   bytes[i][length[i]+k]=static_cast<const std::byte*>(src.Address)[k];
  length[i]+=src.Size;
  return P::FileAppendStatus::Succeeded;
 }
 void header(std::size_t i,std::uint64_t generation,std::byte id=std::byte{1}) {
  L::EdlsHeader h{};
  h.Slot=static_cast<std::uint16_t>(i);
  h.Generation=generation;
  h.RuntimeIncarnation=1U;
  h.DeviceIdentifier.fill(id);
  assert(L::WriteEdlsHeader(h,std::span<std::byte>(bytes[i]).first(40U))==
    L::EdlsHeaderStatus::Succeeded);
  length[i]=40U;
 }
 void frame(std::size_t i) {
  const std::array<std::byte,3U> msg{std::byte{1},std::byte{2},std::byte{3}};
  std::size_t n=0;
  assert(L::WriteEdlgFrame(L::EdlgCodec::Cbor,msg,
    std::span<std::byte>(bytes[i]).subspan(length[i]),n,16U,
    ESPressio::Platform::Portable::Memory::ByteOperationsProvider{})==
    L::EdlgFrameWriteResult::Succeeded);
  length[i]+=n;
 }

};

    /// Independent documented EDLC layout around three owner-framed CBOR records.
    void MakeSealedThree(Storage& store) {
        store.header(0U, 7U, std::byte{0xA6U});
        L::EdlsHeader header{};
        assert(L::ReadEdlsHeader(
            std::span<const std::byte>(store.bytes[0U]).first(40U), header
        ) == L::EdlsHeaderStatus::Succeeded);
        header.Profile = L::EdlsContentProfile::SealedFrames;
        assert(L::WriteEdlsHeader(header, store.bytes[0U]) ==
            L::EdlsHeaderStatus::Succeeded);
        const std::array<std::byte, 3U> payload{
            std::byte{0x82U}, std::byte{0x01U}, std::byte{0x02U}
        };
        for (std::size_t index = 0U; index < 3U; ++index) {
            std::size_t framed = 0U;
            assert(L::WriteEdlgFrame(
                L::EdlgCodec::Cbor, payload,
                std::span<std::byte>(store.bytes[0U]).subspan(store.length[0U]),
                framed, 16U,
                ESPressio::Platform::Portable::Memory::ByteOperationsProvider{}
            ) == L::EdlgFrameWriteResult::Succeeded);
            store.length[0U] += framed;
        }
        const auto sealOffset = store.length[0U];
        auto seal = std::span<std::byte>(store.bytes[0U]).subspan(sealOffset, 16U);
        seal[0U] = std::byte{'E'};
        seal[1U] = std::byte{'D'};
        seal[2U] = std::byte{'L'};
        seal[3U] = std::byte{'C'};
        L::Detail::WriteEdlsLe(seal, 4U, 3U, 4U);
        L::Detail::WriteEdlsLe(seal, 8U, sealOffset - 40U, 4U);
        const auto checksum = L::Detail::EdlgFrameCrc32C(
            std::span<const std::byte>(store.bytes[0U]).first(sealOffset + 12U)
        );
        L::Detail::WriteEdlsLe(seal, 12U, checksum, 4U);
        store.length[0U] += 16U;
        assert(store.length[0U] == 125U);
    }

    /// One complete read and bounded streaming must reject the same corruption.
    void WholeAndStreamingCorruptionParity() {
        constexpr auto path = P::FilePathView::Validate("slot0");
        Storage store{};
        MakeSealedThree(store);
        std::array<std::byte, 128U> scratch{};
        const auto inspect = [&](std::size_t capacity) {
            store.readCalls = 0U;
            return L::InspectEdlsSlotWithScratch<16U>(
                store, path.Value, 0U, std::span<std::byte>(scratch).first(capacity)
            );
        };
        auto captured = inspect(128U);
        const auto capturedReads = store.readCalls;
        auto streamed = inspect(36U);
        assert(captured.Condition == L::EdlsSlotCondition::Clean);
        assert(streamed.Condition == L::EdlsSlotCondition::Clean);
        assert(captured.VerifiedFrames == 3U && streamed.VerifiedFrames == 3U);
        assert(captured.VerifiedPrefixBytes == 125U && streamed.VerifiedPrefixBytes == 125U);
        assert(capturedReads == 1U && store.readCalls > capturedReads);

        const auto original = store.bytes[0U];
        for (std::size_t offset = 0U; offset < store.length[0U]; ++offset) {
            for (unsigned bit = 0U; bit < 8U; ++bit) {
                store.bytes[0U] = original;
                store.bytes[0U][offset] ^= static_cast<std::byte>(1U << bit);
                captured = inspect(128U);
                streamed = inspect(36U);
                assert(captured.Condition != L::EdlsSlotCondition::Clean);
                assert(captured.Condition == streamed.Condition);
                assert(captured.HasHeader == streamed.HasHeader);
                assert(captured.VerifiedFrames == streamed.VerifiedFrames);
                assert(captured.VerifiedPrefixBytes == streamed.VerifiedPrefixBytes);
                if (captured.HasHeader) {
                    assert(captured.Header.DeviceIdentifier == streamed.Header.DeviceIdentifier);
                    assert(captured.Header.Generation == streamed.Header.Generation);
                    assert(captured.Header.Slot == streamed.Header.Slot);
                    assert(captured.Header.Profile == streamed.Header.Profile);
                }
            }
        }
        store.bytes[0U] = original;
        store.failWholeRead = true;
        captured = inspect(128U);
        assert(captured.Condition == L::EdlsSlotCondition::ProviderFailure);
        assert(captured.HasHeader && captured.Header.Generation == 7U);
        assert(captured.VerifiedFrames == 0U);
        assert(inspect(36U).Condition == L::EdlsSlotCondition::Clean);
        store.failWholeRead = false;
        store.shortWholeRead = true;
        captured = inspect(128U);
        assert(captured.Condition == L::EdlsSlotCondition::ProviderFailure);
        assert(captured.HasHeader && captured.Header.Profile == L::EdlsContentProfile::SealedFrames);
        assert(captured.VerifiedFrames == 0U);
        assert(store.bytes[0U] == original && store.removeCount == 0U);
    }

    /// Failed bulk I/O must never turn a readable foreign segment into removable unknown data.
    void BulkFailureCannotAuthoriseForeignRemoval() {
        using Ledger = L::StrictAckTests::TestAckKeyValueStorage;
        using Archive = L::PowerLossAcknowledgedEdls<3U, 2U, 16U, 128U, Storage, Ledger>;
        constexpr auto p0 = P::FilePathView::Validate("slot0");
        constexpr auto p1 = P::FilePathView::Validate("slot1");
        constexpr auto p2 = P::FilePathView::Validate("slot2");
        constexpr auto key = P::KeyView::Validate("a");
        const std::array<P::FilePathView, 3U> paths{p0.Value, p1.Value, p2.Value};
        std::array<std::byte, 16U> identity{};
        identity.fill(std::byte{0xA6U});
        Storage store{};
        Ledger ledger{};
        Archive archive(store, ledger, paths, key.Value, identity, 1U);
        assert(archive.InitializeEmpty().Status == L::EdlsAcknowledgedStatus::Initialized);
        std::array<std::byte, 23U> frame{};
        const std::array<std::byte, 3U> payload{
            std::byte{0x82U}, std::byte{0x01U}, std::byte{0x02U}
        };
        std::size_t framed = 0U;
        assert(L::WriteEdlgFrame(
            L::EdlgCodec::Cbor, payload, frame, framed, 16U,
            ESPressio::Platform::Portable::Memory::ByteOperationsProvider{}
        ) == L::EdlgFrameWriteResult::Succeeded);
        std::array<std::byte, 128U> scratch{};
        assert(archive.Publish(frame, scratch).AcknowledgedGeneration == 1U);
        assert(archive.Publish(frame, scratch).AcknowledgedGeneration == 2U);
        store.header(2U, 999U, std::byte{0xB7U});
        assert(store.AppendFile(
            p2.Value, P::SourceBufferView{frame.data(), frame.size()}
        ) == P::FileAppendStatus::Succeeded);
        const auto before = store.bytes;
        const auto sizes = store.length;
        const auto stores = ledger.stores;
        store.bulkFaultSlot = 2U;
        for (unsigned kind = 0U; kind < 2U; ++kind) {
            store.failWholeRead = kind == 0U;
            store.shortWholeRead = kind == 1U;
            const auto inspected = L::InspectEdlsSlotWithScratch<16U>(
                store, p2.Value, 2U, scratch
            );
            assert(inspected.Condition == L::EdlsSlotCondition::ProviderFailure);
            assert(inspected.HasHeader && inspected.Header.Generation == 999U);
            assert(inspected.Header.DeviceIdentifier[0U] == std::byte{0xB7U});
            assert(archive.Recover(scratch).Status ==
                L::EdlsAcknowledgedStatus::ArchiveAmbiguous);
            assert(store.removeCount == 0U && store.bytes == before && store.length == sizes);
            assert(ledger.stores == stores && archive.Read().AcknowledgedGeneration == 2U);
        }
    }


    /// Rule 2 and Rule 19: numeric states and exact-read operational outcomes.
    void ExactReadResultContractTests() {
        static_assert(static_cast<std::uint8_t>(L::EdlsSlotCondition::Empty) == 0U);
        static_assert(static_cast<std::uint8_t>(L::EdlsSlotCondition::ProviderFailure) == 5U);
        static_assert(static_cast<std::uint8_t>(L::EdlsArchiveCondition::GenerationExhausted) == 2U);
        constexpr auto path = P::FilePathView::Validate("slot0");
        Storage store{};
        store.header(0U, 11U);
        std::array<std::byte, 40U> exact{};
        assert(L::Detail::ReadExactEdlsBytes(
            store, path.Value, 0U, exact
        ) == L::Detail::EdlsExactReadResult::Succeeded);
        std::array<std::byte, 41U> tooLong{};
        assert(L::Detail::ReadExactEdlsBytes(
            store, path.Value, 0U, tooLong
        ) == L::Detail::EdlsExactReadResult::IncompleteRead);
        store.fault = true;
        assert(L::Detail::ReadExactEdlsBytes(
            store, path.Value, 0U, exact
        ) == L::Detail::EdlsExactReadResult::ProviderFailure);
    }

}
int main(){
    static_assert(static_cast<std::uint8_t>(L::EdlsSuccessorStatus::ReadbackUnconfirmed) == 7U);
    static_assert(static_cast<std::uint8_t>(L::EdlsAppendStatus::ProviderRejected) == 5U);
    static_assert(static_cast<std::uint8_t>(L::DurableEdlsStatus::Committed) == 10U);
    ExactReadResultContractTests();
    WholeAndStreamingCorruptionParity();
    BulkFailureCannotAuthoriseForeignRemoval();
 namespace L=ESPressio::Logging;
 namespace P=ESPressio::Persistence;
 constexpr auto p0=P::FilePathView::Validate("slot0");
 constexpr auto p1=P::FilePathView::Validate("slot1");
 static_assert(p0.IsValuePresent&&p1.IsValuePresent);
 const std::array<P::FilePathView,2> paths{p0.Value,p1.Value};
 Storage store{};
 auto r=L::InspectEdlsArchive<2U,16U>(store,paths);
 assert(r.Condition==L::EdlsArchiveCondition::Unambiguous&&r.NextGeneration==1);
 store.header(0,7);store.frame(0);
 store.header(1,8);store.frame(1);
 r=L::InspectEdlsArchive<2U,16U>(store,paths);
 assert(r.Condition==L::EdlsArchiveCondition::Unambiguous&&r.NextGeneration==9);
 assert(r.NewestSlot==1&&r.Slots[0].VerifiedFrames==1);
 store.bytes[1][store.length[1]++]=std::byte{0xFF};
 r=L::InspectEdlsArchive<2U,16U>(store,paths);
 assert(r.Slots[1].Condition==L::EdlsSlotCondition::DamagedTail);
 assert(r.Slots[1].VerifiedFrames==1&&r.Slots[1].VerifiedPrefixBytes==63);
 store.header(0,8);
 r=L::InspectEdlsArchive<2U,16U>(store,paths);
 assert(r.Condition==L::EdlsArchiveCondition::Ambiguous&&r.NextGeneration==0);
 store.header(0,7,std::byte{2});
 r=L::InspectEdlsArchive<2U,16U>(store,paths);
 assert(r.Condition==L::EdlsArchiveCondition::Ambiguous);
 store.header(0,7);
 store.header(1,UINT64_MAX);
 r=L::InspectEdlsArchive<2U,16U>(store,paths);
 assert(r.Condition==L::EdlsArchiveCondition::GenerationExhausted);
 store.header(1,8);
 store.bytes[0][36]^=std::byte{1};
 r=L::InspectEdlsArchive<2U,16U>(store,paths);
 assert(r.Condition==L::EdlsArchiveCondition::Ambiguous);
 assert(r.Slots[0].Condition==L::EdlsSlotCondition::InvalidHeader);
 store.header(0,7);store.fault=true;
 r=L::InspectEdlsArchive<2U,16U>(store,paths);
 assert(r.Condition==L::EdlsArchiveCondition::Ambiguous);
 store.fault=false;
 Storage journal{};
 journal.header(0,1);
 const std::array<std::byte,3U> msg{std::byte{1},std::byte{2},std::byte{3}};
 std::array<std::byte,40U> encoded{};
 std::size_t n=0;
 assert(L::WriteEdlgFrame(L::EdlgCodec::Cbor,msg,encoded,n,16U,
 ESPressio::Platform::Portable::Memory::ByteOperationsProvider{})==
        L::EdlgFrameWriteResult::Succeeded);
 auto exact=std::span<const std::byte>(encoded.data(),n);
 auto posted=L::AppendEdlgToCleanEdlsSlot<16U,80U>(journal,p0.Value,0U,exact);
 assert(posted.Status==L::EdlsAppendStatus::ProviderAccepted&&
        posted.Native==P::FileAppendStatus::Succeeded&&posted.OfferedBytes==n);
 auto inspected=L::InspectEdlsSlot<16U>(journal,p0.Value,0U);
 assert(inspected.VerifiedFrames==1U&&inspected.VerifiedPrefixBytes==40U+n);
 journal.failAppend=true;
 posted=L::AppendEdlgToCleanEdlsSlot<16U,128U>(journal,p0.Value,0U,exact);
 assert(posted.Status==L::EdlsAppendStatus::ProviderRejected&&
        posted.Native==P::FileAppendStatus::IoFailure);
 journal.failAppend=false;
 posted=L::AppendEdlgToCleanEdlsSlot<16U,80U>(journal,p0.Value,0U,exact);
 assert(posted.Status==L::EdlsAppendStatus::SegmentCapacityExceeded);
 encoded[16U]^=std::byte{1U};
 posted=L::AppendEdlgToCleanEdlsSlot<16U,128U>(journal,p0.Value,0U,exact);
 assert(posted.Status==L::EdlsAppendStatus::InvalidFrame);
 encoded[16U]^=std::byte{1U};
 journal.bytes[0][42U]^=std::byte{1U};
 posted=L::AppendEdlgToCleanEdlsSlot<16U,128U>(journal,p0.Value,0U,exact);
 assert(posted.Status==L::EdlsAppendStatus::SegmentUnclean);
 journal.header(0,1);
 journal.skewSize=true;journal.sizeQueries=0;
 posted=L::AppendEdlgToCleanEdlsSlot<16U,128U>(journal,p0.Value,0U,exact);
 assert(posted.Status==L::EdlsAppendStatus::ChangedDuringPreflight);

 // Newly published successor is never permitted to overwrite a prior slot.
 Storage successor{};
 std::array<std::byte,16U> identity{};
 identity.fill(std::byte{1U});
 std::array<std::byte,128U> aliasedScratch{};
 ESPressio::Platform::Portable::Memory::ByteOperationsProvider{}.CopyBytes(
     aliasedScratch.data(),
     exact.data(),
     exact.size()
 );
 const auto originalScratch = aliasedScratch;
 const auto aliased = L::PublishEdlsSuccessorIntoEmptySlotWithScratch<2U,16U,128U>(
     successor,
     paths,
     identity,
     1U,
     std::span<const std::byte>(aliasedScratch).first(exact.size()),
     aliasedScratch
 );
 assert(aliased.Status == L::EdlsSuccessorStatus::InvalidIdentityOrFrame);
 assert(successor.replaceCount == 0U && aliasedScratch == originalScratch);

 auto publish=L::PublishEdlsSuccessorIntoEmptySlot<2U,16U,128U>(
  successor,paths,identity,1U,exact);
 assert(publish.Status==L::EdlsSuccessorStatus::PublishedAndVerified);
 assert(publish.Native==P::FileReplaceStatus::Succeeded &&
        publish.Slot==0U && publish.Generation==1U);
 assert(successor.replaceCount==1U && successor.length[0]==40U+n);
 auto before0=successor.bytes[0];
 publish=L::PublishEdlsSuccessorIntoEmptySlot<2U,16U,128U>(
  successor,paths,identity,2U,exact);
 assert(publish.Status==L::EdlsSuccessorStatus::PublishedAndVerified &&
        publish.Slot==1U && publish.Generation==2U);
 assert(successor.bytes[0]==before0);
 assert(successor.replaceCount==2U);
 publish=L::PublishEdlsSuccessorIntoEmptySlot<2U,16U,128U>(
  successor,paths,identity,3U,exact);
 assert(publish.Status==L::EdlsSuccessorStatus::NoEmptySlot &&
        successor.replaceCount==2U);
 auto invalidIdentity=identity;
 invalidIdentity.fill(std::byte{3U});
 publish=L::PublishEdlsSuccessorIntoEmptySlot<2U,16U,128U>(
  successor,paths,invalidIdentity,4U,exact);
 assert(publish.Status==L::EdlsSuccessorStatus::InvalidIdentityOrFrame);
 Storage deniedSuccessor{};
 deniedSuccessor.failReplace=true;
 publish=L::PublishEdlsSuccessorIntoEmptySlot<2U,16U,128U>(
  deniedSuccessor,paths,identity,1U,exact);
 assert(publish.Status==L::EdlsSuccessorStatus::ProviderRejected &&
        publish.Native==P::FileReplaceStatus::IoFailure &&
        deniedSuccessor.length[0]==0U);
 Storage tornSuccessor{};
 tornSuccessor.tornReplace=true;
 publish=L::PublishEdlsSuccessorIntoEmptySlot<2U,16U,128U>(
  tornSuccessor,paths,identity,1U,exact);
 assert(publish.Status==L::EdlsSuccessorStatus::ReadbackUnconfirmed);
 assert(tornSuccessor.length[0]>0U);
 auto ambiguous=L::InspectEdlsArchive<2U,16U>(tornSuccessor,paths);
 assert(ambiguous.Condition==L::EdlsArchiveCondition::Ambiguous);
 Storage corruptSuccessor{};
 corruptSuccessor.badReadback=true;
 publish=L::PublishEdlsSuccessorIntoEmptySlot<2U,16U,128U>(
  corruptSuccessor,paths,identity,1U,exact);
 assert(publish.Status==L::EdlsSuccessorStatus::ReadbackUnconfirmed);
 Storage oldArchive{};
 oldArchive.header(0U,7U);oldArchive.frame(0U);
 publish=L::PublishEdlsSuccessorIntoEmptySlot<2U,16U,128U>(
  oldArchive,paths,identity,2U,exact);
 assert(publish.Status==L::EdlsSuccessorStatus::PublishedAndVerified &&
        publish.Slot==1U && publish.Generation==8U);
 Storage badHeader{};
 badHeader.header(0U,7U);
 badHeader.bytes[0U][36U]^=std::byte{1U};
 publish=L::PublishEdlsSuccessorIntoEmptySlot<2U,16U,128U>(
  badHeader,paths,identity,2U,exact);
 assert(publish.Status==L::EdlsSuccessorStatus::ArchiveAmbiguous &&
        badHeader.replaceCount==0U);
 Storage exhausted{};
 exhausted.header(0U,UINT64_MAX);
 publish=L::PublishEdlsSuccessorIntoEmptySlot<2U,16U,128U>(
  exhausted,paths,identity,2U,exact);
 assert(publish.Status==L::EdlsSuccessorStatus::ArchiveAmbiguous &&
        exhausted.replaceCount==0U);

 // Strict latest-protected, bounded oldest-eligible rotation is an explicit
 // opt-in over the original no-overwrite successor. No external power-loss
 // guarantee is inferred from any native ReplaceFile success.
 Storage rotating{};
 auto advanced=L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
  rotating,paths,identity,1U,exact);
 assert(advanced.Status==L::EdlsRollingStatus::PublishedInEmptySlot);
 assert(advanced.Slot==0U && advanced.Generation==1U);
 assert(advanced.ProtectedNewestSlot==2U && !advanced.PriorNewestVerified);
 auto initialSlot0=rotating.bytes[0U];
 advanced=L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
  rotating,paths,identity,2U,exact);
 assert(advanced.Status==L::EdlsRollingStatus::PublishedInEmptySlot &&
        advanced.Slot==1U && advanced.Generation==2U &&
        advanced.ProtectedNewestSlot==0U && advanced.PriorNewestVerified);
 assert(rotating.bytes[0U]==initialSlot0);
 const auto originalNewest=rotating.bytes[1U];
 advanced=L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
  rotating,paths,identity,3U,exact);
 assert(advanced.Status==L::EdlsRollingStatus::PublishedAfterOldestReclaim);
 assert(advanced.Slot==0U && advanced.ProtectedNewestSlot==1U &&
        advanced.Generation==3U && advanced.PriorNewestVerified);
 assert(rotating.bytes[1U]==originalNewest);
 auto all=L::InspectEdlsArchive<2U,16U>(rotating,paths);
 assert(all.Condition==L::EdlsArchiveCondition::Unambiguous);
 assert(all.NewestSlot==0U && all.NextGeneration==4U);
 assert(all.Slots[1U].Header.Generation==2U &&
        all.Slots[1U].Condition==L::EdlsSlotCondition::Clean);
 for (std::uint32_t next=4U; next<=64U; ++next) {
  const auto prior=all.NewestSlot;
  const auto retained=rotating.bytes[prior];
  advanced=L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
   rotating,paths,identity,next,exact);
  assert(advanced.Status==L::EdlsRollingStatus::PublishedAfterOldestReclaim);
  assert(advanced.Slot!=prior && advanced.ProtectedNewestSlot==prior &&
         advanced.PriorNewestVerified && advanced.Generation==next);
  assert(rotating.bytes[prior]==retained);
  all=L::InspectEdlsArchive<2U,16U>(rotating,paths);
  assert(all.Condition==L::EdlsArchiveCondition::Unambiguous &&
         all.NextGeneration==next+1U &&
         all.NewestSlot==advanced.Slot);
 }
 // Even though all slots are full, a mismatching identity or bad CRC
 // cannot cause ANY destructive mutation.
 auto beforeCount=rotating.replaceCount;
 auto wrong=identity;
 wrong.fill(std::byte{4U});
 advanced=L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
  rotating,paths,wrong,1U,exact);
 assert(advanced.Status==L::EdlsRollingStatus::InvalidIdentityOrFrame);
 assert(rotating.replaceCount==beforeCount);
 encoded[16U]^=std::byte{0x01U};
 advanced=L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
  rotating,paths,identity,1U,exact);
 assert(advanced.Status==L::EdlsRollingStatus::InvalidIdentityOrFrame);
 assert(rotating.replaceCount==beforeCount);
 encoded[16U]^=std::byte{0x01U};

 // Provider acceptance and valid CRCs do not establish byte-exact readback.
 std::array<std::byte, 128U> substituted{};
 const std::array replacementPayload{std::byte{9U}, std::byte{8U}, std::byte{7U}};
 std::size_t replacementBytes = 0U;
 assert(L::WriteEdlgFrame(
  L::EdlgCodec::Cbor,
  replacementPayload,
  substituted,
  replacementBytes,
  16U,
  ESPressio::Platform::Portable::Memory::ByteOperationsProvider{}
 ) == L::EdlgFrameWriteResult::Succeeded);
 assert(replacementBytes == exact.size());
 const auto differentFrame = std::span{substituted}.first(replacementBytes);

 Storage substitutedSingle{};
 substitutedSingle.substituteFrame = differentFrame;
 const auto single = L::PublishEdlsSuccessorIntoEmptySlot<2U, 16U, 128U>(
  substitutedSingle,
  paths,
  identity,
  1U,
  exact
 );
 assert(single.Status == L::EdlsSuccessorStatus::ReadbackUnconfirmed &&
        single.Native == P::FileReplaceStatus::Succeeded);
 const auto singleReadback = L::InspectEdlsSlot<16U>(
  substitutedSingle,
  paths[0U],
  0U
 );
 assert(singleReadback.Condition == L::EdlsSlotCondition::Clean &&
        singleReadback.VerifiedFrames == 1U);

 Storage substitutedSuccessor{};
 substitutedSuccessor.substituteFrame = differentFrame;
 std::array<std::byte, 128U> sealedScratch{};
 const auto sealed = L::PublishEdlsSuccessorIntoEmptySlotWithScratch<
  2U, 16U, 128U, Storage, 2U>(
   substitutedSuccessor,
   paths,
   identity,
   1U,
   exact,
   sealedScratch
 );
 assert(sealed.Status == L::EdlsSuccessorStatus::ReadbackUnconfirmed &&
        sealed.Native == P::FileReplaceStatus::Succeeded);
 const auto sealedReadback = L::InspectEdlsSlot<16U>(
  substitutedSuccessor,
  paths[0U],
  0U
 );
 assert(sealedReadback.Condition == L::EdlsSlotCondition::Clean &&
        sealedReadback.VerifiedFrames == 1U);

 auto substitutedRotation = rotating;
 substitutedRotation.substituteFrame = differentFrame;
 const auto substitutedOutcome = L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
  substitutedRotation,
  paths,
  identity,
  65U,
  exact
 );
 assert(substitutedOutcome.Status == L::EdlsRollingStatus::ReadbackUnconfirmed &&
        substitutedOutcome.Native == P::FileReplaceStatus::Succeeded &&
        substitutedOutcome.PriorNewestVerified);
 const auto rotatedReadback = L::InspectEdlsSlot<16U>(
  substitutedRotation,
  paths[substitutedOutcome.Slot],
  static_cast<std::uint16_t>(substitutedOutcome.Slot)
 );
 assert(rotatedReadback.Condition == L::EdlsSlotCondition::Clean &&
        rotatedReadback.VerifiedFrames == 1U);

 // An actual provider refusal preserves the prior verified newest slot
 // and never invents a successful rotation/durability acknowledgement.
 const auto newestBeforeFailure=all.NewestSlot;
 const auto intactNewest=rotating.bytes[newestBeforeFailure];
 rotating.failReplace=true;
 advanced=L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
  rotating,paths,identity,65U,exact);
 assert(advanced.Status==L::EdlsRollingStatus::ProviderRejected &&
        advanced.Native==P::FileReplaceStatus::IoFailure &&
        advanced.PriorNewestVerified &&
        advanced.ProtectedNewestSlot==newestBeforeFailure);
 assert(rotating.bytes[newestBeforeFailure]==intactNewest);
 rotating.failReplace=false;

 // A partially written replacement is unconfirmed even though the
 // separate previously newest generation was CRC-valid after mutation.
 rotating.tornReplace=true;
 advanced=L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
  rotating,paths,identity,65U,exact);
 assert(advanced.Status==L::EdlsRollingStatus::ReadbackUnconfirmed &&
        advanced.PriorNewestVerified);
 assert(rotating.bytes[newestBeforeFailure]==intactNewest);
 // The damaged old slot demands explicit recovery; no automatic
 // destructive retry when archive history is now ambiguous.
 rotating.tornReplace=false;
 beforeCount=rotating.replaceCount;
 advanced=L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
  rotating,paths,identity,65U,exact);
 assert(advanced.Status==L::EdlsRollingStatus::ArchiveAmbiguous &&
        rotating.replaceCount==beforeCount);

 Storage exhaustedRotation{};
 exhaustedRotation.header(0U,UINT64_MAX);
 advanced=L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
  exhaustedRotation,paths,identity,1U,exact);
 assert(advanced.Status==L::EdlsRollingStatus::ArchiveAmbiguous &&
        exhaustedRotation.replaceCount==0U);
 Storage corruptRotation{};
 corruptRotation.header(0U,1U);corruptRotation.frame(0U);
 corruptRotation.header(1U,2U);corruptRotation.frame(1U);
 corruptRotation.badReadback=true;
 advanced=L::PublishEdlsOldestEligibleSuccessor<2U,16U,128U>(
  corruptRotation,paths,identity,3U,exact);
 assert(advanced.Status==L::EdlsRollingStatus::ReadbackUnconfirmed &&
        advanced.PriorNewestVerified);

}
