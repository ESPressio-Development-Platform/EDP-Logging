#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <logging/EdlsArchiveInspection.hpp>
#include <logging/EdlsArchiveAppend.hpp>
namespace {
namespace L=ESPressio::Logging;
namespace P=ESPressio::Persistence;
struct Storage {
 std::array<std::array<std::byte,128>,2> bytes{};
 std::array<std::size_t,2> length{};
 bool fault=false;
 bool failAppend=false;
 bool skewSize=false;
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
}
