#pragma once
// Test-only bounded on-target retention, NVS flash update and latency load.
// Does not create a production worker/provider or claim flash wear lifetime.
#include <logging/EdlgFrameWriter.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <esp_timer.h>
#include <esp_littlefs.h>
#include <NvsKeyValueStorage.hpp>
#include <logging/EdlsPowerLossAcknowledgement.hpp>

namespace {
struct TypedLoadNvsBinding final {};
using TypedLoadNvs=E::NvsKeyValueStorage<TypedLoadNvsBinding,Bytes>;
using TypedLoadArchive=L::PowerLossAcknowledgedEdls<
    3U,2U,1024U,1200U,Store,TypedLoadNvs>;
constexpr auto TypedLoadAckKey=P::KeyView::Validate("edlsack");
static_assert(TypedLoadAckKey.IsValuePresent);
static constexpr std::uint32_t TypedLoadGenerations=256U;
static std::array<std::uint32_t,TypedLoadGenerations> TypedLoadDurations{};

[[noreturn]] static void RunTypedStrictLoadHIL(Store& storage,Bytes& bytes,
    esp_err_t phase) {
    Require(phase==ESP_ERR_NVS_NOT_FOUND,"LOAD_PREEXISTING_TEST_PHASE");
    TypedLoadNvs ledger(bytes);
    Require(ledger.Open("loadack")==E::NvsOpenStatus::Succeeded,
        "LOAD_NVS_OPEN");
    TypedLoadArchive archive(storage,ledger,Paths,TypedLoadAckKey.Value,
        DeviceIdentifier,7U);
    Require(archive.InitializeEmpty().Status==
        L::EdlsAcknowledgedStatus::Initialized,"LOAD_ACK_GENESIS");

    std::uint32_t minimumFreeHeap=UINT32_MAX;
    std::uint32_t minimumTaskStack=UINT32_MAX;
    std::size_t maximumLittleFsUsed=0U;
    const auto totalStarted=esp_timer_get_time();
    for(std::uint32_t generation=1U;
        generation<=TypedLoadGenerations;++generation) {
        PrepareTypedRecord(generation);
        Require(L::ProjectGeneratedLogRecord<
            TypedSchema,L::AttemptSequenceWidth::Bits16,
            S::Identity::RuntimeIncarnationId,C::ClockEra>(
            TypedSource,TypedWire,TypedPositional).IsSuccessful(),
            "LOAD_OWNER_TYPED_PROJECTION");
        const auto result=Serial::Serialise<
            Serial::Cbor,Serial::RootProfile::TypedEnvelope>(
            TypedWire,TypedCbor);
        Require(result.IsSuccessful() && result.BytesWritten<=1024U,
            "LOAD_TYPED_CBOR");
        std::size_t framed=0U;
        Require(L::WriteEdlgFrame(L::EdlgCodec::Cbor,
            std::span<const std::byte>(TypedCbor.data(),result.BytesWritten),
            TypedEdlg,framed,1024U,
            ESPressio::Platform::Portable::Memory::ByteOperationsProvider{})==L::EdlgFrameWriteResult::Succeeded,
            "LOAD_OWNER_EDLG_CRC");

        const auto started=esp_timer_get_time();
        const auto admitted=archive.Publish(
            std::span<const std::byte>(TypedEdlg.data(),framed),
            TypedArchiveScratch);
        const auto elapsed=esp_timer_get_time()-started;
        Require(elapsed>=0 && elapsed<10000000LL,"LOAD_OPERATION_TIME_INVALID");
        TypedLoadDurations[generation-1U]=
            static_cast<std::uint32_t>(elapsed);
        Require(admitted.AcknowledgedGeneration==generation &&
            (generation<=2U
                ? admitted.Status==L::EdlsAcknowledgedStatus::AcknowledgedWithinCapacity
                : admitted.Status==L::EdlsAcknowledgedStatus::AcknowledgedAndRetired),
            "LOAD_ACK_OR_RETENTION");
        if(generation==1U || generation%16U==0U) {
            const auto durable=archive.Read();
            Require(durable.Status==L::EdlsAcknowledgedStatus::Ready &&
                durable.AcknowledgedGeneration==generation &&
                durable.AcknowledgedIncarnation==7U,
                "LOAD_NVS_WATERMARK_READBACK");
            if(generation>=2U)
                VerifyTypedArchive(storage,generation-1U,generation);
            std::size_t total=0U,used=0U;
            Require(esp_littlefs_info("edplfs",&total,&used)==ESP_OK &&
                total==0x90000U && used<=total,"LOAD_LITTLEFS_CAPACITY");
            maximumLittleFsUsed=std::max(maximumLittleFsUsed,used);
            const auto freeHeap=esp_get_free_heap_size();
            const auto taskStack=uxTaskGetStackHighWaterMark(nullptr);
            minimumFreeHeap=std::min(minimumFreeHeap,freeHeap);
            minimumTaskStack=std::min(minimumTaskStack,static_cast<std::uint32_t>(taskStack));
            std::printf(
                "EDP_LOAD|PROGRESS|generation=%u|used_fs=%u|free_heap=%u|stack_hwm=%u\n",
                generation,static_cast<unsigned>(used),freeHeap,taskStack);
            std::fflush(stdout);
        }
    }
    const auto elapsedTotal=esp_timer_get_time()-totalStarted;
    std::sort(TypedLoadDurations.begin(),TypedLoadDurations.end());
    const auto p50=TypedLoadDurations[TypedLoadGenerations/2U];
    const auto p95=TypedLoadDurations[(TypedLoadGenerations*95U)/100U];
    const auto p99=TypedLoadDurations[(TypedLoadGenerations*99U)/100U];
    const auto max=TypedLoadDurations.back();
    VerifyTypedArchive(storage,255U,256U);
    Require(archive.Read().AcknowledgedGeneration==256U,
        "LOAD_FINAL_NVS_ACK_MISMATCH");
    std::printf(
        "EDP_LOAD|PASS|count=256|ack=256|window=255,256"
        "|elapsed_us=%lld|p50_us=%u|p95_us=%u|p99_us=%u|max_us=%u"
        "|min_free_heap=%u|min_main_stack_hwm=%u|max_fs_used=%u\n",
        static_cast<long long>(elapsedTotal),p50,p95,p99,max,
        minimumFreeHeap,minimumTaskStack,
        static_cast<unsigned>(maximumLittleFsUsed));
    std::fflush(stdout);
    ledger.Close();
    for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
}
} // test-only bounded native load
