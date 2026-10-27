#pragma once
// Hardware-only Bootstrap composition: true ESP-IDF NVS KeyValueStorage
// (PowerLoss atomic) plus existing conservative LittleFS FileStorage.
#include <NvsKeyValueStorage.hpp>
#include <logging/EdlsPowerLossAcknowledgement.hpp>

namespace {

    struct StrictAckNvsBinding final {};
    using StrictAckNvs=E::NvsKeyValueStorage<StrictAckNvsBinding,Bytes>;
    using StrictAckArchive=L::PowerLossAcknowledgedEdls<
        3U,2U,32U,1024U,Store,StrictAckNvs>;

    static std::array<std::byte,128U> StrictScratch{};
    static std::array<std::uint8_t,65536U> StrictStressCandidate{};
    constexpr auto StrictKey=P::KeyView::Validate("edlsack");
    static_assert(StrictKey.IsValuePresent);

    static void StrictVerifyPair(Store& store,
                                 std::uint64_t older,
                                 std::uint64_t newer,
                                 std::uint32_t olderInc,
                                 std::uint32_t newerInc) {
        const auto scanned=L::InspectEdlsArchiveWithScratch<3U,32U>(
            store,Paths,StrictScratch);
        Require(scanned.Condition==L::EdlsArchiveCondition::Unambiguous &&
            scanned.NextGeneration==newer+1U,
            "STRICT_ACK_PHYSICAL_ARCHIVE_AMBIGUOUS");
        std::size_t count=0U;
        for(const auto& slot:scanned.Slots) {
            if(slot.Condition==L::EdlsSlotCondition::Empty)continue;
            Require(slot.Condition==L::EdlsSlotCondition::Clean &&
                slot.HasHeader &&
                slot.VerifiedFrames==1U &&
                slot.VerifiedPrefixBytes==64U &&
                slot.Header.DeviceIdentifier==DeviceIdentifier &&
                ((slot.Header.Generation==older &&
                  slot.Header.RuntimeIncarnation==olderInc) ||
                 (slot.Header.Generation==newer &&
                  slot.Header.RuntimeIncarnation==newerInc)),
                "STRICT_ACK_PROTECTED_CRC_CHANGED");
            ++count;
        }
        Require(count==2U,"STRICT_ACK_WRONG_RETENTION");
    }

    [[noreturn]] static void ExecuteStrictNvsRecoveryHIL(
        Store& store,
        Bytes& byteOps,
        esp_reset_reason_t reset,
        nvs_handle_t session,
        esp_err_t phase,
        std::uint8_t armed
    ) {
        StrictAckNvs ledger(byteOps);
        Require(ledger.Open("edlsack2")==E::NvsOpenStatus::Succeeded,
            "STRICT_NVS_KEYVALUE_OPEN");
        // TEST-ONLY two-boot incarnation simulation. Genuine production
        // Bootstrap receives the value from EDP-System Identity provider.
        const std::uint32_t incarnation=
            phase==ESP_OK && armed==1U?4U:3U;
        StrictAckArchive archive(store,ledger,Paths,StrictKey.Value,
            DeviceIdentifier,incarnation);
        const auto [source,count]=Frame();
        const auto frame=std::span<const std::byte>(source.data(),count);

        if(phase==ESP_OK && armed==1U) {
            std::printf("EDP_BND|RESET_OBSERVED|reason=%u|por=%u\n",
                static_cast<unsigned>(reset),
                reset==ESP_RST_POWERON?1U:0U);
            std::fflush(stdout);
            const auto before=archive.Read();
            Require(before.Status==L::EdlsAcknowledgedStatus::Ready &&
                before.AcknowledgedGeneration==2U &&
                before.AcknowledgedIncarnation==3U,
                "STRICT_ACK_NVS2_LOST");
            // Prior acknowledged segment CRC checks MUST pass *before*
            // any deletion of a torn/unacknowledged physical spare.
            for(std::size_t slot=0U;slot<2U;++slot) {
                const auto parsed=L::InspectEdlsSlotWithScratch<32U>(
                    store,Paths[slot],static_cast<std::uint16_t>(slot),
                    StrictScratch);
                Require(parsed.Condition==L::EdlsSlotCondition::Clean &&
                    parsed.Header.Generation==slot+1U &&
                    parsed.Header.DeviceIdentifier==DeviceIdentifier &&
                    parsed.Header.RuntimeIncarnation==3U &&
                    parsed.VerifiedFrames==1U &&
                    parsed.VerifiedPrefixBytes==64U,
                    "STRICT_PRE_REPAIR_ACK2_CRC");
            }
            const auto extra=store.GetFileSize(Path2.Value);
            Require(extra.Status==P::FileSizeStatus::NotFound ||
                extra.Status==P::FileSizeStatus::Succeeded,
                "STRICT_UNACKED_CANDIDATE_UNKNOWN");
            const auto recovered=archive.Recover(StrictScratch);
            Require((recovered.Status==
                        L::EdlsAcknowledgedStatus::UnacknowledgedCandidateRemoved ||
                    recovered.Status==L::EdlsAcknowledgedStatus::Ready) &&
                    recovered.AcknowledgedGeneration==2U,
                "STRICT_NVS_ACK2_REPAIR");
            Require(store.GetFileSize(Path2.Value).Status==
                P::FileSizeStatus::NotFound,"STRICT_NO_UNACKED_SPARE_REMAINS");
            StrictVerifyPair(store,1U,2U,3U,3U);
            const auto g3=archive.Publish(frame,StrictScratch);
            Require(g3.Status==
                L::EdlsAcknowledgedStatus::AcknowledgedAndRetired &&
                g3.AcknowledgedGeneration==3U,
                "STRICT_ACK3_COMMIT_AND_ROLL");
            StrictVerifyPair(store,2U,3U,3U,4U);
            const auto g4=archive.Publish(frame,StrictScratch);
            Require(g4.Status==
                L::EdlsAcknowledgedStatus::AcknowledgedAndRetired &&
                g4.AcknowledgedGeneration==4U,
                "STRICT_ACK4_COMMIT_AND_ROLL");
            StrictVerifyPair(store,3U,4U,4U,4U);
            const auto final=archive.Read();
            Require(final.Status==L::EdlsAcknowledgedStatus::Ready &&
                final.AcknowledgedGeneration==4U &&
                final.AcknowledgedIncarnation==4U,
                "STRICT_NVS_ACK4_READBACK");
            std::printf("EDP_EDLS|STRICT_NVS_RECOVERED|ack=4|window=3,4\n");
            std::printf(
                "EDP_BND|RECOVERED|newest=4|older=3|valid=2|contiguous=1\n");
            std::printf(
                "EDP_BND|REPLAY_VERIFIED|STRICT_TWO_PROTECTED_AFTER_REPAIR\n");
            std::fflush(stdout);
            ledger.Close();
            for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
        }
        Require(phase==ESP_ERR_NVS_NOT_FOUND,
            "STRICT_ACK_TEST_PARTITION_NOT_CLEAN");
        const auto init=archive.InitializeEmpty();
        Require(init.Status==L::EdlsAcknowledgedStatus::Initialized,
            "STRICT_ACK_GENESIS");
        for(std::uint32_t generation=1U;generation<=2U;++generation) {
            const auto published=archive.Publish(frame,StrictScratch);
            Require(published.Status==
                L::EdlsAcknowledgedStatus::AcknowledgedWithinCapacity &&
                published.AcknowledgedGeneration==generation,
                "STRICT_ACK_G1_G2_DURABLE");
            std::printf("EDP_BND|ACK|gen=%u\n",generation);
            std::fflush(stdout);
        }
        StrictVerifyPair(store,1U,2U,3U,3U);
        Require(nvs_set_u8(session,"armed",1U)==ESP_OK &&
            nvs_commit(session)==ESP_OK,
            "STRICT_TEST_PHASE_NVS_COMMIT");
        nvs_close(session);
        std::printf(
            "EDP_BND|BASELINE_READY|window=1,2|wait_ms=6000\n");
        std::fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(6000U));
        // No generation-3 ACK is committed. This deliberately stresses
        // only the spare physical file while g1+g2 remain immutable.
        StrictStressCandidate.fill(0xA5U);
        std::printf("EDP_BND|STRICT_CANDIDATE_START|attempt=1\n");
        std::fflush(stdout);
        for(std::uint32_t index=1U;index<=240U;++index) {
            StrictStressCandidate[0U]=static_cast<std::uint8_t>(index);
            const auto native=store.ReplaceFile(
                Path2.Value,P::SourceBufferView{
                    StrictStressCandidate.data(),
                    StrictStressCandidate.size()});
            std::printf(
                "EDP_EDLS|STRICT_UNACK_NATIVE|attempt=%u|status=%u\n",
                index,static_cast<unsigned>(native));
            std::fflush(stdout);
            if(native!=P::FileReplaceStatus::Succeeded)break;
        }
        Halt("STRICT_TEST_DID_NOT_OBSERVE_CUT");
    }

} // Test-only ESP32 Bootstrap composition
