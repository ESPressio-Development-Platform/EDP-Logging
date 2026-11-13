#pragma once
// Native test-only Bootstrap composition: owner-generated complete fifteen-
// field canonical CBOR -> existing EDLG -> existing strict immutable EDLS,
// with actual NVS PowerLoss-atomic KeyValueStorage ledger. The GPIO17 host
// controller, never ESP32 firmware, is the electrical VDD-cut witness.
#include <logging/EdlgFrameWriter.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>

#include <NvsKeyValueStorage.hpp>
#include <logging/EdlsPowerLossAcknowledgement.hpp>

namespace {
    struct TypedStrictNvsBinding final {};
    using TypedStrictNvs=E::NvsKeyValueStorage<TypedStrictNvsBinding,Bytes>;
    using TypedStrictArchive=L::PowerLossAcknowledgedEdls<
        3U,2U,1024U,1200U,Store,TypedStrictNvs>;
    constexpr auto TypedStrictAckKey=P::KeyView::Validate("edlsack");
    static_assert(TypedStrictAckKey.IsValuePresent);

    static auto PublishTypedStrict(
        TypedStrictArchive& archive,std::uint32_t generation
    ) {
        PrepareTypedRecord(generation);
        Require(L::ProjectGeneratedLogRecord<
            TypedSchema,L::AttemptSequenceWidth::Bits16,
            S::Identity::RuntimeIncarnationId,C::ClockEra>(
            TypedSource,TypedWire,TypedPositional).IsSuccessful(),
            "TYPED_STRICT_OWNER_PROJECTION");
        const auto encoded=Serial::Serialise<
            Serial::Cbor,Serial::RootProfile::TypedEnvelope>(
            TypedWire,TypedCbor);
        Require(encoded.IsSuccessful() &&
            encoded.BytesWritten<=1024U,"TYPED_STRICT_CANONICAL_CBOR");
        std::size_t framed=0U;
        Require(L::WriteEdlgFrame(L::EdlgCodec::Cbor,
            std::span<const std::byte>(TypedCbor.data(),
                encoded.BytesWritten),
            TypedEdlg,framed,1024U,
            ESPressio::Platform::Portable::Memory::ByteOperationsProvider{})==L::EdlgFrameWriteResult::Succeeded,
            "TYPED_STRICT_OWNER_EDLG_FRAME");
        return archive.Publish(
            std::span<const std::byte>(TypedEdlg.data(),framed),
            TypedArchiveScratch);
    }

    [[noreturn]] static void ExecuteTypedStrictAckHIL(
        Store& store,
        Bytes& byteOps,
        esp_reset_reason_t reset,
        nvs_handle_t session,
        esp_err_t phase,
        std::uint8_t armed
    ) {
        TypedStrictNvs ledger(byteOps);
        Require(ledger.Open("typedack")==E::NvsOpenStatus::Succeeded,
            "TYPED_STRICT_NVS_OPEN");
        TypedStrictArchive archive(store,ledger,Paths,
            TypedStrictAckKey.Value,DeviceIdentifier,7U);

        if(phase==ESP_OK && armed==1U) {
            std::printf("EDP_BND|RESET_OBSERVED|reason=%u|por=%u\n",
                static_cast<unsigned>(reset),
                reset==ESP_RST_POWERON?1U:0U);
            std::fflush(stdout);
            const auto pre=archive.Read();
            Require(pre.Status==L::EdlsAcknowledgedStatus::Ready &&
                pre.AcknowledgedGeneration==2U &&
                pre.AcknowledgedIncarnation==7U,
                "TYPED_STRICT_PRECUT_NVS_ACK2_NOT_DURABLE");
            // Confirm all three real typed records before cleanup: g3 was
            // complete/readback-verified, but NEVER acknowledged by NVS.
            VerifyTypedArchive(store,1U,3U);
            const auto recovered=archive.Recover(TypedArchiveScratch);
            Require(recovered.Status==
                L::EdlsAcknowledgedStatus::UnacknowledgedCandidateRemoved &&
                recovered.AcknowledgedGeneration==2U,
                "TYPED_STRICT_UNACK_G3_NOT_RECLAIMABLE");
            VerifyTypedArchive(store,1U,2U);
            Require(store.GetFileSize(Path2.Value).Status==
                P::FileSizeStatus::NotFound,
                "TYPED_STRICT_UNACKED_CANDIDATE_NOT_ABSENT");
            for(std::uint32_t gen=3U;gen<=4U;++gen) {
                const auto posted=PublishTypedStrict(archive,gen);
                Require(posted.Status==
                    L::EdlsAcknowledgedStatus::AcknowledgedAndRetired &&
                    posted.AcknowledgedGeneration==gen,
                    "TYPED_STRICT_POSTCUT_NVS_ACK_ROLL");
                const auto confirmed=archive.Read();
                Require(confirmed.Status==L::EdlsAcknowledgedStatus::Ready &&
                    confirmed.AcknowledgedGeneration==gen &&
                    confirmed.AcknowledgedIncarnation==7U,
                    "TYPED_STRICT_POSTCUT_NVS_READBACK");
            }
            VerifyTypedArchive(store,3U,4U);
            std::printf(
                "EDP_BND|RECOVERED|newest=4|older=3|valid=2|contiguous=1\n");
            std::printf(
                "EDP_EDLS|TYPED_STRICT_ACK_REPLAY_VERIFIED|"
                "all_15_fields=1|ack=4|window=3,4\n");
            std::printf(
                "EDP_BND|REPLAY_VERIFIED|STRICT_TYPED15_NVS_ACK_AFTER_CUT\n");
            std::fflush(stdout);
            ledger.Close();
            for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
        }

        Require(phase==ESP_ERR_NVS_NOT_FOUND,
            "TYPED_STRICT_TEST_PHASE_ALREADY_USED");
        Require(archive.InitializeEmpty().Status==
            L::EdlsAcknowledgedStatus::Initialized,
            "TYPED_STRICT_NVS_GENESIS");
        for(std::uint32_t gen=1U;gen<=2U;++gen) {
            const auto posted=PublishTypedStrict(archive,gen);
            Require(posted.Status==
                L::EdlsAcknowledgedStatus::AcknowledgedWithinCapacity &&
                posted.AcknowledgedGeneration==gen,
                "TYPED_STRICT_PRECUT_NVS_ACK");
            std::printf("EDP_BND|ACK|gen=%u\n",gen);
            std::fflush(stdout);
        }
        VerifyTypedArchive(store,1U,2U);
        const auto base=archive.Read();
        Require(base.Status==L::EdlsAcknowledgedStatus::Ready &&
            base.AcknowledgedGeneration==2U,
            "TYPED_STRICT_NVS_ACK2_NOT_COMMITTED");
        Require(nvs_set_u8(session,"armed",1U)==ESP_OK &&
            nvs_commit(session)==ESP_OK,"TYPED_STRICT_TEST_ARM");
        nvs_close(session);
        // Test-only: bypass strict ACK publisher to create a COMPLETELY
        // durable EDLS g3 without ever committing NVS ACK3. Recovery MUST
        // discard it and MUST NOT retire still acknowledged g1 or g2.
        PreparePendingTypedSuccessor(store);
        std::printf(
            "EDP_BND|TYPED_ACK_GAP|gen=3|nvs_ack=2|"
            "candidate_complete=1\n");
        std::fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(15000U));
        Halt("TYPED_STRICT_ACK_GAP_POWER_CUT_NOT_OBSERVED");
    }
} // test-only native composition
