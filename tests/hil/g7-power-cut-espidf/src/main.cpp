// Dedicated adversarial G7 FileStorage / EDLS retention fixture.
// Never formats SPIFFS, never modifies another application's EDLS paths.
// Its VFS FileStorage capability is Restart-only, NOT PowerLoss-atomic.
#include <logging/EdlgFrameWriter.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_spiffs.h>
#include <esp_system.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <ESPressio_Persistence_ESP_IDF.hpp>
#include <logging/EdlgFrame.hpp>
#include <logging/EdlsArchiveInspection.hpp>
#include <logging/EdlsArchiveSuccessor.hpp>
#include <VfsFileStorage.hpp>
#include <memory/ByteOperationsProvider.hpp>

namespace L = ESPressio::Logging;
namespace P = ESPressio::Persistence;
namespace Platform = ESPressio::Platform;
using Bytes = Platform::Portable::Memory::ByteOperationsProvider;

struct PowerCutMount final {};
using Binding = P::EspIdf::VfsBindingProfile<
    P::RetentionLevel::Restart,
    P::TextCaseSensitivity::CaseSensitive,
    P::MediaRemovability::Fixed,
    32U, 32U, 131072U
>;
using Files = P::EspIdf::VfsFileStorage<PowerCutMount, Binding, Bytes>;

constexpr auto Slot0 = P::FilePathView::Validate("power0");
constexpr auto Slot1 = P::FilePathView::Validate("power1");
constexpr auto CutTarget = P::FilePathView::Validate("powerlarge");
static_assert(Slot0.IsValuePresent && Slot1.IsValuePresent && CutTarget.IsValuePresent);
constexpr std::array<P::FilePathView, 2U> Paths{Slot0.Value, Slot1.Value};
std::array<std::byte, 16U> DeviceIdentity{};

// Fixed-size bounded native provider write; long enough for a controlled
// external relay interruption while the replace call is in progress.
// No dynamic overflow, fragmentation, other user paths or key mutation.
static std::array<std::byte, 96U * 1024U> PowerCutPayload{};

[[noreturn]] void Stop(const char* tag) {
    std::printf("EDP_POWERLOSS|HALT|%s\n", tag);
    std::fflush(stdout);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000U));
}

void Require(bool condition, const char* why) {
    if (!condition) Stop(why);
}

void EmitArchiveFacts(const char* stage, const L::EdlsArchiveInspection<2U>& result) {
    std::printf(
        "EDP_POWERLOSS|%s|condition=%u|slot0=%u|gen0=%llu|slot1=%u|gen1=%llu|newest=%u\n",
        stage,
        static_cast<unsigned>(result.Condition),
        static_cast<unsigned>(result.Slots[0].Condition),
        static_cast<unsigned long long>(result.Slots[0].Header.Generation),
        static_cast<unsigned>(result.Slots[1].Condition),
        static_cast<unsigned long long>(result.Slots[1].Header.Generation),
        static_cast<unsigned>(result.NewestSlot)
    );
    std::fflush(stdout);
}

bool BaselineIsValid(const L::EdlsArchiveInspection<2U>& state) {
    return state.Condition == L::EdlsArchiveCondition::Unambiguous &&
        state.Slots[0].Condition == L::EdlsSlotCondition::Clean &&
        state.Slots[1].Condition == L::EdlsSlotCondition::Clean &&
        state.Slots[0].Header.Generation == 1U &&
        state.Slots[1].Header.Generation == 2U &&
        state.Slots[0].VerifiedFrames == 1U &&
        state.Slots[1].VerifiedFrames == 1U &&
        state.NewestSlot == 1U;
}

extern "C" void app_main() {
    std::printf("EDP_POWERLOSS|BOOT|relay-test-v1\n");
    const auto resetReason = esp_reset_reason();
    std::printf("EDP_POWERLOSS|RESET_REASON|value=%u\n",
        static_cast<unsigned>(resetReason));
    std::fflush(stdout);
    Require(nvs_flash_init() == ESP_OK, "NVS_INIT");
    nvs_handle_t nvs{};
    Require(nvs_open("edploss26", NVS_READWRITE, &nvs) == ESP_OK, "NVS_OPEN");
    std::uint8_t stage{};
    const auto stageRead = nvs_get_u8(nvs, "armed", &stage);
    Require(stageRead == ESP_OK || stageRead == ESP_ERR_NVS_NOT_FOUND, "NVS_STAGE");

    const esp_vfs_spiffs_conf_t config{
        .base_path = "/edpfs",
        .partition_label = "edpfs",
        .max_files = 4,
        // Never erase/format after a damaged or failed power-loss mount.
        .format_if_mount_failed = (stageRead == ESP_ERR_NVS_NOT_FOUND)
    };
    const auto mounted = esp_vfs_spiffs_register(&config);
    if (mounted != ESP_OK) {
        std::printf("EDP_POWERLOSS|RECOVERY|MOUNT_FAILED|esp_err=%d\n", (int)mounted);
        std::fflush(stdout);
        Stop("NO_DESTRUCTIVE_FORMAT");
    }
    static Bytes bytes;
    Files files("/edpfs", bytes);
    Require(files.IsFileStorageReady(), "VFS_UNAVAILABLE");

    DeviceIdentity.fill(std::byte{0x9AU});
    auto before = L::InspectEdlsArchive<2U, 32U>(files, Paths);
    EmitArchiveFacts("ARCHIVE_SCAN", before);
    if (stageRead == ESP_OK && stage == 1U) {
        // A genuine loss and restoration of VDD must produce a hardware
        // power-on reset, not a software reboot or GPIO EN-only reset.
        Require(resetReason == ESP_RST_POWERON, "RECOVERY_NOT_POWERON_RESET");
        // This is the ONLY post-interruption branch: never repair in place.
        // It checks prior completed EDLS generations despite a concurrent
        // large FileStorage::ReplaceFile operation having been interrupted.
        const bool preserved = BaselineIsValid(before);
        const auto largeSize = files.GetFileSize(CutTarget.Value);
        std::printf(
            "EDP_POWERLOSS|RECOVERY|prior_edls=%s|cut_file_status=%u|cut_file_bytes=%llu\n",
            preserved ? "PRESERVED" : "NOT_PRESERVED",
            static_cast<unsigned>(largeSize.Status),
            static_cast<unsigned long long>(largeSize.Size.RawValue)
        );
        std::fflush(stdout);
        if (!preserved) Stop("PRIOR_EDLS_NOT_PRESERVED");
        std::printf("EDP_POWERLOSS|PASS|PRIOR_EDLS_SURVIVED_RELAY_CYCLE\n");
        std::fflush(stdout);
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000U));
    }


    // Initial baseline may already exist if firmware flashed and auto-ran
    // before the host's Serial monitor became attached. Reuse only an
    // independently verified intact baseline, never destroy existing data.
    if (!BaselineIsValid(before)) {
        Require(
            before.Slots[0].Condition == L::EdlsSlotCondition::Empty &&
            before.Slots[1].Condition == L::EdlsSlotCondition::Empty,
            "NOT_FRESH_DEDICATED_TEST"
        );
        constexpr std::array<std::byte, 4U> payload{
            std::byte{0x83U}, std::byte{0x01U},
            std::byte{0x02U}, std::byte{0x03U}
        };
        std::array<std::byte, 64U> frameBytes{};
        std::size_t used{};
        Require(L::WriteEdlgFrame(
            L::EdlgCodec::Cbor, payload, frameBytes, used, 32U,
            ESPressio::Platform::Portable::Memory::ByteOperationsProvider{}
        ) == L::EdlgFrameWriteResult::Succeeded, "EDLG_SOURCE");

        const auto frame = std::span<const std::byte>(frameBytes.data(), used);
        const auto one = L::PublishEdlsSuccessorIntoEmptySlot<2U, 32U, 1024U>(
            files, Paths, DeviceIdentity, 1U, frame
        );
        Require(one.Status == L::EdlsSuccessorStatus::PublishedAndVerified &&
            one.Slot == 0U && one.Generation == 1U, "BASELINE_GEN1");
        const auto two = L::PublishEdlsSuccessorIntoEmptySlot<2U, 32U, 1024U>(
            files, Paths, DeviceIdentity, 2U, frame
        );
        Require(two.Status == L::EdlsSuccessorStatus::PublishedAndVerified &&
            two.Slot == 1U && two.Generation == 2U, "BASELINE_GEN2");
    }
    Files independent("/edpfs", bytes);
    before = L::InspectEdlsArchive<2U, 32U>(independent, Paths);
    Require(BaselineIsValid(before), "BASELINE_REOPEN");

    for (std::size_t i = 0U; i < PowerCutPayload.size(); ++i) {
        PowerCutPayload[i] = std::byte{static_cast<unsigned char>((i * 13U + 7U) & 0xFFU)};
    }
    // Permits guarded flash/verify helpers to regain Reset ownership
    // after their automatically applied reset; no NVS arm during this wait.
    std::printf("EDP_POWERLOSS|BASELINE_STABLE|wait_ms=12000\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(12000U));
    Require(nvs_set_u8(nvs, "armed", 1U) == ESP_OK &&
        nvs_commit(nvs) == ESP_OK, "NVS_PHASE_COMMIT");
    nvs_close(nvs);
    std::printf("EDP_POWERLOSS|BASELINE_READY|gen0=1|gen1=2|payload_bytes=%u\n",
        static_cast<unsigned>(PowerCutPayload.size()));
    std::printf("EDP_POWERLOSS|REPLACE_ARMED|wait_ms=200\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(200U));
    std::printf("EDP_POWERLOSS|REPLACE_ENTER\n");
    std::fflush(stdout);

    const auto replacement = files.ReplaceFile(
        CutTarget.Value,
        P::SourceBufferView{PowerCutPayload.data(), PowerCutPayload.size()}
    );
    std::printf("EDP_POWERLOSS|REPLACE_RETURN|status=%u\n",
        static_cast<unsigned>(replacement));
    std::fflush(stdout);
    // A normal return is not power-cut evidence. The host must actively
    // interrupt the supply during the in-progress call.
    Stop("NO_POWER_CUT_OBSERVED");
}
