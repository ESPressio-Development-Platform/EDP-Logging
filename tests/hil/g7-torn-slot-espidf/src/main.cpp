// Physical G7 crash-window fixture: torn oldest slot, protected newest.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_spiffs.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <nvs.h>

#include <ESPressio_Persistence_ESP_IDF.hpp>
#include <logging/EdlgFrame.hpp>
#include <logging/EdlsArchiveInspection.hpp>
#include <logging/EdlsArchiveRotation.hpp>
#include <logging/EdlsSegment.hpp>
#include <VfsFileStorage.hpp>
#include <memory/ByteOperationsProvider.hpp>

namespace P = ESPressio::Persistence;
namespace L = ESPressio::Logging;
namespace Platform = ESPressio::Platform;
using Bytes = Platform::Portable::Memory::ByteOperationsProvider;
struct TornMount final {};
using Profile = P::EspIdf::VfsBindingProfile<
    P::RetentionLevel::Restart,
    P::TextCaseSensitivity::CaseSensitive,
    P::MediaRemovability::Fixed,
    32U, 32U, 131072U
>;
using Files = P::EspIdf::VfsFileStorage<TornMount, Profile, Bytes>;

constexpr auto S0 = P::FilePathView::Validate("power0");
constexpr auto S1 = P::FilePathView::Validate("power1");
static_assert(S0.IsValuePresent && S1.IsValuePresent);
const std::array<P::FilePathView, 2U> Paths{S0.Value, S1.Value};
std::array<std::byte, 16U> Identity{};

[[noreturn]] void Stop(const char* name) {
    std::printf("EDP_TORN|HALT|%s\n", name);
    std::fflush(stdout);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000U));
}

void Require(bool success, const char* name) {
    if (!success) Stop(name);
}

void PrintArchive(const char* name, const L::EdlsArchiveInspection<2U>& state) {
    std::printf(
        "EDP_TORN|%s|archive=%u|slot0=%u|gen0=%llu|slot1=%u|gen1=%llu|newest=%u\n",
        name,
        static_cast<unsigned>(state.Condition),
        static_cast<unsigned>(state.Slots[0].Condition),
        static_cast<unsigned long long>(state.Slots[0].Header.Generation),
        static_cast<unsigned>(state.Slots[1].Condition),
        static_cast<unsigned long long>(state.Slots[1].Header.Generation),
        static_cast<unsigned>(state.NewestSlot)
    );
    std::fflush(stdout);
}

extern "C" void app_main() {
    const auto reset = esp_reset_reason();
    std::printf("EDP_TORN|BOOT|torn-slot-v1\n");
    std::printf("EDP_TORN|RESET_REASON|value=%u\n", static_cast<unsigned>(reset));
    std::fflush(stdout);
    Require(nvs_flash_init() == ESP_OK, "NVS_INIT");
    nvs_handle_t nvs{};
    Require(nvs_open("edptorn26", NVS_READWRITE, &nvs) == ESP_OK, "NVS_OPEN");
    std::uint8_t armed{};
    const auto phase = nvs_get_u8(nvs, "armed", &armed);
    Require(phase == ESP_OK || phase == ESP_ERR_NVS_NOT_FOUND, "PHASE");

    const esp_vfs_spiffs_conf_t conf{
        .base_path = "/edpfs",
        .partition_label = "edpfs",
        .max_files = 4,
        .format_if_mount_failed = false
    };
    const auto mounted = esp_vfs_spiffs_register(&conf);
    if (mounted != ESP_OK) {
        std::printf("EDP_TORN|MOUNT_FAILURE|code=%d\n", static_cast<int>(mounted));
        std::fflush(stdout);
        Stop("MOUNT_FAILED_NO_FORMAT");
    }
    static Bytes bytes;
    Files storage("/edpfs", bytes);
    Require(storage.IsFileStorageReady(), "STORAGE_NOT_READY");
    Identity.fill(std::byte{0x9AU});

    const auto observed = L::InspectEdlsArchive<2U, 32U>(storage, Paths);
    PrintArchive("ARCHIVE", observed);
    if (phase == ESP_OK && armed == 1U) {
        // True relay interruption: firmware may not repair or format storage.
        Require(reset == ESP_RST_POWERON, "RECOVERY_NOT_POWERON");
        Require(
            observed.Slots[0].Condition == L::EdlsSlotCondition::DamagedTail &&
            observed.Slots[0].HasHeader &&
            observed.Slots[0].Header.Generation == 3U &&
            observed.Slots[0].VerifiedFrames == 0U,
            "OLD_SLOT_NOT_DAMAGED_TAIL"
        );
        Require(
            observed.Slots[1].Condition == L::EdlsSlotCondition::Clean &&
            observed.Slots[1].HasHeader &&
            observed.Slots[1].Header.Generation == 2U &&
            observed.Slots[1].VerifiedFrames == 1U,
            "PROTECTED_NEWEST_NOT_CLEAN"
        );

        constexpr std::array<std::byte, 4U> payload{
            std::byte{0x83U}, std::byte{0x01U},
            std::byte{0x02U}, std::byte{0x03U}
        };
        std::array<std::byte, 64U> encoded{};
        std::size_t encodedSize{};
        Require(L::WriteEdlgFrame(
            L::EdlgCodec::Cbor, payload, encoded, encodedSize, 32U
        ) == L::EdlgFrameWriteResult::Succeeded, "REPLAY_FRAME");
        // Application must never automatically destroy a damaged generation,
        // even though the older valid physical file remains available.
        const auto tried = L::PublishEdlsOldestEligibleSuccessor<2U, 32U, 1024U>(
            storage, Paths, Identity, 4U,
            std::span<const std::byte>(encoded.data(), encodedSize)
        );
        Require(
            tried.Status == L::EdlsRollingStatus::ArchiveAmbiguous,
            "ROTATION_DID_NOT_FAIL_CLOSED"
        );
        const auto verify = L::InspectEdlsArchive<2U, 32U>(storage, Paths);
        Require(
            verify.Slots[1].Condition == L::EdlsSlotCondition::Clean &&
            verify.Slots[1].Header.Generation == 2U &&
            verify.Slots[0].Condition == L::EdlsSlotCondition::DamagedTail,
            "REPLAY_MUTATED_PRIOR_GENERATION"
        );
        std::printf("EDP_TORN|PASS|PROTECTED_GENERATION_SURVIVED_AND_FAILED_CLOSED\n");
        std::fflush(stdout);
        Stop("TEST_COMPLETE");
    }

    // Reuse ONLY the independently verified two-slot baseline created by
    // the earlier G7 actual-write interruption test. Do not seed or format.
    Require(
        observed.Condition == L::EdlsArchiveCondition::Unambiguous &&
        observed.Slots[0].Condition == L::EdlsSlotCondition::Clean &&
        observed.Slots[0].Header.Generation == 1U &&
        observed.Slots[1].Condition == L::EdlsSlotCondition::Clean &&
        observed.Slots[1].Header.Generation == 2U,
        "BASELINE_NOT_VERIFIED"
    );
    // Deliberately persist a complete checksum-valid newer EDLS header
    // followed by an incomplete frame. This models a torn slot during a
    // multi-step successor publication on a real native SPIFFS substrate.
    std::array<std::byte, L::EdlsHeaderBytes + 8U> partial{};
    L::EdlsHeader next{};
    next.Slot = 0U;
    next.Generation = 3U;
    next.DeviceIdentifier = Identity;
    next.RuntimeIncarnation = 3U;
    Require(
        L::WriteEdlsHeader(next, partial) == L::EdlsHeaderStatus::Succeeded,
        "PARTIAL_HEADER"
    );
    constexpr std::array<std::byte, 8U> prefix{
        std::byte{'E'}, std::byte{'D'}, std::byte{'L'}, std::byte{'G'},
        std::byte{1U}, std::byte{0U}, std::byte{0U}, std::byte{0U}
    };
    for (std::size_t index = 0U; index < prefix.size(); ++index) {
        partial[L::EdlsHeaderBytes + index] = prefix[index];
    }

    std::printf("EDP_TORN|BASELINE_STABLE|wait_ms=12000\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(12000U));
    Require(
        nvs_set_u8(nvs, "armed", 1U) == ESP_OK &&
        nvs_commit(nvs) == ESP_OK,
        "PHASE_COMMIT"
    );
    nvs_close(nvs);
    const auto written = storage.ReplaceFile(
        S0.Value,
        P::SourceBufferView{partial.data(), partial.size()}
    );
    Require(written == P::FileReplaceStatus::Succeeded, "PARTIAL_WRITE");
    // Only after the real partial native FileStorage write is completed
    // does the host cut power; this is a crash BETWEEN logical writes,
    // not a claim that supply was lost inside ReplaceFile itself.
    std::printf("EDP_TORN|PARTIAL_PUBLISHED|slot=0|gen=3|bytes=%u\n",
        static_cast<unsigned>(partial.size()));
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(5000U));
    Stop("NO_RELAY_CUT");
}
