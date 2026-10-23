// Real ESP32 PowerLoss-atomic KeyValueStorage qualification.
// The user-verified GPIO17 switched supply is controlled ONLY by the host
// Policy-approved esp32-integrated daemon. This firmware owns no GPIO relay.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <ESPressio_Persistence_ESP_IDF.hpp>
#include <NvsKeyValueStorage.hpp>
#include <memory/ByteOperationsProvider.hpp>
#include <logging/PowerLossEdlsSnapshots.hpp>
#include <logging/EdlgFrame.hpp>

namespace P=ESPressio::Persistence;
namespace L=ESPressio::Logging;
namespace Platform=ESPressio::Platform;
using Bytes=Platform::Portable::Memory::ByteOperationsProvider;
struct SnapshotTag final {};
using Storage=P::EspIdf::NvsKeyValueStorage<SnapshotTag,Bytes>;
using Archive=L::PowerLossEdlsSnapshots<2U,256U,64U,Storage>;
constexpr auto KA=P::KeyView::Validate("a");
constexpr auto KB=P::KeyView::Validate("b");
static_assert(KA.IsValuePresent && KB.IsValuePresent);
const std::array<P::KeyView,2U> Keys{KA.Value,KB.Value};
std::array<std::byte,16U> DeviceId{};

[[noreturn]] static void Halt(const char* tag) {
    std::printf("EDP_DURABLE|HALT|%s\n",tag);
    std::fflush(stdout);
    for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
}
static void Require(bool ok,const char* reason) {
    if(!ok)Halt(reason);
}

extern "C" void app_main() {
    const auto why=esp_reset_reason();
    std::printf("EDP_DURABLE|BOOT|reset=%u\n",static_cast<unsigned>(why));
    std::fflush(stdout);
    Require(nvs_flash_init()==ESP_OK,"NVS_INIT");
    nvs_handle_t phaseOwner{};
    Require(nvs_open("edpdsnap26",NVS_READWRITE,&phaseOwner)==ESP_OK,"PHASE_OPEN");
    std::uint8_t armed=0U;
    const auto phaseRead=nvs_get_u8(phaseOwner,"phase",&armed);
    Require(phaseRead==ESP_OK||phaseRead==ESP_ERR_NVS_NOT_FOUND,"PHASE_READ");
    static Bytes bytes;
    static Storage storage(bytes);
    Require(storage.Open("edpksnap26")==P::EspIdf::NvsOpenStatus::Succeeded,"STORAGE_OPEN");
    Archive archive(storage,Keys);
    DeviceId.fill(std::byte{0xA5U});
    if(phaseRead==ESP_OK&&armed==1U) {
        Require(why==ESP_RST_POWERON,"POWER_CUT_NOT_REAL");
        const auto observed=archive.Inspect();
        std::array<std::byte,128U> recovered{};
        std::size_t size{};
        const auto restored=archive.RecoverNewest(recovered,size);
        const bool valid=(restored.Status==L::DurableEdlsStatus::Clean ||
            restored.Status==L::DurableEdlsStatus::Degraded);
        std::printf("EDP_DURABLE|RECOVERY|status=%u|gen=%llu|slot=%u|bytes=%u|inspection=%u\n",
            static_cast<unsigned>(restored.Status),
            static_cast<unsigned long long>(restored.Generation),
            static_cast<unsigned>(restored.NewestSlot),
            static_cast<unsigned>(size),
            static_cast<unsigned>(observed.Status));
        std::fflush(stdout);
        Require(valid&&restored.Generation>=2U&&size>0U,"LATEST_NOT_RECOVERED");
        L::EdlgFrameView decoded{};
        Require(L::ReadEdlgFrame(
            std::span<const std::byte>(recovered.data(),size),
            decoded,64U
        )==L::EdlgFrameReadResult::Succeeded,"RECOVERED_CRC");
        std::printf("EDP_DURABLE|PASS|POWERLOSS_NEWEST_VALID|gen=%llu\n",
            static_cast<unsigned long long>(restored.Generation));
        std::fflush(stdout);
        for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
    }
    Require(phaseRead==ESP_ERR_NVS_NOT_FOUND,"UNEXPECTED_PHASE");
    const auto existing=archive.Inspect();
    // The first ESP-IDF flash/verify operation may briefly start the
    // application before the host has taken UART monitor ownership. A
    // previously completed *exact* two-key baseline is safe to reuse;
    // unexpected or partially prepared data must never be erased.
    Require(existing.Status==L::DurableEdlsStatus::Empty ||
        (existing.Status==L::DurableEdlsStatus::Clean &&
         existing.Generation==2U && existing.NewestSlot==1U &&
         existing.SlotGenerations[0U]==1U &&
         existing.SlotGenerations[1U]==2U &&
         existing.Identity==DeviceId),"UNEXPECTED_EXISTING_KEY_DATA");
    std::array<std::byte,80U> encoded{};
    const std::array<std::byte,4U> payload{
        std::byte{0x84U},std::byte{0x01U},std::byte{0x02U},std::byte{0x03U}
    };
    std::size_t used{};
    Require(L::WriteEdlgFrame(L::EdlgCodec::Cbor,payload,encoded,used,64U)
        ==L::EdlgFrameWriteResult::Succeeded,"FRAME_INIT");
    auto frame=std::span<const std::byte>(encoded.data(),used);
    if(existing.Status==L::DurableEdlsStatus::Empty) {
        for(std::uint32_t n=1U;n<=2U;++n) {
            const auto committed=archive.Commit(DeviceId,n,frame);
            Require(committed.Status==L::DurableEdlsStatus::Committed&&
                committed.Generation==n,"BASELINE_COMMIT");
        }
    }
    std::printf("EDP_DURABLE|BASELINE_STABLE|gen=2|wait_ms=12000\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(12000U));
    // Only arm after the host's flash/verify/reset sequencing has had
    // adequate time to regain sole control of the serial UART.
    Require(nvs_set_u8(phaseOwner,"phase",1U)==ESP_OK &&
        nvs_commit(phaseOwner)==ESP_OK,"PHASE_COMMIT");
    nvs_close(phaseOwner);
    std::printf("EDP_DURABLE|ACK|gen=2|slot=1\n");
    std::printf("EDP_DURABLE|BASELINE_READY|gen=2|wait_ms=250\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(250U));
    for(std::uint32_t n=3U;n<=80U;++n) {
        std::printf("EDP_DURABLE|COMMIT_ENTER|gen=%u\n",n);
        std::fflush(stdout);
        const auto result=archive.Commit(DeviceId,n,frame);
        if(result.Status!=L::DurableEdlsStatus::Committed) {
            std::printf("EDP_DURABLE|ERROR|commit=%u|status=%u|native=%u\n",
                n,static_cast<unsigned>(result.Status),
                static_cast<unsigned>(result.Native));
            std::fflush(stdout);
            Halt("COMMIT_FAILURE");
        }
        std::printf("EDP_DURABLE|ACK|gen=%u|slot=%u\n",
            n,static_cast<unsigned>(result.Slot));
        std::fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    Halt("NO_RELAY_POWER_CUT");
}
