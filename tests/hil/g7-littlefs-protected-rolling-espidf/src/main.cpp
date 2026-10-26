// Test-only native ESP32 app: real EDLG/EDLS + existing LittleFS FileStorage.
// The approved RPI400 GPIO17 controller, never firmware, attests supply cuts.
// Physical reserved-path lifecycle is 2 protected EDLS files + one empty spare.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <utility>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <memory/ByteOperationsProvider.hpp>
#include <LittleFsVfsMount.hpp>
#include <LittleFsVfsBinding.hpp>
#include <logging/EdlsArchiveRotation.hpp>

namespace P = ESPressio::Persistence;
namespace E = P::EspIdf;
namespace L = ESPressio::Logging;

using Bytes = ESPressio::Platform::Portable::Memory::ByteOperationsProvider;
struct TestOnlyLittleFsBinding final {};
using Store = E::LittleFsConservativeFileStorage<TestOnlyLittleFsBinding,Bytes>;

constexpr auto Path0 = P::FilePathView::Validate("slot0");
constexpr auto Path1 = P::FilePathView::Validate("slot1");
constexpr auto Path2 = P::FilePathView::Validate("slot2");
constexpr auto Seed = P::FilePathView::Validate("seed.txt");

static_assert(Path0.IsValuePresent && Path1.IsValuePresent &&
    Path2.IsValuePresent && Seed.IsValuePresent);

const std::array<P::FilePathView,3U> Paths{
    Path0.Value,Path1.Value,Path2.Value
};

std::array<std::byte,16U> DeviceIdentifier{};

[[noreturn]] static void Halt(const char* reason) {
    std::printf("EDP_EDLS|HALT|%s\n",reason);
    std::fflush(stdout);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

static void Require(bool condition,const char* why) {
    if (!condition) {
        Halt(why);
    }
}

static auto Frame() {
    std::array<std::byte,64U> buffer{};
    constexpr std::array<std::byte,4U> cbor{
        std::byte{0x83U},std::byte{0x01U},
        std::byte{0x02U},std::byte{0x03U}
    };
    std::size_t used=0U;
    Require(L::WriteEdlgFrame(
        L::EdlgCodec::Cbor,
        cbor,
        std::span<std::byte>(buffer),
        used,
        32U
    )==L::EdlgFrameWriteResult::Succeeded,"FRAME_BUILD");
    return std::pair{buffer,used};
}

static auto Inspect(Store& store) {
    return L::InspectEdlsArchive<3U,32U>(store,Paths);
}

// Exactly the two newest recorded generations, independently CRC checked.
static void ExpectPair(Store& store,
                       std::uint64_t older,
                       std::uint64_t newer) {
    const auto state=Inspect(store);
    Require(state.Condition==L::EdlsArchiveCondition::Unambiguous,
        "ARCHIVE_AMBIGUOUS");
    Require(state.NextGeneration==newer+1U,"GENERATION_UNEXPECTED");
    std::size_t found=0U;
    for (const auto& slot:state.Slots) {
        if (slot.Condition==L::EdlsSlotCondition::Empty) {
            continue;
        }
        Require(slot.Condition==L::EdlsSlotCondition::Clean,
            "FRAME_OR_HEADER_UNCLEAN");
        Require(slot.VerifiedFrames==1U,
            "EXPECTED_ONE_CRC_VALID_FRAME");
        Require(slot.VerifiedPrefixBytes==64U,
            "EDLS_ENVELOPE_SIZE_UNEXPECTED");
        Require(slot.Header.DeviceIdentifier==DeviceIdentifier,
            "EDLS_DEVICE_ID_CHANGED");
        Require(slot.Header.Generation==older ||
            slot.Header.Generation==newer,"UNEXPECTED_RETAINED_GENERATION");
        ++found;
    }
    Require(found==2U,"EXPECTED_EXACT_TWO");
}

extern "C" void app_main() {
    const auto reset=esp_reset_reason();
    std::printf("EDP_BND|BOOT|reset=%u\n",static_cast<unsigned>(reset));
    std::fflush(stdout);

    Require(nvs_flash_init()==ESP_OK,"NVS_INIT");
    nvs_handle_t session{};
    Require(nvs_open("edpedls26",NVS_READWRITE,&session)==ESP_OK,
        "NVS_SESSION");
    std::uint8_t armed=0U;
    const auto phase=nvs_get_u8(session,"armed",&armed);
    Require(phase==ESP_OK || phase==ESP_ERR_NVS_NOT_FOUND,
        "NVS_PHASE");

    E::LittleFsVfsMount mount{};
    Require(mount.Mount({"/edplfs","edplfs",false,false})==
        E::LittleFsMountStatus::Succeeded,
        "LITTLEFS_MOUNT_NO_REPAIR");
    static Bytes byteOps{};
    Store storage("/edplfs",byteOps);
    Require(storage.IsFileStorageReady(),"FILESTORAGE_NOT_READY");
    std::printf("EDP_EDLS|RESOURCE|phase=after_mount|free_heap=%u|min_heap=%u|main_stack_hwm=%u\n",
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(esp_get_minimum_free_heap_size()),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    std::fflush(stdout);

    std::array<std::uint8_t,4U> seed{};
    const auto seeded=storage.ReadFileAt(
        Seed.Value,
        P::StorageOffset{},
        P::DestinationBufferView{seed.data(),seed.size()}
    );
    Require(seeded.Status==P::FileReadStatus::Succeeded &&
        seeded.BytesTransferred==4U &&
        seed[0U]=='E'&&seed[1U]=='D'&&seed[2U]=='P'&&seed[3U]=='1',
        "TEST_PARTITION_SEED_INVALID");

    DeviceIdentifier.fill(std::byte{0xA6U});
    const auto [frameBytes,frameCount]=Frame();
    const auto encoded=std::span<const std::byte>(
        frameBytes.data(),frameCount
    );

    if (phase==ESP_OK && armed==1U) {
        std::printf("EDP_BND|RESET_OBSERVED|reason=%u|por=%u\n",
            static_cast<unsigned>(reset),
            reset==ESP_RST_POWERON?1U:0U);
        std::fflush(stdout);
        // The complete successor g3 was written before this power-cut,
        // but no physical oldest was retired. Verify all three here.
        const auto found=Inspect(storage);
        Require(found.Condition==L::EdlsArchiveCondition::Unambiguous,
            "REPLAY_THREE_NOT_CLEAN");
        Require(found.NextGeneration==4U,"REPLAY_G3_NOT_COMMITTED");
        std::size_t count=0U;
        for (const auto& slot:found.Slots) {
            Require(slot.Condition==L::EdlsSlotCondition::Clean &&
                slot.VerifiedFrames==1U &&
                slot.VerifiedPrefixBytes==64U &&
                slot.Header.DeviceIdentifier==DeviceIdentifier,
                "REPLAY_N_PLUS_ONE_CRC_FAILED");
            ++count;
        }
        Require(count==3U,"REPLAY_N_PLUS_ONE_MISSING");

        const auto recovered=
            L::FinishEdlsProtectedRollingRetirement<3U,2U,32U>(
                storage,Paths
            );
        Require(recovered.Status==
            L::EdlsProtectedRollingStatus::RecoveredPendingRetirement,
            "REBOOT_RETIRE_FAILED");
        ExpectPair(storage,2U,3U);
        std::printf(
            "EDP_BND|LOWSPACE_ROLLED|ack=3|window=2,3|retired=1\n"
        );
        std::fflush(stdout);

        const auto rolled=
            L::PublishEdlsProtectedRollingSuccessor<3U,2U,32U,1024U>(
                storage,Paths,DeviceIdentifier,4U,encoded
            );
        Require(rolled.Status==
            L::EdlsProtectedRollingStatus::PublishedAndRetired &&
            rolled.Generation==4U &&
            rolled.NativeRemove==P::FileRemoveStatus::Succeeded,
            "FINAL_FINITE_ROLL_FAILED");
        ExpectPair(storage,3U,4U);

        std::printf("EDP_EDLS|RESOURCE|phase=rolled|free_heap=%u|min_heap=%u|main_stack_hwm=%u\n",
            static_cast<unsigned>(esp_get_free_heap_size()),
            static_cast<unsigned>(esp_get_minimum_free_heap_size()),
            static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
        std::printf("EDP_EDLS|REAL_ROLLED|window=3,4|CRC=PASS\n");
        std::printf(
            "EDP_BND|RECOVERED|newest=4|older=3|valid=2|contiguous=1\n"
        );
        std::printf(
            "EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT\n"
        );
        std::printf("EDP_EDLS|RESOURCE|free_heap=%u|min_free=%u\n",
            static_cast<unsigned>(esp_get_free_heap_size()),
            static_cast<unsigned>(esp_get_minimum_free_heap_size()));
        std::fflush(stdout);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }

    Require(phase==ESP_ERR_NVS_NOT_FOUND,"UNEXPECTED_PRIOR_SESSION");
    for (std::uint32_t generation=1U;generation<=2U;++generation) {
        const auto committed=
            L::PublishEdlsProtectedRollingSuccessor<3U,2U,32U,1024U>(
                storage,Paths,DeviceIdentifier,generation,encoded
            );
        Require(committed.Status==
            L::EdlsProtectedRollingStatus::PublishedWithinCapacity &&
            committed.Generation==generation,
            "BASELINE_IMMUTABLE_EDLS_FAILED");
    }
    ExpectPair(storage,1U,2U);
    std::printf("EDP_EDLS|RESOURCE|phase=protected_baseline|free_heap=%u|min_heap=%u|main_stack_hwm=%u\n",
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(esp_get_minimum_free_heap_size()),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    std::printf("EDP_EDLS|BASELINE_READY|window=1,2|wait_ms=12000\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(12000U));
    Require(nvs_set_u8(session,"armed",1U)==ESP_OK &&
        nvs_commit(session)==ESP_OK,"NVS_ARM");
    nvs_close(session);

    // Explicit COMPLETE but NOT YET retired successor. Host VDD cut
    // occurs on the marker below, between checked EDLS logical phases.
    const auto candidate=
        L::PublishEdlsSuccessorIntoEmptySlot<3U,32U,1024U>(
            storage,Paths,DeviceIdentifier,3U,encoded
        );
    Require(candidate.Status==
        L::EdlsSuccessorStatus::PublishedAndVerified &&
        candidate.Generation==3U,
        "IMMUTABLE_G3_CANDIDATE_FAILED");
    const auto all=Inspect(storage);
    Require(all.Condition==L::EdlsArchiveCondition::Unambiguous &&
        all.NextGeneration==4U,
        "COMPLETE_N_PLUS_ONE_UNCONFIRMED");

    std::printf("EDP_BND|ACK|gen=2\n");
    std::printf(
        "EDP_BND|LOWSPACE_SAFE|protected=1,2|last_ack=2|pads=0|no_ack_3=1\n"
    );
    std::printf("EDP_EDLS|RESOURCE|phase=pending_retirement|free_heap=%u|min_heap=%u|main_stack_hwm=%u\n",
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(esp_get_minimum_free_heap_size()),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    std::printf("EDP_EDLS|COMPLETE_SUCCESSOR_PENDING_RETIRE|gen=3\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(15000U));
    Halt("HARD_POWER_CUT_NOT_OBSERVED");
}
