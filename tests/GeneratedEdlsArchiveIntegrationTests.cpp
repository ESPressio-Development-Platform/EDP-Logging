#include <array>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <unistd.h>
#include <GeneratedLocalisationContract.hpp>
#include <VfsFileStorage.hpp>
#include <memory/ByteOperationsProvider.hpp>
#include <logging/DiagnosticClockEraArguments.hpp>
#include <logging/LogRecordSourceAssembly.hpp>
#include <logging/GeneratedLogRecordEdlsArchive.hpp>
#include <logging/GeneratedEdlsTaskAdapters.hpp>
#include <logging/EdlgFrameReader.hpp>

namespace L=ESPressio::Logging;
namespace P=ESPressio::Persistence;
namespace S=ESPressio::System;
namespace C=ESPressio::Clock;
namespace Serial=ESPressio::Serialisation;
namespace M=ESPressio::Platform::Portable::Memory;
using FixtureMessage=Fixture::Diagnostic::DiagnosticMessageSchema<1U,0U,5U>;
using Source=L::BasicLogRecord<L::AttemptSequenceWidth::Bits16,2U,12U>;
using Projection=L::GeneratedLogRecordWireProjection<
    L::AttemptSequenceWidth::Bits16,
    S::Identity::RuntimeIncarnationId,C::ClockEra>;
using Scratch=L::OwnedPositionalScratch<
    S::Identity::RuntimeIncarnationId,C::ClockEra>;

struct TestBinding final {};
using Profile=P::EspIdf::VfsBindingProfile<
    P::RetentionLevel::Restart,P::TextCaseSensitivity::CaseSensitive,
    P::MediaRemovability::Fixed,64U,64U,4096U>;
using Storage=P::EspIdf::VfsFileStorage<TestBinding,Profile,M::ByteOperationsProvider>;

constexpr auto P0=P::FilePathView::Validate("slot0");
constexpr auto P1=P::FilePathView::Validate("slot1");
constexpr auto P2=P::FilePathView::Validate("slot2");
constexpr std::array<P::FilePathView,3U> Paths{P0.Value,P1.Value,P2.Value};

int main() {
    char directory[]="/tmp/edp-typed-edls-XXXXXX";
    const auto folder=::mkdtemp(directory);
    assert(folder!=nullptr);
    M::ByteOperationsProvider bytes{};
    S::Identity::DeviceIdentifier::Storage deviceBytes{};
    deviceBytes.fill(0xA6U);
    const S::Identity::DeviceIdentifier device{deviceBytes};
    std::array<std::byte,16U> identity{};
    identity.fill(std::byte{0xA6U});
    const L::QualifiedLogTime time{
        C::MonotonicTimestamp::FromNanoseconds(123456U),
        C::SynchronizedTimestamp{},
        C::SynchronizationUncertainty::Maximum(),
        C::ClockEra{},
        C::SynchronizationState::NeverSynchronized,
        L::LogTemporalEvidenceStatus::Unavailable
    };
    const auto attempt=L::AttemptSequence<
        L::AttemptSequenceWidth::Bits16>::FromValidatedValue(27U);
    Source record{};
    assert((L::AssembleLogRecordSource<FixtureMessage>(
        record,L::Severity::Info,device,
        S::Identity::RuntimeIncarnationId{7U},attempt,time,
        S::Identity::RuntimeIncarnationId{0x01020304U},
        C::ClockEra::FromValue(0x0102030405060708ULL)
    )==L::LogRecordAssemblyStatus::Prepared));

    Projection projection{};
    Scratch scratch{};
    std::array<std::byte,2048U> cbor{};
    std::array<std::byte,2200U> frame{};
    std::array<std::byte,2200U> archiveScratch{};
    const auto submit=[&](Storage& storage, const Source& source,
                          std::span<std::byte> cborOutput,
                          const std::array<std::byte,16U>& id,
                          std::uint32_t incarnation) {
        return L::PublishGeneratedLogRecordToProtectedEdlsWithScratch<
            FixtureMessage,2U,2048U,2200U
        >(source,projection,scratch,cborOutput,frame,storage,
          Paths,id,incarnation,archiveScratch);
    };

    for(std::uint32_t gen=1U;gen<=12U;++gen) {
        Storage storage(folder,bytes);
        assert(storage.IsFileStorageReady());
        const auto outcome=submit(storage,record,cbor,identity,7U);
        assert(outcome.IsPublished());
        assert(outcome.CborBytes>0U);
        assert(outcome.EdlgBytes==outcome.CborBytes+20U);
        assert(outcome.Archive.Generation==gen);
        const auto expected=gen<=2U
            ? L::EdlsProtectedRollingStatus::PublishedWithinCapacity
            : L::EdlsProtectedRollingStatus::PublishedAndRetired;
        assert(outcome.Archive.Status==expected);
        // Fresh provider instance, no shared storage object, no implied ACK.
        Storage reopened(folder,bytes);
        const auto scanned=L::InspectEdlsArchive<3U,2048U>(reopened,Paths);
        assert(scanned.Condition==L::EdlsArchiveCondition::Unambiguous);
        assert(scanned.NextGeneration==gen+1U);
        std::size_t retained=0U;
        for(const auto& slot:scanned.Slots)
            if(slot.Condition==L::EdlsSlotCondition::Clean) ++retained;
        assert(retained==(gen==1U?1U:2U));
        // Independent readback validates the fully typed fifteen FieldId CBOR
        // envelope, rather than trusting just EDLS/EDLG outer CRC checks.
        std::array<std::byte,2300U> persisted{};
        const auto last=Paths[scanned.NewestSlot];
        const auto size=reopened.GetFileSize(last);
        assert(size.Status==P::FileSizeStatus::Succeeded);
        assert(size.Size.RawValue <= persisted.size());
        const auto recovered=reopened.ReadFileAt(last,P::StorageOffset{},
            P::DestinationBufferView{persisted.data(),persisted.size()});
        assert(recovered.Status==P::FileReadStatus::Succeeded);
        assert(recovered.BytesTransferred==size.Size.RawValue);
        L::EdlgFrameView inspected{};
        assert(L::ReadEdlgFrame(
            std::span<const std::byte>(persisted.data()+L::EdlsHeaderBytes,
                recovered.BytesTransferred-L::EdlsHeaderBytes),
            inspected,2048U)==L::EdlgFrameReadResult::Succeeded);
        assert(inspected.Codec==L::EdlgCodec::Cbor);
        Projection decoded{};
        const auto parsed=Serial::Deserialise<
            Serial::Cbor,Serial::RootProfile::TypedEnvelope>(
                inspected.Payload,decoded);
        assert(parsed.IsSuccessful());
        assert(decoded.Message.String==record.Message.String);
        assert(decoded.SequenceWidth==16U);
        assert(decoded.Attempt.Value()==record.Attempt.Value());
        assert(decoded.DiagnosticSchemaFingerprint==
            FixtureMessage::OwningContract::DiagnosticSchemaFingerprint);
        assert(std::get<0>(decoded.Arguments)==0x01020304U);
        assert(std::get<1>(decoded.Arguments)==0x0102030405060708ULL);
    }
    Storage final(folder,bytes);
    const auto before=L::InspectEdlsArchive<3U,2048U>(final,Paths);
    assert(before.NextGeneration==13U);
    const auto writesBefore=before.NextGeneration;
    auto badId=identity;
    badId[0U]^=std::byte{0x01U};
    const auto refusedId=submit(final,record,cbor,badId,7U);
    assert(refusedId.Status==L::GeneratedEdlsPublishStatus::SourceIdentityMismatch);
    const auto refusedIncarnation=submit(final,record,cbor,identity,8U);
    assert(refusedIncarnation.Status==
        L::GeneratedEdlsPublishStatus::SourceIdentityMismatch);
    const auto tooSmall=submit(final,record,
        std::span<std::byte>(cbor).first(2U),identity,7U);
    assert(tooSmall.Status==
        L::GeneratedEdlsPublishStatus::CborCapacityExceeded);
    auto invalidRecord=record;
    invalidRecord.DiagnosticSchemaFingerprint[0U]^=0x11U;
    const auto refusedSchema=submit(final,invalidRecord,cbor,identity,7U);
    assert(refusedSchema.Status==
        L::GeneratedEdlsPublishStatus::ProjectionRejected);
    assert(refusedSchema.Projection.Status==
        L::GeneratedLogRecordProjectionStatus::DiagnosticFingerprintMismatch);
    assert(!std::get<0>(scratch).has_value());
    assert(!std::get<1>(scratch).has_value());
    const auto after=L::InspectEdlsArchive<3U,2048U>(final,Paths);
    assert(after.Condition==L::EdlsArchiveCondition::Unambiguous);
    assert(after.NextGeneration==writesBefore);

    // Real worker-side adapter boundaries: existing generated projection
    // encoder produces borrowed CBOR, and existing Task worker callback
    // receives *separately copied* bytes for typed decode + EDLS commit.
    // The independently qualified AsyncCopyingTaskSink holds that copy.
    L::GeneratedLogRecordCborEncoder<
        FixtureMessage,L::AttemptSequenceWidth::Bits16,2U,12U,
        S::Identity::RuntimeIncarnationId,C::ClockEra
    > encoder(projection,scratch);
    const auto workerCbor=encoder(record,cbor);
    assert(workerCbor.Status==L::RepresentationEncodeStatus::Succeeded);
    assert(workerCbor.Written>0U);
    Projection workerDecoded{};
    L::TypedCborProtectedEdlsTaskDelivery<
        FixtureMessage,L::AttemptSequenceWidth::Bits16,
        3U,2U,2048U,2200U,Storage,
        S::Identity::RuntimeIncarnationId,C::ClockEra
    > delivery(final,workerDecoded,frame,archiveScratch,Paths,identity,7U);
    const auto view=std::span<const std::byte>(
        cbor.data(),workerCbor.Written);
    assert(delivery(L::EdlgCodec::Json,view)==
        L::RecordSinkSubmission::Rejected);
    assert(delivery.LastCompletion==
        L::TypedEdlsTaskCompletion::InvalidCodec);
    assert(delivery(L::EdlgCodec::Cbor,
        std::span<const std::byte>(cbor.data(),1U))==
        L::RecordSinkSubmission::Rejected);
    assert(delivery.LastCompletion==
        L::TypedEdlsTaskCompletion::InvalidTypedRecord);
    // A semantic decoder may accept trailing CBOR; the actual persistence
    // Task sink requires exact owner-canonical byte representation.
    std::array<std::byte,2049U> trailing{};
    for(std::size_t i=0U;i<view.size();++i) trailing[i]=view[i];
    trailing[view.size()]=std::byte{0U};
    assert(delivery(L::EdlgCodec::Cbor,
        std::span<const std::byte>(trailing.data(),view.size()+1U))==
        L::RecordSinkSubmission::Rejected);
    assert(delivery.LastCompletion==
        L::TypedEdlsTaskCompletion::InvalidTypedRecord);
    assert((L::InspectEdlsArchive<3U,2048U>(final,Paths).NextGeneration==13U));
    assert(delivery(L::EdlgCodec::Cbor,view)==
        L::RecordSinkSubmission::Accepted);
    assert(delivery.LastCompletion==L::TypedEdlsTaskCompletion::Archived);
    assert(delivery.LastArchive.Generation==13U);
    assert(delivery.LastArchive.Status==
        L::EdlsProtectedRollingStatus::PublishedAndRetired);
    assert((L::InspectEdlsArchive<3U,2048U>(final,Paths).NextGeneration==14U));
    std::size_t cleaned=0U;
    for(std::size_t i=0U;i<3U;++i) {
        char name[240U]{};
        assert(std::snprintf(name,sizeof(name),"%s/slot%zu",folder,i)>0);
        if(::unlink(name)==0)++cleaned;
        else assert(errno==ENOENT);
    }
    assert(cleaned==2U);
    assert(::rmdir(folder)==0);
}
