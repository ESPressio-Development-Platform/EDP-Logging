#pragma once
// Native HIL of owner-generated all-fifteen-field TypedEnvelope CBOR,
// real canonical EDLS, and explicit latest-N durability after GPIO17 cut.
// Composition ONLY: production library code remains portable.
#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticClockEraArguments.hpp>
#include <logging/LogRecordSourceAssembly.hpp>
#include <logging/GeneratedLogRecordEdlsArchive.hpp>
#include <logging/EdlgFrameReader.hpp>

namespace C=ESPressio::Clock;
namespace S=ESPressio::System;
namespace Serial=ESPressio::Serialisation;
using TypedSchema=Fixture::Diagnostic::DiagnosticMessageSchema<1U,0U,5U>;
using TypedRecord=L::BasicLogRecord<
    L::AttemptSequenceWidth::Bits16,2U,12U>;
using TypedProjection=L::GeneratedLogRecordWireProjection<
    L::AttemptSequenceWidth::Bits16,
    S::Identity::RuntimeIncarnationId,C::ClockEra>;
using TypedScratch=L::OwnedPositionalScratch<
    S::Identity::RuntimeIncarnationId,C::ClockEra>;
static TypedRecord TypedSource{};
static TypedProjection TypedWire{};
static TypedProjection TypedDecoded{};
static TypedScratch TypedPositional{};
static std::array<std::byte,1024U> TypedCbor{};
static std::array<std::byte,1200U> TypedEdlg{};
static std::array<std::byte,1200U> TypedArchiveScratch{};
static std::array<std::byte,1200U> TypedReadback{};

static void PrepareTypedRecord(std::uint32_t generation) {
    // A prior already-committed record remains structurally occupied;
    // the next per-attempt source slot must be newly empty. No changes
    // to the production assembly precondition.
    TypedSource=TypedRecord{};
    S::Identity::DeviceIdentifier::Storage id{};
    id.fill(0xA6U);
    const S::Identity::DeviceIdentifier device{id};
    const L::QualifiedLogTime clock{
        C::MonotonicTimestamp::FromNanoseconds(123456U+generation),
        C::SynchronizedTimestamp{},
        C::SynchronizationUncertainty::Maximum(),
        C::ClockEra{},
        C::SynchronizationState::NeverSynchronized,
        L::LogTemporalEvidenceStatus::Unavailable
    };
    const auto attempt=L::AttemptSequence<
        L::AttemptSequenceWidth::Bits16>::FromValidatedValue(generation);
    Require(L::AssembleLogRecordSource<TypedSchema>(
        TypedSource,L::Severity::Warning,device,
        S::Identity::RuntimeIncarnationId{7U},attempt,clock,
        S::Identity::RuntimeIncarnationId{0x01020304U},
        C::ClockEra::FromValue(0x0102030405060708ULL)
    )==L::LogRecordAssemblyStatus::Prepared,"TYPED_PREPARE");
}
static auto PublishTyped(Store& store,std::uint32_t generation) {
    PrepareTypedRecord(generation);
    return L::PublishGeneratedLogRecordToProtectedEdlsWithScratch<
        TypedSchema,2U,1024U,1200U>(
        TypedSource,TypedWire,TypedPositional,
        TypedCbor,TypedEdlg,store,Paths,DeviceIdentifier,7U,
        TypedArchiveScratch);
}
static void VerifyTypedArchive(
    Store& store,std::uint64_t oldest,std::uint64_t newest
) {
    const auto archive=L::InspectEdlsArchiveWithScratch<3U,1024U>(
        store,Paths,TypedArchiveScratch);
    Require(archive.Condition==L::EdlsArchiveCondition::Unambiguous &&
        archive.NextGeneration==newest+1U,"TYPED_ARCHIVE_STATUS");
    std::uint32_t found=0U;
    for(std::size_t slot=0U;slot<Paths.size();++slot) {
        const auto& item=archive.Slots[slot];
        if(item.Condition==L::EdlsSlotCondition::Empty)continue;
        Require(item.Condition==L::EdlsSlotCondition::Clean &&
            item.HasHeader && item.VerifiedFrames==1U &&
            item.Header.DeviceIdentifier==DeviceIdentifier &&
            item.Header.RuntimeIncarnation==7U &&
            item.Header.Generation>=oldest &&
            item.Header.Generation<=newest,"TYPED_EDLS_CONTENT");
        const auto bytes=store.ReadFileAt(
            Paths[slot],P::StorageOffset{},
            P::DestinationBufferView{
                TypedReadback.data(),TypedReadback.size()
            });
        Require(bytes.Status==P::FileReadStatus::Succeeded &&
            bytes.BytesTransferred> L::EdlsHeaderBytes+20U &&
            bytes.BytesTransferred<TypedReadback.size(),
            "TYPED_READBACK_FAIL");
        L::EdlgFrameView view{};
        Require(L::ReadEdlgFrame(
            std::span<const std::byte>(
                TypedReadback.data()+L::EdlsHeaderBytes,
                bytes.BytesTransferred-L::EdlsHeaderBytes),
            view,1024U)==L::EdlgFrameReadResult::Succeeded &&
            view.Codec==L::EdlgCodec::Cbor &&
            view.Consumed==bytes.BytesTransferred-L::EdlsHeaderBytes,
            "TYPED_FRAME_CRC");
        const auto parsed=Serial::Deserialise<
            Serial::Cbor,Serial::RootProfile::TypedEnvelope>(
            view.Payload,TypedDecoded);
        Require(parsed.IsSuccessful() &&
            TypedDecoded.Attempt.Value()==item.Header.Generation &&
            TypedDecoded.Incarnation.Value()==7U &&
            TypedDecoded.Message.Domain==TypedSchema::SourceDomain &&
            TypedDecoded.Message.SubDomain==TypedSchema::SourceSubDomain &&
            TypedDecoded.Message.String==TypedSchema::SourceString &&
            TypedDecoded.DiagnosticSchemaFingerprint==
              TypedSchema::OwningContract::DiagnosticSchemaFingerprint &&
            std::get<0>(TypedDecoded.Arguments)==0x01020304U &&
            std::get<1>(TypedDecoded.Arguments)==0x0102030405060708ULL,
            "TYPED_FIFTEEN_FIELDS_DECODE");
        ++found;
        std::printf("EDP_EDLS|TYPED_VALID|gen=%u|payload=%u|slot=%u\n",
            static_cast<unsigned>(item.Header.Generation),
            static_cast<unsigned>(view.Payload.size()),
            static_cast<unsigned>(slot));
    }
    Require(found==newest-oldest+1U,"TYPED_FINITE_WINDOW_COUNT");
    std::fflush(stdout);
}
static void PreparePendingTypedSuccessor(Store& store) {
    PrepareTypedRecord(3U);
    Require(L::ProjectGeneratedLogRecord<
        TypedSchema,L::AttemptSequenceWidth::Bits16,
        S::Identity::RuntimeIncarnationId,C::ClockEra>(
        TypedSource,TypedWire,TypedPositional).IsSuccessful(),
        "TYPED_PROJECT_G3");
    const auto serialized=Serial::Serialise<
        Serial::Cbor,Serial::RootProfile::TypedEnvelope>(
        TypedWire,TypedCbor);
    Require(serialized.IsSuccessful(),"TYPED_ENCODE_G3");
    std::size_t framed=0U;
    Require(L::WriteEdlgFrame(L::EdlgCodec::Cbor,
        std::span<const std::byte>(TypedCbor.data(),
            serialized.BytesWritten),
        TypedEdlg,framed,1024U)==L::EdlgFrameWriteResult::Succeeded,
        "TYPED_FRAME_G3");
    const auto candidate=L::PublishEdlsSuccessorIntoEmptySlotWithScratch<
        3U,1024U,1200U>(
        store,Paths,DeviceIdentifier,7U,
        std::span<const std::byte>(TypedEdlg.data(),framed),
        TypedArchiveScratch);
    Require(candidate.Status==L::EdlsSuccessorStatus::PublishedAndVerified &&
        candidate.Generation==3U,"TYPED_G3_IMMUTABLE_READBACK");
    VerifyTypedArchive(store,1U,3U);
}
static void ExecuteTypedArchiveHIL(Store& store,esp_reset_reason_t reset,
    nvs_handle_t session,esp_err_t phase,std::uint8_t armed) {
    if(phase==ESP_OK && armed==1U) {
        std::printf("EDP_BND|RESET_OBSERVED|reason=%u|por=%u\n",
            static_cast<unsigned>(reset),reset==ESP_RST_POWERON?1U:0U);
        std::fflush(stdout);
        VerifyTypedArchive(store,1U,3U);
        const auto afterCut=L::FinishEdlsProtectedRollingRetirementWithScratch<
            3U,2U,1024U>(store,Paths,TypedArchiveScratch);
        Require(afterCut.Status==
            L::EdlsProtectedRollingStatus::RecoveredPendingRetirement,
            "TYPED_RECOVER_PENDING_RETIRE");
        VerifyTypedArchive(store,2U,3U);
        const auto g4=PublishTyped(store,4U);
        Require(g4.IsPublished() && g4.Archive.Generation==4U &&
            g4.Archive.Status==
            L::EdlsProtectedRollingStatus::PublishedAndRetired,
            "TYPED_G4_ROLL");
        VerifyTypedArchive(store,3U,4U);
        std::printf("EDP_BND|RECOVERED|newest=4|older=3|valid=2|contiguous=1\n");
        std::printf("EDP_EDLS|TYPED_REPLAY_VERIFIED|all_15_fields=1|window=3,4\n");
        std::fflush(stdout);
        for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
    }
    Require(phase==ESP_ERR_NVS_NOT_FOUND,"TYPED_PRIOR_SESSION");
    for(std::uint32_t gen=1U;gen<=2U;++gen) {
        const auto published=PublishTyped(store,gen);
        Require(published.IsPublished() &&
            published.Archive.Generation==gen &&
            published.Archive.Status==
                L::EdlsProtectedRollingStatus::PublishedWithinCapacity,
            "TYPED_PROTECTED_G1_G2");
    }
    VerifyTypedArchive(store,1U,2U);
    std::printf("EDP_BND|BASELINE_READY|window=1,2|wait_ms=12000\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(12000U));
    Require(nvs_set_u8(session,"armed",1U)==ESP_OK &&
        nvs_commit(session)==ESP_OK,"TYPED_ARM");
    nvs_close(session);
    PreparePendingTypedSuccessor(store);
    std::printf("EDP_BND|ACK|gen=2\n");
    std::printf("EDP_EDLS|TYPED_SUCCESSOR_PENDING|gen=3\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(15000U));
    Halt("TYPED_HARD_POWER_CUT_NOT_OBSERVED");
}
