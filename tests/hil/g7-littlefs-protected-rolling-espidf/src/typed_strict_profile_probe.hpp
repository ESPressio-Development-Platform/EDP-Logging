#pragma once
// Test-only bounded on-target retention, NVS flash update and latency load.
// Does not create a production worker/provider or claim flash wear lifetime.
#include <algorithm>
#include <array>
#include <cstdint>
#include <esp_timer.h>
#include <esp_littlefs.h>
#include <NvsKeyValueStorage.hpp>
#include <logging/EdlsPowerLossAcknowledgement.hpp>
#include "edls_provider_timing.hpp"

namespace {
struct TypedProfileNvsBinding final {};
using TypedProfileNvs=E::NvsKeyValueStorage<TypedProfileNvsBinding,Bytes>;
using ProfileStore=ProfileFileStorage<Store>;
using ProfileNvs=ProfileKeyValue<TypedProfileNvs>;
using TypedProfileArchive=L::PowerLossAcknowledgedEdls<
    3U,2U,1024U,1200U,ProfileStore,ProfileNvs>;
constexpr auto TypedProfileAckKey=P::KeyView::Validate("edlsack");
static_assert(TypedProfileAckKey.IsValuePresent);
static constexpr std::uint32_t TypedProfileGenerations=256U;
static std::array<std::uint32_t,TypedProfileGenerations> TypedProfileDurations{};

[[noreturn]] static void RunTypedStrictProfileHIL(Store& storage,Bytes& bytes,
    esp_err_t phase) {
    Require(phase==ESP_ERR_NVS_NOT_FOUND,"PROFILE_PREEXISTING_TEST_PHASE");
    TypedProfileNvs ledger(bytes);
    Require(ledger.Open("profack")==E::NvsOpenStatus::Succeeded,
        "PROFILE_NVS_OPEN");
    ProfileStore timedStorage(storage);
    ProfileNvs timedLedger(ledger);
    TypedProfileArchive archive(timedStorage,timedLedger,Paths,
        TypedProfileAckKey.Value,DeviceIdentifier,7U);
    Require(archive.InitializeEmpty().Status==
        L::EdlsAcknowledgedStatus::Initialized,"PROFILE_ACK_GENESIS");

    ProfileLog.Reset();
    std::uint32_t minimumFreeHeap=UINT32_MAX;
    std::uint32_t minimumTaskStack=UINT32_MAX;
    std::size_t maximumLittleFsUsed=0U;
    const auto totalStarted=esp_timer_get_time();
    for(std::uint32_t generation=1U;
        generation<=TypedProfileGenerations;++generation) {
        PrepareTypedRecord(generation);
        Require(L::ProjectGeneratedLogRecord<
            TypedSchema,L::AttemptSequenceWidth::Bits16,
            S::Identity::RuntimeIncarnationId,C::ClockEra>(
            TypedSource,TypedWire,TypedPositional).IsSuccessful(),
            "PROFILE_OWNER_TYPED_PROJECTION");
        const auto result=Serial::Serialise<
            Serial::Cbor,Serial::RootProfile::TypedEnvelope>(
            TypedWire,TypedCbor);
        Require(result.IsSuccessful() && result.BytesWritten<=1024U,
            "PROFILE_TYPED_CBOR");
        std::size_t framed=0U;
        Require(L::WriteEdlgFrame(L::EdlgCodec::Cbor,
            std::span<const std::byte>(TypedCbor.data(),result.BytesWritten),
            TypedEdlg,framed,1024U)==L::EdlgFrameWriteResult::Succeeded,
            "PROFILE_OWNER_EDLG_CRC");

        ProfileLog.Enabled=true;
        const auto started=esp_timer_get_time();
        const auto admitted=archive.Publish(
            std::span<const std::byte>(TypedEdlg.data(),framed),
            TypedArchiveScratch);
        const auto elapsed=esp_timer_get_time()-started;
        ProfileLog.Enabled=false;
        Require(elapsed>=0 && elapsed<10000000LL,"PROFILE_OPERATION_TIME_INVALID");
        TypedProfileDurations[generation-1U]=
            static_cast<std::uint32_t>(elapsed);
        ProfileLog.FinishGeneration(generation);
        Require(admitted.AcknowledgedGeneration==generation &&
            (generation<=2U
                ? admitted.Status==L::EdlsAcknowledgedStatus::AcknowledgedWithinCapacity
                : admitted.Status==L::EdlsAcknowledgedStatus::AcknowledgedAndRetired),
            "PROFILE_ACK_OR_RETENTION");
        if(generation==1U || generation%16U==0U) {
            const auto durable=archive.Read();
            Require(durable.Status==L::EdlsAcknowledgedStatus::Ready &&
                durable.AcknowledgedGeneration==generation &&
                durable.AcknowledgedIncarnation==7U,
                "PROFILE_NVS_WATERMARK_READBACK");
            if(generation>=2U)
                VerifyTypedArchive(storage,generation-1U,generation);
            std::size_t total=0U,used=0U;
            Require(esp_littlefs_info("edplfs",&total,&used)==ESP_OK &&
                total==0x90000U && used<=total,"PROFILE_LITTLEFS_CAPACITY");
            maximumLittleFsUsed=std::max(maximumLittleFsUsed,used);
            const auto freeHeap=esp_get_free_heap_size();
            const auto taskStack=uxTaskGetStackHighWaterMark(nullptr);
            minimumFreeHeap=std::min(minimumFreeHeap,freeHeap);
            minimumTaskStack=std::min(minimumTaskStack,static_cast<std::uint32_t>(taskStack));
            std::printf(
                "EDP_PROFILE|PROGRESS|generation=%u|used_fs=%u|free_heap=%u|stack_hwm=%u\n",
                generation,static_cast<unsigned>(used),freeHeap,taskStack);
            std::fflush(stdout);
        }
    }
    const auto elapsedTotal=esp_timer_get_time()-totalStarted;
    std::sort(TypedProfileDurations.begin(),TypedProfileDurations.end());
    const auto p50=TypedProfileDurations[TypedProfileGenerations/2U];
    const auto p95=TypedProfileDurations[(TypedProfileGenerations*95U)/100U];
    const auto p99=TypedProfileDurations[(TypedProfileGenerations*99U)/100U];
    const auto max=TypedProfileDurations.back();
    VerifyTypedArchive(storage,255U,256U);
    Require(archive.Read().AcknowledgedGeneration==256U,
        "PROFILE_FINAL_NVS_ACK_MISMATCH");
    std::uint64_t providerTotal=0U;
    for(std::size_t op=0U;op<ProfileOpCount;++op) {
        const auto& stat=ProfileLog.Ops[op];
        std::array<std::uint32_t,TypedProfileGenerations> perGeneration{};
        std::uint64_t previousCount=0U,previousUs=0U;
        for(std::size_t i=0U;i<TypedProfileGenerations;++i) {
            const auto current=ProfileLog.Samples[i][op];
            Require(current.Count>=previousCount &&
                current.TotalUs>=previousUs,"PROFILE_COUNTER_ROLLOVER");
            const auto delta=current.TotalUs-previousUs;
            Require(delta<=UINT32_MAX,"PROFILE_DELTA_INVALID");
            perGeneration[i]=static_cast<std::uint32_t>(delta);
            previousCount=current.Count;
            previousUs=current.TotalUs;
        }
        providerTotal+=stat.TimeUs;
        std::sort(perGeneration.begin(),perGeneration.end());
        std::printf(
            "EDP_PROFILE|OP|name=%s|calls=%llu|total_us=%llu"
            "|max_call_us=%llu|p50_generation_us=%u"
            "|p99_generation_us=%u\n",
            ProfileNames[op],
            static_cast<unsigned long long>(stat.Count),
            static_cast<unsigned long long>(stat.TimeUs),
            static_cast<unsigned long long>(stat.MaxUs),
            perGeneration[TypedProfileGenerations/2U],
            perGeneration[(TypedProfileGenerations*99U)/100U]);
    }
    Require(providerTotal<=static_cast<std::uint64_t>(elapsedTotal),
        "PROFILE_PROVIDER_TIME_EXCEEDS_WALL");
    std::printf(
        "EDP_PROFILE|ATTRIBUTION|provider_total_us=%llu"
        "|other_us=%llu|provider_percent_x100=%llu"
        "|timer_errors=%u\n",
        static_cast<unsigned long long>(providerTotal),
        static_cast<unsigned long long>(
            elapsedTotal-static_cast<std::int64_t>(providerTotal)),
        static_cast<unsigned long long>(
            providerTotal*10000U/static_cast<std::uint64_t>(elapsedTotal)),
        ProfileLog.ErrorCount);
    std::fflush(stdout);
    std::printf(
        "EDP_PROFILE|PASS|count=256|ack=256|window=255,256"
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
