#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <logging/EdlsArchiveInspection.hpp>
#include <logging/EdlsArchiveAppend.hpp>
#include <logging/EdlsArchiveSuccessor.hpp>
#include <logging/EdlsArchiveRotation.hpp>
namespace {
namespace L=ESPressio::Logging;
namespace P=ESPressio::Persistence;
struct Storage {
 std::array<std::array<std::byte,128>,2> bytes{};
 std::array<std::size_t,2> length{};
 bool fault=false;
 bool failAppend=false;
 bool skewSize=false;
 bool failReplace=false;
 bool tornReplace=false;
 bool badReadback=false;
 unsigned replaceCount=0U;
 mutable unsigned sizeQueries=0;
 static std::size_t index(P::FilePathView p){return p.Data()[4]=='0'?0U:1U;}
 P::FileSizeResult GetFileSize(P::FilePathView p) const noexcept {
  if(fault)return {P::FileSizeStatus::IoFailure,{}};
  auto i=index(p);
  ++sizeQueries;
  const auto reported=length[i]+(skewSize&&sizeQueries>1?1U:0U);
  return length[i]?P::FileSizeResult{P::FileSizeStatus::Succeeded,P::StorageSize{reported}}:
   P::FileSizeResult{P::FileSizeStatus::NotFound,{}};
 }
 P::FileReadResult ReadFileAt(P::FilePathView p,P::StorageOffset o,P::DestinationBufferView d) const noexcept {
  if(fault)return {P::FileReadStatus::IoFailure,0U,0U,{}};
  const auto i=index(p);
  if(o.RawValue>length[i])return {P::FileReadStatus::InvalidOffset,0U,0U,{}};
  auto n=length[i]-static_cast<std::size_t>(o.RawValue);
  const auto a=n;
  if(n>d.Capacity)n=d.Capacity;
  for(std::size_t k=0;k<n;++k)
   static_cast<std::byte*>(d.Address)[k]=bytes[i][o.RawValue+k];
  return {P::FileReadStatus::Succeeded,
   static_cast<std::uint8_t>(P::ReadFact::AvailableDataSizeIsKnown),n,P::StorageSize{a}};
 }
 bool IsFileStorageReady() const noexcept { return true; }
 P::FileRemoveStatus RemoveFile(P::FilePathView p) noexcept {
  length[index(p)]=0U;return P::FileRemoveStatus::Succeeded;
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
    std::span<std::byte>(bytes[i]).subspan(length[i]),n,16U)==
    L::EdlgFrameWriteResult::Succeeded);
  length[i]+=n;
 }
};
}
int main(){
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
 assert(L::WriteEdlgFrame(L::EdlgCodec::Cbor,msg,encoded,n,16U)==
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
