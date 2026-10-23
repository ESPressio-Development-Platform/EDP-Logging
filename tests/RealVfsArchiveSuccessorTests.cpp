#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <unistd.h>

#include <memory/ByteOperationsProvider.hpp>
#include <VfsFileStorage.hpp>
#include <logging/EdlsArchiveSuccessor.hpp>
#include <logging/EdlsArchiveRotation.hpp>

namespace {
namespace L=ESPressio::Logging;
namespace P=ESPressio::Persistence;
namespace Portable=ESPressio::Platform::Portable::Memory;
struct LabBinding final {};
using Profile=P::EspIdf::VfsBindingProfile<
 P::RetentionLevel::Restart,
 P::TextCaseSensitivity::CaseSensitive,
 P::MediaRemovability::Fixed,64U,64U,4096U>;
using Store=P::EspIdf::VfsFileStorage<LabBinding,Profile,Portable::ByteOperationsProvider>;

void Run() {
 char directory[]="/tmp/edp-logging-vfs-XXXXXX";
 const auto root=mkdtemp(directory);
 assert(root!=nullptr);
 Portable::ByteOperationsProvider bytes;
 Store owner(root,bytes);
 assert(owner.IsFileStorageReady());
 constexpr auto p0=P::FilePathView::Validate("slot0");
 constexpr auto p1=P::FilePathView::Validate("slot1");
 static_assert(p0.IsValuePresent && p1.IsValuePresent);
 const std::array<P::FilePathView,2U> paths{p0.Value,p1.Value};
 std::array<std::byte,16U> id{};
 id.fill(std::byte{0x5AU});
 std::array<std::byte,48U> wire{};
 constexpr std::array<std::byte,4U> cbor{std::byte{0x83U},
     std::byte{0x01U},std::byte{0x02U},std::byte{0x03U}};
 std::size_t length=0U;
 assert(L::WriteEdlgFrame(L::EdlgCodec::Cbor,cbor,wire,length,32U)==
  L::EdlgFrameWriteResult::Succeeded);
 auto encoded=std::span<const std::byte>(wire.data(),length);

 auto first=L::PublishEdlsSuccessorIntoEmptySlot<2U,32U,512U>(
  owner,paths,id,1U,encoded);
 assert(first.Status==L::EdlsSuccessorStatus::PublishedAndVerified);
 assert(first.Generation==1U&&first.Slot==0U);
 // Completely new provider object and native file handle to prove
 // independent persistence readback on the actual host filesystem.
 Store reopened(root,bytes);
 auto observation=L::InspectEdlsArchive<2U,32U>(reopened,paths);
 assert(observation.Condition==L::EdlsArchiveCondition::Unambiguous &&
        observation.NextGeneration==2U &&
        observation.Slots[0].VerifiedFrames==1U);
 auto second=L::PublishEdlsSuccessorIntoEmptySlot<2U,32U,512U>(
  reopened,paths,id,2U,encoded);
 assert(second.Status==L::EdlsSuccessorStatus::PublishedAndVerified &&
        second.Generation==2U && second.Slot==1U);
 auto full=L::PublishEdlsSuccessorIntoEmptySlot<2U,32U,512U>(
  reopened,paths,id,3U,encoded);
 assert(full.Status==L::EdlsSuccessorStatus::NoEmptySlot);
 // The optional approved oldest-eligible rolling path may now reclaim
 // ONLY the clean oldest slot; each existing newest generation survives.
 for(std::uint32_t next=3U;next<=12U;++next) {
  const auto before=L::InspectEdlsArchive<2U,32U>(reopened,paths);
  const auto protectedSlot=before.NewestSlot;
  const auto retainedGeneration=before.Slots[protectedSlot].Header.Generation;
  const auto published=L::PublishEdlsOldestEligibleSuccessor<2U,32U,512U>(
   reopened,paths,id,next,encoded);
  assert(published.Status==L::EdlsRollingStatus::PublishedAfterOldestReclaim &&
         published.Generation==next && published.PriorNewestVerified &&
         published.Slot!=protectedSlot);
  // A brand-new native provider owner re-opens both actual OS files.
  Store independentlyReopened(root,bytes);
  const auto after=L::InspectEdlsArchive<2U,32U>(
   independentlyReopened,paths);
  assert(after.Condition==L::EdlsArchiveCondition::Unambiguous &&
         after.NewestSlot==published.Slot &&
         after.NextGeneration==next+1U &&
         after.Slots[protectedSlot].Condition==L::EdlsSlotCondition::Clean &&
         after.Slots[protectedSlot].Header.Generation==retainedGeneration);
 }
 observation=L::InspectEdlsArchive<2U,32U>(reopened,paths);
 assert(observation.Slots[0].VerifiedFrames==1U &&
        observation.Slots[1].VerifiedFrames==1U);
 // Inject a genuine OS-file damaged tail independently of the provider
 // to emulate a partially written/garbled frame without any power cut.
 char file1[256]{};
 assert(std::snprintf(file1,sizeof(file1),"%s/slot1",root)>0);
 auto* corrupt=std::fopen(file1,"r+b");
 assert(corrupt!=nullptr && std::fseek(corrupt,56,SEEK_SET)==0);
 const unsigned char tamper=0xAE;
 assert(std::fwrite(&tamper,1U,1U,corrupt)==1U);
 assert(std::fflush(corrupt)==0);
 assert(std::fclose(corrupt)==0);
 observation=L::InspectEdlsArchive<2U,32U>(reopened,paths);
 assert(observation.Slots[1].Condition==L::EdlsSlotCondition::DamagedTail);
 assert(observation.Slots[0].Condition==L::EdlsSlotCondition::Clean);
 // Recovery MUST NOT automatically erase the damaged older slot.
 const auto blocked=L::PublishEdlsOldestEligibleSuccessor<2U,32U,512U>(
  reopened,paths,id,13U,encoded);
 assert(blocked.Status==L::EdlsRollingStatus::ArchiveAmbiguous);
 const auto priorGuard=L::PublishEdlsSuccessorIntoEmptySlot<2U,32U,512U>(
  reopened,paths,id,13U,encoded);
 assert(priorGuard.Status==L::EdlsSuccessorStatus::NoEmptySlot);
 char file0[256]{};
 assert(std::snprintf(file0,sizeof(file0),"%s/slot0",root)>0);
 assert(::unlink(file0)==0 && ::unlink(file1)==0);
 assert(::rmdir(root)==0);
}
} // namespace
int main() { Run(); }
