// Test-only native ESP32 app: real EDLG/EDLS + existing LittleFS FileStorage.
// The approved RPI400 GPIO17 controller, never firmware, attests supply cuts.
// Physical reserved-path lifecycle is 2 protected EDLS files + one empty spare.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <utility>

#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#if defined(EDP_TEST_EDLS_LOWSPACE)
#include <esp_littlefs.h>
#endif
#include <nvs.h>
#include <nvs_flash.h>

#include <LittleFsVfsBinding.hpp>
#include <LittleFsVfsMount.hpp>
#include <logging/EdlsArchiveRotation.hpp>
#include <memory/ByteOperationsProvider.hpp>

namespace P = ESPressio::Persistence;
namespace E = P::EspIdf;
namespace L = ESPressio::Logging;

using Bytes = ESPressio::Platform::Portable::Memory::ByteOperationsProvider;
struct TestOnlyLittleFsBinding final {};
using NativeStore = E::LittleFsConservativeFileStorage<TestOnlyLittleFsBinding, Bytes>;
#if defined(EDP_TEST_SEALED_WRITE_ACTIVITY_CUT)
// Test-only instrumentation of the existing provider. Only the unacknowledged
// spare is rewritten; protected predecessors are never passed to this loop.
/// Bootstrap arms only after both protected batches and the session flag commit.
static bool SealedWriteActivityArmed = false;
#if defined(EDP_WORKER_CAPACITY_CUT)
/// Test-only observation of the existing provider's actual result.
static P::FileReplaceStatus SealedLastReplace = P::FileReplaceStatus::Succeeded;
/// Serialized test telemetry; Bootstrap reads only after worker quiescence.
static std::uint32_t SealedReplaceCalls = 0U;
#endif
/// Application-only forwarding instrumentation over the existing concrete provider.
struct Store final {
    using ProviderDeclarationTag = NativeStore::ProviderDeclarationTag;
    using CompositionDomain = NativeStore::CompositionDomain;
    using CompositionOffers = NativeStore::CompositionOffers;
    /// Same conservative provider owns all native filesystem operations.
    NativeStore Actual;
    /// Borrows the same application mount root and stateless byte operations.
    Store(const char* root, Bytes& bytes) noexcept
        : Actual(
            root,
            bytes
        ) {}
    /// Reports readiness from the existing owner.
    [[nodiscard]] bool IsFileStorageReady() const noexcept {
        return Actual.IsFileStorageReady();
    }
    /// Delegates physical size observation without changing capability semantics.
    [[nodiscard]] P::FileSizeResult GetFileSize(P::FilePathView path) const noexcept {
        return Actual.GetFileSize(path);
    }
    /// Delegates bounded physical readback.
    [[nodiscard]] P::FileReadResult ReadFileAt(
        P::FilePathView path, P::StorageOffset offset, P::DestinationBufferView target) const noexcept {
        return Actual.ReadFileAt(
            path,
            offset,
            target
        );
    }
    /// Delegates the archive's existing retirement/removal operation.
    [[nodiscard]] P::FileRemoveStatus RemoveFile(P::FilePathView path) noexcept {
        return Actual.RemoveFile(path);
    }
    /// Intercepts only the armed spare publication for finite write-activity injection.
    [[nodiscard]] P::FileReplaceStatus ReplaceFile(
        P::FilePathView path, P::SourceBufferView source) noexcept {
        if (!SealedWriteActivityArmed) {
#if defined(EDP_WORKER_CAPACITY_CUT)
            ++SealedReplaceCalls;
            SealedLastReplace = Actual.ReplaceFile(
                path,
                source
            );
            return SealedLastReplace;
#else
            return Actual.ReplaceFile(
                path,
                source
            );
#endif
        }
        std::printf("EDP_BND|SEALED_WRITE_ACTIVITY|gen=3|nvs_ack=2\n");
        std::fflush(stdout);
        const auto started = xTaskGetTickCount();
        unsigned completed = 0U;
        while (xTaskGetTickCount() - started < pdMS_TO_TICKS(15000U)) {
            const auto result = Actual.ReplaceFile(
                path,
                source
            );
            if (result != P::FileReplaceStatus::Succeeded) {
                std::printf("EDP_BND|HALT|SEALED_WRITE_ACTIVITY_PROVIDER_FAILURE\n");
                std::fflush(stdout);
                for (;;) {
                    vTaskDelay(pdMS_TO_TICKS(1000U));
                }
            }
            ++completed;
            // Keep native watchdog service possible without manufacturing a
            // promise that UART/controller latency intersects a flash opcode.
            vTaskDelay(pdMS_TO_TICKS(1U));
        }
        std::printf(
            "EDP_BND|HALT|SEALED_WRITE_CUT_NOT_OBSERVED|completed=%u\n",
            completed
        );
        std::fflush(stdout);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }
};
#else
using Store = NativeStore;
#endif

#if defined(EDP_WORKER_CAPACITY_CUT)
/// Existing test padding owner, with its standalone small-record trial optional.
static void FillUnprotectedAndRefuse(Store& storage, bool standalone);
/// Shared existing test-padding reclamation; never removes protected archive paths.
static void RemoveUnprotectedPadding(Store& storage);
#endif

constexpr auto Path0 = P::FilePathView::Validate("slot0");
constexpr auto Path1 = P::FilePathView::Validate("slot1");
constexpr auto Path2 = P::FilePathView::Validate("slot2");
constexpr auto Seed = P::FilePathView::Validate("seed.txt");

static_assert(Path0.IsValuePresent && Path1.IsValuePresent &&
              Path2.IsValuePresent && Seed.IsValuePresent);

const std::array<P::FilePathView, 3U> Paths{
    Path0.Value, Path1.Value, Path2.Value};

std::array<std::byte, 16U> DeviceIdentifier{};

[[noreturn]] static void Halt(const char* reason) {
    std::printf(
        "EDP_EDLS|HALT|%s\n",
        reason
    );
    std::fflush(stdout);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

static void Require(bool condition, const char* why) {
    if (!condition) {
        Halt(why);
    }
}

static auto Frame() {
    std::array<std::byte, 64U> buffer{};
    constexpr std::array<std::byte, 4U> cbor{
        std::byte{0x83U},
        std::byte{0x01U},
        std::byte{0x02U},
        std::byte{0x03U}};
    std::size_t used = 0U;
    Require(
        L::WriteEdlgFrame(
            L::EdlgCodec::Cbor,
            cbor,
            std::span<std::byte>(buffer),
            used,
            32U
        ) == L::EdlgFrameWriteResult::Succeeded,
        "FRAME_BUILD"
    );
    return std::pair{buffer, used};
}

static auto Inspect(Store& store) {
    return L::InspectEdlsArchive<3U, 32U>(
        store,
        Paths
    );
}

// Exactly the two newest recorded generations, independently CRC checked.
static void ExpectPair(Store& store,
                       std::uint64_t older,
                       std::uint64_t newer) {
    const auto state = Inspect(store);
    Require(
        state.Condition == L::EdlsArchiveCondition::Unambiguous,
        "ARCHIVE_AMBIGUOUS"
    );
    Require(
        state.NextGeneration == newer + 1U,
        "GENERATION_UNEXPECTED"
    );
    std::size_t found = 0U;
    for (const auto& slot : state.Slots) {
        if (slot.Condition == L::EdlsSlotCondition::Empty) {
            continue;
        }
        Require(
            slot.Condition == L::EdlsSlotCondition::Clean,
            "FRAME_OR_HEADER_UNCLEAN"
        );
        Require(
            slot.VerifiedFrames == 1U,
            "EXPECTED_ONE_CRC_VALID_FRAME"
        );
        Require(
            slot.VerifiedPrefixBytes == 64U,
            "EDLS_ENVELOPE_SIZE_UNEXPECTED"
        );
        Require(
            slot.Header.DeviceIdentifier == DeviceIdentifier,
            "EDLS_DEVICE_ID_CHANGED"
        );
        Require(
            slot.Header.Generation == older ||
                    slot.Header.Generation == newer,
            "UNEXPECTED_RETAINED_GENERATION"
        );
        ++found;
    }
    Require(
        found == 2U,
        "EXPECTED_EXACT_TWO"
    );
}

#if defined(EDP_TEST_TYPED_ARCHIVE) || defined(EDP_TEST_TYPED_STRICT_ACK) || defined(EDP_TEST_TYPED_STRICT_LOAD) || defined(EDP_TEST_TYPED_STRICT_PROFILE)
#include "typed_archive_probe.hpp"
#if defined(EDP_TEST_SEALED_BATCH)
#include "sealed_batch_probe.hpp"
#if defined(EDP_TEST_BATCH_WORKER)
#include "batch_worker_probe.hpp"
#endif
#endif
#endif
#if defined(EDP_TEST_TYPED_STRICT_ACK)
#include "typed_strict_ack_probe.hpp"
#endif
#if defined(EDP_TEST_TYPED_STRICT_LOAD)
#include "typed_strict_load_probe.hpp"
#endif
#if defined(EDP_TEST_TYPED_STRICT_PROFILE)
#include "typed_strict_profile_probe.hpp"
#endif
#if defined(EDP_TEST_EDLS_MIDWRITE)
#include "midwrite_fault_probe.hpp"
#endif
#if defined(EDP_TEST_STRICT_ACK)
#include "strict_ack_probe.hpp"
#endif

#if defined(EDP_TEST_EDLS_LOWSPACE)
#include <cstdio>
#include <cstring>
#include <esp_littlefs.h>
static std::array<std::uint8_t, 8192U> UnprotectedPadding{};
// A complete 4096-byte canonical CBOR byte-string stresses a >4K real
// EDLS successor when the exhausted LittleFS partition reports <= one
// physical 4096-byte erase block available. Static to avoid task stack.
static std::array<std::byte, 4096U> FullCborPayload{};
static std::array<std::byte, 4116U> FullEdlgFrame{};
static std::array<std::byte, 4156U> FullArchiveScratch{};
static constexpr std::uint32_t MaximumPaddingFiles = 96U;

static void PressureUsage(const char* stage) {
    std::size_t total = 0U, used = 0U;
    Require(
        esp_littlefs_info(
            "edplfs",
            &total,
            &used
        ) == ESP_OK &&
                total == 0x90000U && used <= total,
        "PRESSURE_INFO"
    );
    std::printf(
        "EDP_EDLS|PRESSURE|stage=%s|total=%u|used=%u|free=%u\n",
        stage,
        static_cast<unsigned>(total),
        static_cast<unsigned>(used),
        static_cast<unsigned>(total - used)
    );
    std::fflush(stdout);
}
static auto PaddingPath(std::uint32_t index, char (&name)[16U]) {
    const int n = std::snprintf(
        name,
        sizeof(name),
        "pad%03u.bin",
        static_cast<unsigned>(index)
    );
    Require(
        n > 0 && n < static_cast<int>(sizeof(name)),
        "PADDING_NAME"
    );
    const auto parsed = P::FilePathView::Validate(
        name,
        static_cast<std::size_t>(n)
    );
    Require(
        parsed.Status == P::FilePathValidationStatus::Succeeded,
        "PADDING_PATH"
    );
    return parsed.Value;
}
static void VerifyLowspaceProtectedPair(Store& storage) {
    const auto inspected = Inspect(storage);
    for (std::size_t slot = 0U; slot < 2U; ++slot) {
        const auto& prior = inspected.Slots[slot];
        Require(
            prior.Condition == L::EdlsSlotCondition::Clean &&
                    prior.HasHeader && prior.Header.Generation == slot + 1U &&
                    prior.Header.DeviceIdentifier == DeviceIdentifier &&
                    prior.VerifiedFrames == 1U && prior.VerifiedPrefixBytes == 64U,
            "ACKED_EDLS_PROTECTED_WINDOW_DAMAGED"
        );
    }
}
static void FillUnprotectedAndRefuse(Store& storage, bool standalone = true) {
    // Protected real EDLS CRC files remain untouched while this disposable
    // test-only allocation pressure consumes the LittleFS physical reserve.
    UnprotectedPadding.fill(0xA5U);
    bool reachedExhaustion = false;
    for (std::uint32_t i = 0U; i < MaximumPaddingFiles; ++i) {
        char filename[16U]{};
        const auto path = PaddingPath(
            i,
            filename
        );
        const auto result = storage.ReplaceFile(
            path,
            P::SourceBufferView{UnprotectedPadding.data(), UnprotectedPadding.size()}
        );
        if (result != P::FileReplaceStatus::Succeeded) {
            std::printf(
                "EDP_EDLS|PADDING_REFUSED|index=%u|native=%u\n",
                static_cast<unsigned>(i),
                static_cast<unsigned>(result)
            );
            reachedExhaustion = true;
            break;
        }
        if (i % 8U == 7U)
            PressureUsage("filling");
    }
    Require(
        reachedExhaustion,
        "PHYSICAL_EXHAUSTION_NOT_REACHED"
    );
    PressureUsage("exhausted");
    if (!standalone) {
        return;
    }
    // CBOR definite-length byte-string 0x0FFD = 4093 data bytes,
    // total canonical payload 4096 bytes. This is an actual complete
    // EDLG CRC-protected frame with a physical EDLS length > 4096.
    FullCborPayload[0U] = std::byte{0x59U};
    FullCborPayload[1U] = std::byte{0x0FU};
    FullCborPayload[2U] = std::byte{0xFDU};
    FullCborPayload.fill(std::byte{0xA5U});
    FullCborPayload[0U] = std::byte{0x59U};
    FullCborPayload[1U] = std::byte{0x0FU};
    FullCborPayload[2U] = std::byte{0xFDU};
    std::size_t fullFrameSize = 0U;
    Require(
        L::WriteEdlgFrame(
            L::EdlgCodec::Cbor,
            FullCborPayload,
            FullEdlgFrame,
            fullFrameSize,
            4096U
        ) ==
                    L::EdlgFrameWriteResult::Succeeded &&
                fullFrameSize == 4116U,
        "LARGE_CBOR_FRAME_INVALID"
    );
    std::printf(
        "EDP_EDLS|PRESSURE_CANDIDATE|payload=4096|edlg=%u|edls=4156\n",
        static_cast<unsigned>(fullFrameSize)
    );
    std::fflush(stdout);
    const auto attempted = L::PublishEdlsProtectedRollingSuccessorWithScratch<
        3U,
        2U,
        4096U,
        4200U>(
            storage,
            Paths,
            DeviceIdentifier,
            3U,
            std::span<const std::byte>(
            FullEdlgFrame.data(),
            fullFrameSize
        ),
            FullArchiveScratch
        );
    std::printf(
        "EDP_EDLS|REFUSED_G3|status=%u|native=%u\n",
        static_cast<unsigned>(attempted.Status),
        static_cast<unsigned>(attempted.NativeWrite)
    );
    std::fflush(stdout);
    Require(
        attempted.Status != L::EdlsProtectedRollingStatus::PublishedAndRetired && attempted.Status != L::EdlsProtectedRollingStatus::PublishedWithinCapacity,
        "SUCCESSOR_UNEXPECTEDLY_ACCEPTED"
    );
    VerifyLowspaceProtectedPair(storage);
}
/// Removes only test-owned padding, shared by existing and full typed fixtures.
static void RemoveUnprotectedPadding(Store& storage) {
    for (std::uint32_t i = 0U; i < MaximumPaddingFiles; ++i) {
        char filename[16U]{};
        const auto removed = storage.RemoveFile(PaddingPath(
            i,
            filename
        ));
        Require(
            removed == P::FileRemoveStatus::Succeeded ||
                    removed == P::FileRemoveStatus::NotFound,
            "PADDING_RECLAIM"
        );
    }
}
static void ReclaimUnprotectedAfterVerifiedAck(Store& storage) {
    // TEST-ONLY. Caller has read independent committed NVS ack=2 and
    // revalidated BOTH protected records before deliberately discarding
    // unacknowledged candidate g3. Not automatic production repair logic.
    VerifyLowspaceProtectedPair(storage);
    RemoveUnprotectedPadding(storage);
    const auto candidate = storage.RemoveFile(Path2.Value);
    Require(
        candidate == P::FileRemoveStatus::Succeeded ||
                candidate == P::FileRemoveStatus::NotFound,
        "UNACKED_CANDIDATE_RECLAIM"
    );
    PressureUsage("reclaimed");
    ExpectPair(
        storage,
        1U,
        2U
    );
}
#endif

extern "C" void app_main() {
    const auto reset = esp_reset_reason();
    std::printf(
        "EDP_BND|BOOT|reset=%u\n",
        static_cast<unsigned>(reset)
    );
    std::fflush(stdout);

    Require(
        nvs_flash_init() == ESP_OK,
        "NVS_INIT"
    );
    nvs_handle_t session{};
#if defined(EDP_TEST_TYPED_STRICT_ACK)
    Require(
        nvs_open(
            "edptyp27",
            NVS_READWRITE,
            &session
        ) == ESP_OK,
        "NVS_SESSION"
    );
#elif defined(EDP_TEST_TYPED_STRICT_PROFILE)
    Require(
        nvs_open(
            "edpprof28",
            NVS_READWRITE,
            &session
        ) == ESP_OK,
        "NVS_SESSION"
    );
#elif defined(EDP_TEST_TYPED_STRICT_LOAD)
    Require(
        nvs_open(
            "edpload28",
            NVS_READWRITE,
            &session
        ) == ESP_OK,
        "NVS_SESSION"
    );
#elif defined(EDP_TEST_STRICT_ACK)
    Require(
        nvs_open(
            "edpstrict28",
            NVS_READWRITE,
            &session
        ) == ESP_OK,
        "NVS_SESSION"
    );
#else
    Require(
        nvs_open(
            "edpedls26",
            NVS_READWRITE,
            &session
        ) == ESP_OK,
        "NVS_SESSION"
    );
#endif
    std::uint8_t armed = 0U;
    const auto phase = nvs_get_u8(
        session,
        "armed",
        &armed
    );
    Require(
        phase == ESP_OK || phase == ESP_ERR_NVS_NOT_FOUND,
        "NVS_PHASE"
    );

    E::LittleFsVfsMount mount{};
    Require(
        mount.Mount({"/edplfs", "edplfs", false, false}) ==
                E::LittleFsMountStatus::Succeeded,
        "LITTLEFS_MOUNT_NO_REPAIR"
    );
    static Bytes byteOps{};
    Store storage(
        "/edplfs",
        byteOps
    );
    Require(
        storage.IsFileStorageReady(),
        "FILESTORAGE_NOT_READY"
    );
    std::printf(
        "EDP_EDLS|RESOURCE|phase=after_mount|free_heap=%u|min_heap=%u|main_stack_hwm=%u\n",
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(esp_get_minimum_free_heap_size()),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
    );
    std::fflush(stdout);

    std::array<std::uint8_t, 4U> seed{};
    const auto seeded = storage.ReadFileAt(
        Seed.Value,
        P::StorageOffset{},
        P::DestinationBufferView{seed.data(), seed.size()}
    );
    Require(
        seeded.Status == P::FileReadStatus::Succeeded &&
                seeded.BytesTransferred == 4U &&
                seed[0U] == 'E' && seed[1U] == 'D' && seed[2U] == 'P' && seed[3U] == '1',
        "TEST_PARTITION_SEED_INVALID"
    );

    DeviceIdentifier.fill(std::byte{0xA6U});
#if defined(EDP_TEST_BATCH_WORKER)
    BatchWorkerHIL::Run(
        storage,
        byteOps,
        session,
        phase
    );
#endif
#if defined(EDP_TEST_SEALED_BATCH)
    RunSealedBatchHIL(
        storage,
        byteOps,
        reset,
        session,
        phase,
        armed
    );
#endif
#if defined(EDP_TEST_TYPED_STRICT_LOAD)
    nvs_close(session);
    RunTypedStrictLoadHIL(
        storage,
        byteOps,
        phase
    );
#endif
#if defined(EDP_TEST_TYPED_STRICT_PROFILE)
    nvs_close(session);
    RunTypedStrictProfileHIL(
        storage,
        byteOps,
        phase
    );
#endif
#if defined(EDP_TEST_TYPED_STRICT_ACK)
    ExecuteTypedStrictAckHIL(
        storage,
        byteOps,
        reset,
        session,
        phase,
        armed
    );
#endif
#if defined(EDP_TEST_STRICT_ACK)
    ExecuteStrictNvsRecoveryHIL(
        storage,
        byteOps,
        reset,
        session,
        phase,
        armed
    );
#endif
#if defined(EDP_TEST_EDLS_MIDWRITE)
    ExecuteMidwriteFaultProbe(
        storage,
        reset,
        session,
        phase,
        armed
    );
#endif
#if defined(EDP_TEST_TYPED_ARCHIVE)
    ExecuteTypedArchiveHIL(
        storage,
        reset,
        session,
        phase,
        armed
    );
#endif
    const auto [frameBytes, frameCount] = Frame();
    const auto encoded = std::span<const std::byte>(
        frameBytes.data(),
        frameCount
    );

    if (phase == ESP_OK && armed == 1U) {
        std::printf(
            "EDP_BND|RESET_OBSERVED|reason=%u|por=%u\n",
            static_cast<unsigned>(reset),
            reset == ESP_RST_POWERON ? 1U : 0U
        );
        std::fflush(stdout);
#if defined(EDP_TEST_EDLS_LOWSPACE)
        VerifyLowspaceProtectedPair(storage);
        std::uint8_t lastAck = 0U;
        Require(
            nvs_get_u8(
                session,
                "ack",
                &lastAck
            ) == ESP_OK &&
                    lastAck == 2U,
            "NVS_PROTECTED_ACK_MISMATCH"
        );
        ReclaimUnprotectedAfterVerifiedAck(storage);
        const auto first = L::PublishEdlsProtectedRollingSuccessor<
            3U,
            2U,
            32U,
            1024U>(
                storage,
                Paths,
                DeviceIdentifier,
                3U,
                encoded
            );
        Require(
            first.Status == L::EdlsProtectedRollingStatus::PublishedAndRetired &&
                    first.Generation == 3U,
            "PRESSURE_RECOVERY_NEW_G3"
        );
        ExpectPair(
            storage,
            2U,
            3U
        );
        std::printf("EDP_BND|LOWSPACE_ROLLED|ack=3|window=2,3|retired=1\n");
        std::fflush(stdout);
        const auto second = L::PublishEdlsProtectedRollingSuccessor<
            3U,
            2U,
            32U,
            1024U>(
                storage,
                Paths,
                DeviceIdentifier,
                4U,
                encoded
            );
        Require(
            second.Status == L::EdlsProtectedRollingStatus::PublishedAndRetired &&
                    second.Generation == 4U,
            "PRESSURE_RECOVERY_NEW_G4"
        );
        ExpectPair(
            storage,
            3U,
            4U
        );
        std::printf("EDP_EDLS|REAL_LOWSPACE_ROLLED|window=3,4|CRC=PASS\n");
        std::printf("EDP_BND|RECOVERED|newest=4|older=3|valid=2|contiguous=1\n");
        std::printf("EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT\n");
        std::fflush(stdout);
        for (;;)
            vTaskDelay(pdMS_TO_TICKS(1000U));
#else
        // The complete successor g3 was written before this power-cut,
        // but no physical oldest was retired. Verify all three here.
        const auto found = Inspect(storage);
        Require(
            found.Condition == L::EdlsArchiveCondition::Unambiguous,
            "REPLAY_THREE_NOT_CLEAN"
        );
        Require(
            found.NextGeneration == 4U,
            "REPLAY_G3_NOT_COMMITTED"
        );
        std::size_t count = 0U;
        for (const auto& slot : found.Slots) {
            Require(
                slot.Condition == L::EdlsSlotCondition::Clean &&
                        slot.VerifiedFrames == 1U &&
                        slot.VerifiedPrefixBytes == 64U &&
                        slot.Header.DeviceIdentifier == DeviceIdentifier,
                "REPLAY_N_PLUS_ONE_CRC_FAILED"
            );
            ++count;
        }
        Require(
            count == 3U,
            "REPLAY_N_PLUS_ONE_MISSING"
        );

        const auto recovered =
            L::FinishEdlsProtectedRollingRetirement<3U, 2U, 32U>(
                storage,
                Paths
            );
        Require(
            recovered.Status ==
                    L::EdlsProtectedRollingStatus::RecoveredPendingRetirement,
            "REBOOT_RETIRE_FAILED"
        );
        ExpectPair(
            storage,
            2U,
            3U
        );
        std::printf(
            "EDP_BND|LOWSPACE_ROLLED|ack=3|window=2,3|retired=1\n");
        std::fflush(stdout);

        const auto rolled =
            L::PublishEdlsProtectedRollingSuccessor<3U, 2U, 32U, 1024U>(
                storage,
                Paths,
                DeviceIdentifier,
                4U,
                encoded
            );
        Require(
            rolled.Status ==
                        L::EdlsProtectedRollingStatus::PublishedAndRetired &&
                    rolled.Generation == 4U &&
                    rolled.NativeRemove == P::FileRemoveStatus::Succeeded,
            "FINAL_FINITE_ROLL_FAILED"
        );
        ExpectPair(
            storage,
            3U,
            4U
        );

        std::printf(
            "EDP_EDLS|RESOURCE|phase=rolled|free_heap=%u|min_heap=%u|main_stack_hwm=%u\n",
            static_cast<unsigned>(esp_get_free_heap_size()),
            static_cast<unsigned>(esp_get_minimum_free_heap_size()),
            static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
        );
        std::printf("EDP_EDLS|REAL_ROLLED|window=3,4|CRC=PASS\n");
        std::printf(
            "EDP_BND|RECOVERED|newest=4|older=3|valid=2|contiguous=1\n");
        std::printf(
            "EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT\n");
        std::printf(
            "EDP_EDLS|RESOURCE|free_heap=%u|min_free=%u\n",
            static_cast<unsigned>(esp_get_free_heap_size()),
            static_cast<unsigned>(esp_get_minimum_free_heap_size())
        );
        std::fflush(stdout);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
#endif
    }

    Require(
        phase == ESP_ERR_NVS_NOT_FOUND,
        "UNEXPECTED_PRIOR_SESSION"
    );
    for (std::uint32_t generation = 1U; generation <= 2U; ++generation) {
        const auto committed =
            L::PublishEdlsProtectedRollingSuccessor<3U, 2U, 32U, 1024U>(
                storage,
                Paths,
                DeviceIdentifier,
                generation,
                encoded
            );
        Require(
            committed.Status ==
                        L::EdlsProtectedRollingStatus::PublishedWithinCapacity &&
                    committed.Generation == generation,
            "BASELINE_IMMUTABLE_EDLS_FAILED"
        );
    }
    ExpectPair(
        storage,
        1U,
        2U
    );
    std::printf(
        "EDP_EDLS|RESOURCE|phase=protected_baseline|free_heap=%u|min_heap=%u|main_stack_hwm=%u\n",
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(esp_get_minimum_free_heap_size()),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
    );
    std::printf("EDP_EDLS|BASELINE_READY|window=1,2|wait_ms=12000\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(12000U));
    Require(
        nvs_set_u8(
            session,
            "armed",
            1U
        ) == ESP_OK &&
                nvs_commit(session) == ESP_OK,
        "NVS_ARM"
    );
#if defined(EDP_TEST_EDLS_LOWSPACE)
    Require(
        nvs_set_u8(
            session,
            "ack",
            2U
        ) == ESP_OK &&
                nvs_commit(session) == ESP_OK,
        "NVS_PROTECTED_ACK2"
    );
#endif
    nvs_close(session);
#if defined(EDP_TEST_EDLS_LOWSPACE)
    FillUnprotectedAndRefuse(storage);
    std::printf("EDP_BND|ACK|gen=2\n");
    std::printf("EDP_BND|LOWSPACE_SAFE|protected=1,2|last_ack=2|pads=1|no_ack_3=1\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(15000U));
    Halt("LOWSPACE_HARD_POWER_CUT_NOT_OBSERVED");
#else
    // Explicit COMPLETE but NOT YET retired successor. Host VDD cut
    // occurs on the marker below, between checked EDLS logical phases.
    const auto candidate =
        L::PublishEdlsSuccessorIntoEmptySlot<3U, 32U, 1024U>(
            storage,
            Paths,
            DeviceIdentifier,
            3U,
            encoded
        );
    Require(
        candidate.Status ==
                    L::EdlsSuccessorStatus::PublishedAndVerified &&
                candidate.Generation == 3U,
        "IMMUTABLE_G3_CANDIDATE_FAILED"
    );
    const auto all = Inspect(storage);
    Require(
        all.Condition == L::EdlsArchiveCondition::Unambiguous &&
                all.NextGeneration == 4U,
        "COMPLETE_N_PLUS_ONE_UNCONFIRMED"
    );

    std::printf("EDP_BND|ACK|gen=2\n");
    std::printf(
        "EDP_BND|LOWSPACE_SAFE|protected=1,2|last_ack=2|pads=0|no_ack_3=1\n");
    std::printf(
        "EDP_EDLS|RESOURCE|phase=pending_retirement|free_heap=%u|min_heap=%u|main_stack_hwm=%u\n",
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(esp_get_minimum_free_heap_size()),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
    );
    std::printf("EDP_EDLS|COMPLETE_SUCCESSOR_PENDING_RETIRE|gen=3\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(15000U));
    Halt("HARD_POWER_CUT_NOT_OBSERVED");
#endif
}
