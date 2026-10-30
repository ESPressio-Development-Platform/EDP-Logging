#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>

#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include <ESPressio_Serialisation.hpp>
#include <LittleFsVfsBinding.hpp>
#include <LittleFsVfsMount.hpp>
#include <NvsKeyValueStorage.hpp>
#include <clock/MonotonicClockProvider.hpp>
#include <logging/BoundedEdlsBatch.hpp>
#include <memory/ByteOperationsProvider.hpp>
#include <timing/EspTimerMonotonicTimebaseProvider.hpp>

#ifndef EDP_DEMO_DWELL_NS
#define EDP_DEMO_DWELL_NS 100000000LL
#endif

namespace ESPressio::Demos::ConfigurableBoundedEdlsBatch {


    namespace C = ESPressio::Clock;
    namespace CF = ESPressio::System::CompositionFramework;
    namespace E = ESPressio::Persistence::EspIdf;
    namespace L = ESPressio::Logging;
    namespace P = ESPressio::Persistence;
    namespace Serial = ESPressio::Serialisation;

    /// Finite outcome of Bootstrap wiring and the demonstrated operations.
    enum class Result : std::uint8_t {
        Completed = 0U,
        NativeInitialisationFailed = 1U,
        IdentityFailed = 2U,
        MountFailed = 3U,
        ArchiveFailed = 4U,
        ConfigurationFailed = 5U,
        EncodingFailed = 6U,
        AdmissionFailed = 7U,
        AcknowledgementFailed = 8U
    };

    /// Separate selectable binding for this example's finite archive.
    struct FileBinding final {};
    /// Separate namespace binding for this example's committed metadata.
    struct NvsBinding final {};

    using Bytes = ESPressio::Platform::Portable::Memory::ByteOperationsProvider;
    using Files = E::LittleFsConservativeFileStorage<FileBinding, Bytes>;
    using Ledger = E::NvsKeyValueStorage<NvsBinding, Bytes>;
    using Timebase = ESPressio::Platform::ESPIDF::Timing::EspTimerMonotonicTimebaseProvider;
    using Clock = C::MonotonicClockProvider<Timebase>;
    using Architecture = CF::Architecture<
        CF::Composition<ESPressio::Memory::Domain, Bytes>,
        CF::Composition<P::Domain, Files, Ledger>,
        CF::Composition<C::Domain, Timebase, Clock>>;
    static_assert(Architecture::IsValid);

    /// Four complete frames, with two retained segments and one physical spare.
    constexpr std::size_t MaximumRecords = 4U;
    /// Bounded framed-byte capacity; EDLS metadata is counted separately.
    constexpr std::size_t MaximumBodyBytes = 384U;
    /// Whole-segment scratch, disjoint from input and accumulator storage.
    constexpr std::size_t MaximumSegmentBytes =
        MaximumBodyBytes + L::EdlsHeaderBytes + L::EdlsSealBytes;
    /// Explicit application setting; zero is immediate, upper bound one second.
    constexpr std::int64_t DwellNanoseconds = EDP_DEMO_DWELL_NS;
    static_assert(DwellNanoseconds >= 0 && DwellNanoseconds <= 1000000000LL);

    using Archive = L::PowerLossAcknowledgedEdls<
        3U,
        2U,
        32U,
        MaximumSegmentBytes,
        Files,
        Ledger>;
    using Batch = L::BoundedEdlsBatch<
        MaximumRecords,
        MaximumBodyBytes,
        Archive,
        Clock,
        Bytes,
        1000000000LL>;

    /// Fixed provider-relative names; never enumerate or delete unrelated files.
    constexpr std::array<P::FilePathView, 3U> Paths{
        P::FilePathView::Validate("demo0").Value,
        P::FilePathView::Validate("demo1").Value,
        P::FilePathView::Validate("demo2").Value};
    /// Independent strict segment watermark.
    constexpr auto AckKey = P::KeyView::Validate("ack").Value;
    /// Durable demo boot counter; it is not reset by normal startup.
    constexpr auto IncarnationKey = P::KeyView::Validate("incarnation").Value;

    // Finite application Bootstrap and demonstration operations.

    /// Establishes demo identity using hardware MAC and durably advanced metadata.
    /// No library provider is declared here: Bootstrap consumes existing owners.
    /// Failure or exhaustion never exposes a new incarnation for record emission.
    [[nodiscard]] inline Result EstablishIdentity(
        Ledger& ledger,
        std::array<std::byte, 16U>& device,
        std::uint32_t& incarnation) noexcept {
        std::array<std::uint8_t, 6U> mac{};
        if (esp_read_mac(
            mac.data(),
            ESP_MAC_WIFI_STA
        ) != ESP_OK) {
            return Result::IdentityFailed;
        }
        constexpr std::array<std::uint8_t, 10U> prefix{
            0x45U,
            0x44U,
            0x50U,
            0x2DU,
            0x44U,
            0x45U,
            0x4DU,
            0x4FU,
            0x2DU,
            0x01U};
        for (std::size_t index = 0U; index < prefix.size(); ++index) {
            device[index] = static_cast<std::byte>(prefix[index]);
        }
        for (std::size_t index = 0U; index < mac.size(); ++index) {
            device[prefix.size() + index] = static_cast<std::byte>(mac[index]);
        }

        std::array<std::byte, 8U> encoded{};
        const auto previous = ledger.ReadValue(
            IncarnationKey,
            P::DestinationBufferView{encoded.data(), encoded.size()}
        );
        std::uint32_t old = 0U;
        if (previous.Status == P::KeyValueReadStatus::Succeeded) {
            if (!P::HasFact(
                previous.Facts,
                P::ReadFact::AvailableDataSizeIsKnown
            ) ||
                previous.AvailableDataSize.RawValue != previous.BytesTransferred ||
                !Serial::Deserialise<Serial::Cbor>(
                    std::span<const std::byte>(
                         encoded.data(),
                         previous.BytesTransferred
                     ),
                    old
                )
                     .IsSuccessful() ||
                old == 0U) {
                return Result::IdentityFailed;
            }
        } else if (previous.Status != P::KeyValueReadStatus::NotFound) {
            return Result::IdentityFailed;
        }
        if (old == std::numeric_limits<std::uint32_t>::max()) {
            return Result::IdentityFailed;
        }
        const std::uint32_t next = old + 1U;
        const auto serialised = Serial::Serialise<Serial::Cbor>(
            next,
            encoded
        );
        if (!serialised.IsSuccessful() ||
            ledger.StoreValue(
                IncarnationKey,
                P::SourceBufferView{encoded.data(), serialised.BytesWritten}
            ) != P::KeyValueStoreStatus::Succeeded) {
            return Result::IdentityFailed;
        }
        const auto confirmed = ledger.ReadValue(
            IncarnationKey,
            P::DestinationBufferView{encoded.data(), encoded.size()}
        );
        std::uint32_t observed = 0U;
        if (confirmed.Status != P::KeyValueReadStatus::Succeeded ||
            !P::HasFact(
                confirmed.Facts,
                P::ReadFact::AvailableDataSizeIsKnown
            ) ||
            confirmed.AvailableDataSize.RawValue != confirmed.BytesTransferred ||
            !Serial::Deserialise<Serial::Cbor>(
                std::span<const std::byte>(
                     encoded.data(),
                     confirmed.BytesTransferred
                 ),
                observed
            )
                 .IsSuccessful() ||
            observed != next) {
            return Result::IdentityFailed;
        }
        incarnation = next;
        return Result::Completed;
    }

    /// Encodes a small CBOR demonstration value using the existing codec owner.
    /// This container example is not a complete generated fifteen-field LogRecord.
    [[nodiscard]] inline Result OfferValue(Batch& batch, std::uint32_t value) noexcept {
        std::array<std::byte, 32U> payload{};
        std::array<std::byte, 52U> frame{};
        const auto encoded = Serial::Serialise<Serial::Cbor>(
            value,
            payload
        );
        std::size_t framed = 0U;
        if (!encoded.IsSuccessful() ||
            L::WriteEdlgFrame(
                L::EdlgCodec::Cbor,
                std::span<const std::byte>(
                    payload.data(),
                    encoded.BytesWritten
                ),
                frame,
                framed,
                payload.size()
            ) != L::EdlgFrameWriteResult::Succeeded) {
            return Result::EncodingFailed;
        }
        const auto offered = batch.Offer(
            std::span<const std::byte>(
                frame.data(),
                framed
            ));
        if (offered.Status != L::EdlsBatchStatus::Buffered ||
            offered.AcknowledgedRecords != 0U) {
            return Result::AdmissionFailed;
        }
        return Result::Completed;
    }

    /// Uses one already-open ledger; all dependent archive/mount objects end here.
    /// The outer Bootstrap closes the NVS handle for every operational result.
    [[nodiscard]] inline Result RunOpenLedger(Ledger& ledger, Bytes& bytes) noexcept {
        std::array<std::byte, 16U> device{};
        std::uint32_t incarnation = 0U;
        if (EstablishIdentity(
            ledger,
            device,
            incarnation
        ) != Result::Completed) {
            return Result::IdentityFailed;
        }
        E::LittleFsVfsMount mount{};
        if (mount.Mount({"/edplfs", "edplfs", false, false}) !=
            E::LittleFsMountStatus::Succeeded) {
            return Result::MountFailed;
        }
        Files files(
            "/edplfs",
            bytes
        );
        Archive archive(files, ledger, Paths, AckKey, device, incarnation);
        static std::array<std::byte, MaximumSegmentBytes> scratch{};
        const auto initial = archive.InitializeEmpty();
        if (initial.Status != L::EdlsAcknowledgedStatus::Initialized &&
            initial.Status != L::EdlsAcknowledgedStatus::Ready) {
            return Result::ArchiveFailed;
        }
        const auto recovered = archive.Recover(scratch);
        if (recovered.Status != L::EdlsAcknowledgedStatus::Ready &&
            recovered.Status != L::EdlsAcknowledgedStatus::UnacknowledgedCandidateRemoved &&
            recovered.Status != L::EdlsAcknowledgedStatus::CommittedOldestRetired) {
            return Result::ArchiveFailed;
        }
        Timebase timebase{};
        Clock clock(timebase);
        Batch batch(archive, clock, scratch);
        if (batch.ConfigureDwell(C::Duration::FromNanoseconds(1000000000LL)) !=
            L::EdlsBatchConfigurationStatus::Configured) {
            return Result::ConfigurationFailed;
        }
        const auto before = archive.Read().AcknowledgedGeneration;
        for (std::uint32_t value = 1U; value <= MaximumRecords; ++value) {
            const auto offered = OfferValue(
                batch,
                value
            );
            if (offered != Result::Completed) {
                return offered;
            }
        }
        if (archive.Read().AcknowledgedGeneration != before) {
            return Result::AcknowledgementFailed;
        }
        const auto full = batch.Service();
        if (full.AcknowledgedRecords != MaximumRecords ||
            full.Status != L::EdlsBatchStatus::DurablyAcknowledged) {
            return Result::AcknowledgementFailed;
        }
        if (batch.ConfigureDwell(C::Duration::FromNanoseconds(DwellNanoseconds)) !=
            L::EdlsBatchConfigurationStatus::Configured) {
            return Result::ConfigurationFailed;
        }
        const auto partial = OfferValue(
            batch,
            5U
        );
        if (partial != Result::Completed) {
            return partial;
        }
        const auto deadline = batch.NextFlushDeadline();
        if (!deadline.has_value()) {
            return Result::ConfigurationFailed;
        }
        // The demo Bootstrap yields while idle. A dedicated Logging coordinator
        // instead forwards this canonical deadline to ThreadContext::WaitUntil.
        while (clock.Now().Nanoseconds() < deadline->Nanoseconds()) {
            vTaskDelay(1U);
        }
        const auto idle = batch.Service();
        if (idle.AcknowledgedRecords != 1U ||
            idle.Status != L::EdlsBatchStatus::DurablyAcknowledged) {
            return Result::AcknowledgementFailed;
        }
        std::printf(
            "EDP_BATCH_DEMO|PASS|incarnation=%u|dwell_ns=%lld|full_records=4|idle_records=1|ack_segment=%llu\n",
            static_cast<unsigned>(incarnation),
            static_cast<long long>(DwellNanoseconds),
            static_cast<unsigned long long>(archive.Read().AcknowledgedGeneration)
        );
        std::fflush(stdout);
        return Result::Completed;
    }

    /// Runs once in ordinary Bootstrap context; it creates no worker or timer.
    /// Provision the dedicated LittleFS image explicitly before first use.
    [[nodiscard]] inline Result Run() noexcept {
        if (nvs_flash_init() != ESP_OK) {
            return Result::NativeInitialisationFailed;
        }
        Bytes bytes{};
        Ledger ledger(bytes);
        if (ledger.Open("batchdemo") != E::NvsOpenStatus::Succeeded) {
            return Result::NativeInitialisationFailed;
        }
        const auto result = RunOpenLedger(
            ledger,
            bytes
        );
        ledger.Close();
        return result;
    }

} // namespace ESPressio::Demos::ConfigurableBoundedEdlsBatch
