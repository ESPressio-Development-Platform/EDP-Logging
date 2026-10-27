#pragma once

// Test-only candidate write/GC churn on the *genuinely physical* LittleFS
// partition. Never calls the production retirement path on an unknown tail.
// The GPIO17 host controller (not firmware reset reason) is the cut witness.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

    // Deliberately exceeds the legitimate 4KiB EDLS slot; the candidate is
    // NOT an acknowledged record. Only an unused physical name is mutated.
    // Fixed, static SRAM allocation; never an ESP-IDF main-task stack array.
    static std::array<std::uint8_t,65536U> MidwriteUnackedCandidate{};

    static void VerifyMidwriteProtectedWindow(Store& storage) {
        for(std::size_t slot=0U;slot<2U;++slot) {
            const auto inspected=L::InspectEdlsSlot<32U>(
                storage,Paths[slot],static_cast<std::uint16_t>(slot));
            Require(inspected.Condition==L::EdlsSlotCondition::Clean &&
                inspected.HasHeader &&
                inspected.Header.Generation==slot+1U &&
                inspected.Header.DeviceIdentifier==DeviceIdentifier &&
                inspected.Header.RuntimeIncarnation==3U &&
                inspected.VerifiedFrames==1U &&
                inspected.VerifiedPrefixBytes==64U,
                "MIDWRITE_PROTECTED_ACK_CRC_DAMAGED");
        }
    }

    [[noreturn]] static void ExecuteMidwriteFaultProbe(
        Store& storage,
        esp_reset_reason_t reset,
        nvs_handle_t session,
        esp_err_t phase,
        std::uint8_t armed
    ) {
        if(phase==ESP_OK && armed==1U) {
            std::printf("EDP_BND|RESET_OBSERVED|reason=%u|por=%u\n",
                static_cast<unsigned>(reset),
                reset==ESP_RST_POWERON?1U:0U);
            std::fflush(stdout);
            std::uint8_t ack=0U;
            Require(nvs_get_u8(session,"ack",&ack)==ESP_OK && ack==2U,
                "MIDWRITE_NVS_ACK2_NOT_DURABLE");
            VerifyMidwriteProtectedWindow(storage);
            // Crucial: do NOT interpret a damaged unacknowledged candidate
            // as permission to retire an acknowledged predecessor.
            const auto unack=storage.GetFileSize(Path2.Value);
            if(unack.Status==P::FileSizeStatus::Succeeded) {
                const auto probe=L::InspectEdlsSlot<32U>(
                    storage,Path2.Value,2U);
                std::printf(
                    "EDP_EDLS|MIDWRITE_TORN_CANDIDATE|condition=%u|size=%llu\n",
                    static_cast<unsigned>(probe.Condition),
                    static_cast<unsigned long long>(unack.Size.RawValue));
                std::fflush(stdout);
                const auto [frame,fcount]=Frame();
                const auto refused=L::PublishEdlsProtectedRollingSuccessor<
                    3U,2U,32U,1024U>(
                    storage,Paths,DeviceIdentifier,3U,
                    std::span<const std::byte>(frame.data(),fcount));
                Require(refused.Status==
                    L::EdlsProtectedRollingStatus::ArchiveAmbiguous,
                    "MIDWRITE_TORN_CANDIDATE_NOT_REFUSED");
            } else {
                Require(unack.Status==P::FileSizeStatus::NotFound,
                    "MIDWRITE_CANDIDATE_UNKNOWN_FILE_FAILURE");
                std::printf("EDP_EDLS|MIDWRITE_CANDIDATE_ABSENT\n");
            }
            VerifyMidwriteProtectedWindow(storage);
            std::printf(
                "EDP_BND|RECOVERED|newest=2|older=1|valid=2|contiguous=1\n");
            std::printf(
                "EDP_BND|REPLAY_VERIFIED|TWO_PROTECTED_AFTER_MIDWRITE_CUT\n");
            std::fflush(stdout);
            for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
        }
        Require(phase==ESP_ERR_NVS_NOT_FOUND,
            "MIDWRITE_NOT_CLEAN_PROVISIONED_NVS");
        const auto [source,used]=Frame();
        const auto encoded=std::span<const std::byte>(source.data(),used);
        for(std::uint32_t g=1U;g<=2U;++g) {
            const auto commit=L::PublishEdlsProtectedRollingSuccessor<
                3U,2U,32U,1024U>(
                storage,Paths,DeviceIdentifier,3U,encoded);
            Require(
                commit.Status==
                    L::EdlsProtectedRollingStatus::PublishedWithinCapacity &&
                commit.Generation==g,"MIDWRITE_ACK_BASELINE");
            std::printf("EDP_BND|ACK|gen=%u\n",g);
            std::fflush(stdout);
        }
        VerifyMidwriteProtectedWindow(storage);
        Require(nvs_set_u8(session,"ack",2U)==ESP_OK &&
            nvs_set_u8(session,"armed",1U)==ESP_OK &&
            nvs_commit(session)==ESP_OK,
            "MIDWRITE_NVS_ARM_COMMIT");
        nvs_close(session);
        std::printf(
            "EDP_BND|BASELINE_READY|window=1,2|wait_ms=6000\n");
        std::fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(6000U));

        MidwriteUnackedCandidate.fill(0xACU);
        std::printf("EDP_BND|MIDWRITE_START|attempt=1\n");
        std::fflush(stdout);
        for(std::uint32_t attempt=1U;attempt<=400U;++attempt) {
            MidwriteUnackedCandidate[0U]=static_cast<std::uint8_t>(attempt);
            // The ONLY target of these stressed native writes is the
            // unacknowledged spare, with g1/g2 immutable and CRC-qualified.
            const auto result=storage.ReplaceFile(
                Path2.Value,P::SourceBufferView{
                    MidwriteUnackedCandidate.data(),
                    MidwriteUnackedCandidate.size()
                });
            std::printf(
                "EDP_EDLS|MIDWRITE_RETURN|attempt=%u|native=%u\n",
                attempt,static_cast<unsigned>(result));
            std::fflush(stdout);
            if(result!=P::FileReplaceStatus::Succeeded) {
                // Error can be legitimate physical pressure, but host must
                // still cut and postcut verify both acknowledged records.
                break;
            }
        }
        for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
    }

} // test fixture internal namespace
