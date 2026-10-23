// Dedicated real NVS PowerLoss historical EDLS journal HIL.
// ESP32 VDD relay is controlled ONLY by the Policy-approved RPI400 host.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <ESPressio_Persistence_ESP_IDF.hpp>
#include <NvsKeyValueStorage.hpp>
#include <memory/ByteOperationsProvider.hpp>
#include <logging/PowerLossEdlsJournal.hpp>
#include <logging/EdlgFrame.hpp>

namespace P = ESPressio::Persistence;
namespace L = ESPressio::Logging;
namespace Platform = ESPressio::Platform;

using Bytes = Platform::Portable::Memory::ByteOperationsProvider;
struct JournalTag final {};
using Storage = P::EspIdf::NvsKeyValueStorage<JournalTag,Bytes>;
using Journal = L::PowerLossEdlsJournal<3U,256U,64U,Storage>;

constexpr auto KA = P::KeyView::Validate("a");
constexpr auto KB = P::KeyView::Validate("b");
constexpr auto KC = P::KeyView::Validate("c");
static_assert(KA.IsValuePresent && KB.IsValuePresent && KC.IsValuePresent);
const std::array<P::KeyView,3U> Keys{KA.Value,KB.Value,KC.Value};

[[noreturn]] void Halt(const char* reason) {
    std::printf("EDP_JOURNAL|HALT|%s\n",reason);
    std::fflush(stdout);
    for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
}

void Require(bool ok,const char* reason) {
    if(!ok)Halt(reason);
}

[[nodiscard]] bool MakeFrame(
    std::uint32_t generation,
    std::array<std::byte,80U>& buffer,
    std::size_t& used
) noexcept {
    const std::array<std::byte,3U> payload{
        std::byte{0x83U},
        std::byte{0x01U},
        std::byte{static_cast<std::uint8_t>(generation)}
    };
    return L::WriteEdlgFrame(
        L::EdlgCodec::Cbor,
        payload,
        buffer,
        used,
        64U
    )==L::EdlgFrameWriteResult::Succeeded;
}

extern "C" void app_main() {
    const auto resetReason=esp_reset_reason();
    std::printf("EDP_JOURNAL|BOOT|reset=%u\n",
        static_cast<unsigned>(resetReason));
    std::fflush(stdout);
    Require(nvs_flash_init()==ESP_OK,"NVS_INIT");
    nvs_handle_t phase{};
    Require(nvs_open("edpjphase26",NVS_READWRITE,&phase)==ESP_OK,
        "PHASE_OPEN");
    std::uint8_t armed{};
    const auto phaseResult=nvs_get_u8(phase,"armed",&armed);
    Require(phaseResult==ESP_OK ||
        phaseResult==ESP_ERR_NVS_NOT_FOUND,"PHASE_READ");

    static Bytes byteOperations;
    static Storage storage(byteOperations);
    Require(storage.Open("edpjhist26")==P::EspIdf::NvsOpenStatus::Succeeded,
        "JOURNAL_STORAGE_OPEN");
    Journal journal(storage,Keys);
    std::array<std::byte,16U> identity{};
    identity.fill(std::byte{0xA5U});

    if(phaseResult==ESP_OK && armed==1U) {
        Require(resetReason==ESP_RST_POWERON,"NOT_TRUE_POWERON");
        const auto recovered=journal.Inspect();
        std::printf(
            "EDP_JOURNAL|RECOVERY|status=%u|gen=%llu|oldest=%llu|count=%u\n",
            static_cast<unsigned>(recovered.Status),
            static_cast<unsigned long long>(recovered.Newest),
            static_cast<unsigned long long>(recovered.OldestRetained),
            static_cast<unsigned>(recovered.RetainedCount)
        );
        std::fflush(stdout);
        Require(recovered.Status==L::DurableEdlsStatus::Clean &&
            recovered.RetainedCount==2U &&
            recovered.Newest>=3U &&
            recovered.Newest-recovered.OldestRetained==1U,
            "HISTORY_INVALID");
        std::array<std::byte,128U> bytes{};
        for(auto generation=recovered.OldestRetained;
            generation<=recovered.Newest;++generation) {
            const auto replay=journal.RecoverRetained(
                generation,
                bytes
            );
            Require(replay.Status==L::DurableEdlsStatus::Clean,
                "REPLAY_FAILURE");
            L::EdlgFrameView verified{};
            Require(L::ReadEdlgFrame(
                std::span<const std::byte>(bytes.data(),replay.FrameBytes),
                verified,
                64U
            )==L::EdlgFrameReadResult::Succeeded,"REPLAY_CRC");
            Require(verified.Payload.size()==3U &&
                verified.Payload[2U]==std::byte{
                    static_cast<std::uint8_t>(generation)
                },"REPLAY_WRONG_GENERATION");
            std::printf("EDP_JOURNAL|REPLAY|gen=%llu|crc=PASS\n",
                static_cast<unsigned long long>(generation));
            std::fflush(stdout);
        }
        std::printf(
            "EDP_JOURNAL|PASS|POWERLOSS_HISTORY_VALID|gen=%llu|count=2\n",
            static_cast<unsigned long long>(recovered.Newest)
        );
        std::fflush(stdout);
        for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
    }

    Require(phaseResult==ESP_ERR_NVS_NOT_FOUND,
        "UNEXPECTED_PRIOR_PHASE");
    const auto initial=journal.Inspect();
    Require(initial.Status==L::DurableEdlsStatus::Empty ||
       (initial.Status==L::DurableEdlsStatus::Clean &&
        initial.Newest==3U &&
        initial.OldestRetained==2U &&
        initial.RetainedCount==2U),
       "UNEXPECTED_STORED_ARCHIVE");

    std::array<std::byte,80U> frame{};
    if(initial.Status==L::DurableEdlsStatus::Empty) {
        for(std::uint32_t generation=1U;generation<=3U;++generation) {
            std::size_t size{};
            Require(MakeFrame(generation,frame,size),"BASELINE_FRAME");
            const auto result=journal.Append(
                identity,
                generation,
                std::span<const std::byte>(frame.data(),size)
            );
            Require(result.Status==L::DurableEdlsStatus::Committed &&
                result.Generation==generation,"BASELINE_APPEND");
        }
    }
    std::printf("EDP_JOURNAL|BASELINE_STABLE|gen=3|count=2|wait_ms=12000\n");
    std::fflush(stdout);
    // Independent esptool flash/verify may reset/boot before UART ownership.
    vTaskDelay(pdMS_TO_TICKS(12000U));
    Require(nvs_set_u8(phase,"armed",1U)==ESP_OK &&
        nvs_commit(phase)==ESP_OK,"PHASE_COMMIT");
    nvs_close(phase);
    std::printf("EDP_JOURNAL|ACK|gen=3|count=2\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(250U));

    // Bounded verification campaign, NOT production flash write cadence.
    for(std::uint32_t generation=4U;generation<=80U;++generation) {
        std::size_t size{};
        Require(MakeFrame(generation,frame,size),"ACTIVE_FRAME");
        std::printf("EDP_JOURNAL|COMMIT_ENTER|gen=%u\n",generation);
        std::fflush(stdout);
        const auto result=journal.Append(
            identity,
            generation,
            std::span<const std::byte>(frame.data(),size)
        );
        if(result.Status!=L::DurableEdlsStatus::Committed ||
           result.Generation!=generation) {
            std::printf("EDP_JOURNAL|ERROR|generation=%u|status=%u\n",
                generation,static_cast<unsigned>(result.Status));
            std::fflush(stdout);
            Halt("JOURNAL_APPEND_FAILED");
        }
        std::printf("EDP_JOURNAL|ACK|gen=%u|slot=%u\n",
            generation,static_cast<unsigned>(result.Slot));
        std::fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    Halt("NO_PHYSICAL_POWER_CUT");
}
