#pragma once
// Test-only native Bootstrap. Production batching remains SDK-neutral.
#include <algorithm>
#include <bounded/Bytes.hpp>
#include <clock/MonotonicClockProvider.hpp>
#include <timing/EspTimerMonotonicTimebaseProvider.hpp>
#include <NvsKeyValueStorage.hpp>
#include <logging/BoundedEdlsBatch.hpp>

namespace {

    struct SealedNvsBinding final {};
    using SealedNvs = E::NvsKeyValueStorage<SealedNvsBinding, Bytes>;
    using SealedArchive = L::PowerLossAcknowledgedEdls<
        3U, 2U, 1024U, 4152U, Store, SealedNvs>;
    using NativeTimebase = ESPressio::Platform::ESPIDF::Timing::EspTimerMonotonicTimebaseProvider;
    using NativeClock = C::MonotonicClockProvider<NativeTimebase>;
    using SealedBatch = L::BoundedEdlsBatch<16U, 4096U, SealedArchive, NativeClock, Bytes>;
    constexpr auto SealedAckKey = P::KeyView::Validate("edlsack");
    static std::array<std::byte, 4152U> SealedScratch{};
    static std::array<std::byte, 4152U> SealedReadback{};
    static ESPressio::Bounded::Bytes<4096U, Bytes> UnacknowledgedFrames{};
    static std::array<std::uint64_t, 16U> AdmissionCoordinates{};
    static std::array<std::uint32_t, 256U> RecordLatencies{};

    /// Reuse the existing generated fifteen-field owner fixture and canonical codec.
    static std::size_t EncodeSealedRecord(std::uint32_t ticket) {
        PrepareTypedRecord(ticket);
        Require(L::ProjectGeneratedLogRecord<
            TypedSchema, L::AttemptSequenceWidth::Bits16,
            S::Identity::RuntimeIncarnationId, C::ClockEra>(
                TypedSource, TypedWire, TypedPositional).IsSuccessful(),
            "BATCH_TYPED_SOURCE_PROJECTION");
        const auto encoded = Serial::Serialise<
            Serial::Cbor, Serial::RootProfile::TypedEnvelope>(TypedWire, TypedCbor);
        Require(encoded.IsSuccessful(), "BATCH_TYPED_CBOR");
        std::size_t framed = 0U;
        Require(L::WriteEdlgFrame(
            L::EdlgCodec::Cbor,
            std::span<const std::byte>(TypedCbor.data(), encoded.BytesWritten),
            TypedEdlg, framed, 1024U) == L::EdlgFrameWriteResult::Succeeded,
            "BATCH_EDLG_FRAME");
        return framed;
    }

    /// Independently parse actual media and every original typed record in a segment.
    static void VerifySealedWindow(Store& store, std::uint32_t firstSegment,
                                   std::uint32_t lastSegment) {
        const auto history = L::InspectEdlsArchiveWithScratch<3U, 1024U>(
            store, Paths, SealedScratch);
        Require(history.Condition == L::EdlsArchiveCondition::Unambiguous &&
            history.NextGeneration == lastSegment + 1U, "BATCH_WINDOW_STATUS");
        std::size_t present = 0U;
        for (std::size_t slot = 0U; slot < Paths.size(); ++slot) {
            const auto& item = history.Slots[slot];
            if (item.Condition == L::EdlsSlotCondition::Empty) {
                continue;
            }
            Require(item.Condition == L::EdlsSlotCondition::Clean &&
                item.Header.Profile == L::EdlsContentProfile::SealedFrames &&
                item.VerifiedFrames == 16U &&
                item.Header.Generation >= firstSegment &&
                item.Header.Generation <= lastSegment &&
                item.Header.DeviceIdentifier == DeviceIdentifier &&
                item.Header.RuntimeIncarnation == 7U, "BATCH_WINDOW_CONTENT");
            const auto read = store.ReadFileAt(Paths[slot], P::StorageOffset{},
                P::DestinationBufferView{SealedReadback.data(), SealedReadback.size()});
            Require(read.Status == P::FileReadStatus::Succeeded &&
                read.BytesTransferred == item.VerifiedPrefixBytes, "BATCH_READBACK_SIZE");
            std::size_t position = L::EdlsHeaderBytes;
            const auto bodyEnd = read.BytesTransferred - L::EdlsSealBytes;
            const auto firstTicket = (item.Header.Generation - 1U) * 16U + 1U;
            for (std::uint32_t index = 0U; index < 16U; ++index) {
                L::EdlgFrameView frame{};
                Require(position < bodyEnd &&
                    L::ReadEdlgFrame(std::span<const std::byte>(
                        SealedReadback.data() + position, bodyEnd - position),
                        frame, 1024U) == L::EdlgFrameReadResult::Succeeded &&
                    frame.Codec == L::EdlgCodec::Cbor, "BATCH_RECORD_CRC");
                const auto decoded = Serial::Deserialise<
                    Serial::Cbor, Serial::RootProfile::TypedEnvelope>(
                        frame.Payload, TypedDecoded);
                const auto ticket = firstTicket + index;
                Require(decoded.IsSuccessful() &&
                    TypedDecoded.Attempt.Value() == ticket &&
                    TypedDecoded.Incarnation.Value() == 7U &&
                    TypedDecoded.Message.Domain == TypedSchema::SourceDomain &&
                    TypedDecoded.Message.SubDomain == TypedSchema::SourceSubDomain &&
                    TypedDecoded.Message.String == TypedSchema::SourceString &&
                    TypedDecoded.DiagnosticSchemaFingerprint ==
                        TypedSchema::OwningContract::DiagnosticSchemaFingerprint &&
                    std::get<0>(TypedDecoded.Arguments) == 0x01020304U &&
                    std::get<1>(TypedDecoded.Arguments) == 0x0102030405060708ULL,
                    "BATCH_ALL_FIFTEEN_FIELDS");
                const auto expectedBytes = EncodeSealedRecord(ticket);
                Require(expectedBytes == frame.Consumed &&
                    std::equal(TypedEdlg.begin(), TypedEdlg.begin() + expectedBytes,
                        SealedReadback.begin() + position),
                    "BATCH_EXACT_ORIGINAL_FIFTEEN_FIELDS");
                position += frame.Consumed;
            }
            Require(position == bodyEnd, "BATCH_NO_EXTRA_RECORDS");
            ++present;
        }
        Require(present == lastSegment - firstSegment + 1U, "BATCH_WINDOW_COUNT");
    }

    /// Offer one known full16-record batch; RAM admission is explicitly not ACK.
    static auto CommitSealedSixteen(SealedBatch& batch, SealedArchive& archive,
                                    std::uint32_t segment) {
        const auto oldAck = archive.Read().AcknowledgedGeneration;
        for (std::uint32_t index = 0U; index < 16U; ++index) {
            const auto bytes = EncodeSealedRecord((segment - 1U) * 16U + index + 1U);
            Require(batch.Offer(std::span<const std::byte>(TypedEdlg.data(), bytes)).Status ==
                L::EdlsBatchStatus::Buffered, "BATCH_RAM_ADMISSION");
        }
        Require(archive.Read().AcknowledgedGeneration == oldAck, "BATCH_NO_RAM_ACK");
        const auto result = batch.Service();
        Require(result.AcknowledgedRecords == 16U &&
            result.Archive.AcknowledgedGeneration == segment &&
            result.Status == L::EdlsBatchStatus::DurablyAcknowledged,
            "BATCH_STRICT_DURABLE_COMPLETION");
        return result;
    }

    /// Real NVS/LittleFS workload; same source schema and individual record frames.
    [[noreturn]] static void RunSealedBatchHIL(
        Store& store, Bytes& bytes, esp_reset_reason_t reset,
        nvs_handle_t session, esp_err_t phase, std::uint8_t armed) {
        SealedNvs ledger(bytes);
        Require(ledger.Open("batchack") == E::NvsOpenStatus::Succeeded, "BATCH_NVS_OPEN");
        SealedArchive archive(store, ledger, Paths, SealedAckKey.Value, DeviceIdentifier, 7U);
        static NativeTimebase timebase{};
        static NativeClock clock(timebase);
        static SealedBatch batch(archive, clock, SealedScratch);
        Require(batch.ConfigureDwell(C::Duration::FromNanoseconds(100000000)) ==
            L::EdlsBatchConfigurationStatus::Configured, "BATCH_CONFIGURABLE_DWELL");

#if defined(EDP_TEST_SEALED_BATCH_CUT)
        if (phase == ESP_OK && armed == 1U) {
            std::printf("EDP_BND|RESET_OBSERVED|reason=%u|por=%u\n",
                static_cast<unsigned>(reset), reset == ESP_RST_POWERON ? 1U : 0U);
            Require(archive.Read().AcknowledgedGeneration == 2U, "BATCH_COLD_ACK2");
            VerifySealedWindow(store, 1U, 3U); // complete candidate is not acknowledged
            Require(archive.Recover(SealedScratch).Status ==
                L::EdlsAcknowledgedStatus::UnacknowledgedCandidateRemoved,
                "BATCH_REMOVE_ONLY_UNACKED");
            VerifySealedWindow(store, 1U, 2U);
            CommitSealedSixteen(batch, archive, 3U);
            CommitSealedSixteen(batch, archive, 4U);
            VerifySealedWindow(store, 3U, 4U);
            std::printf("EDP_BND|RECOVERED|newest=4|older=3|valid=2|contiguous=1\n");
            std::printf("EDP_EDLS|TYPED_STRICT_ACK_REPLAY_VERIFIED|all_15_fields=1|ack=4|window=3,4\n");
            std::printf("EDP_BATCH|REPLAY|ack_segment=4|records_per_segment=16|first=33|last=64\n");
            std::printf("EDP_BND|REPLAY_VERIFIED|STRICT_TYPED15_NVS_ACK_AFTER_CUT\n");
            std::fflush(stdout);
            for (;;) { vTaskDelay(pdMS_TO_TICKS(1000U)); }
        }
#endif
        Require(phase == ESP_ERR_NVS_NOT_FOUND, "BATCH_PRIOR_TEST_PHASE");
        Require(archive.InitializeEmpty().Status == L::EdlsAcknowledgedStatus::Initialized,
            "BATCH_GENESIS_ACK0");
#if defined(EDP_TEST_SEALED_BATCH_CUT)
        for (std::uint32_t segment = 1U; segment <= 2U; ++segment) {
            CommitSealedSixteen(batch, archive, segment);
            std::printf("EDP_BND|ACK|gen=%u\n", segment);
        }
        VerifySealedWindow(store, 1U, 2U);
        Require(nvs_set_u8(session, "armed", 1U) == ESP_OK &&
            nvs_commit(session) == ESP_OK, "BATCH_CUT_ARM");
        nvs_close(session);
        for (std::uint32_t ticket = 33U; ticket <= 48U; ++ticket) {
            const auto framed = EncodeSealedRecord(ticket);
            Require(UnacknowledgedFrames.Append(
                reinterpret_cast<const std::uint8_t*>(TypedEdlg.data()), framed) ==
                ESPressio::Bounded::BytesAppendResult::Succeeded, "BATCH_CANDIDATE_BYTES");
        }
        const auto candidate = L::PublishEdlsSuccessorIntoEmptySlotWithScratch<
            3U, 1024U, 4152U, Store, 16U>(
                store, Paths, DeviceIdentifier, 7U,
                std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(UnacknowledgedFrames.Data()),
                    UnacknowledgedFrames.Size()), SealedScratch);
        Require(candidate.Status == L::EdlsSuccessorStatus::PublishedAndVerified &&
            candidate.Generation == 3U && archive.Read().AcknowledgedGeneration == 2U,
            "BATCH_COMPLETE_UNACKED_CANDIDATE");
        VerifySealedWindow(store, 1U, 3U);
        std::printf("EDP_BND|TYPED_ACK_GAP|gen=3|nvs_ack=2|candidate_complete=1\n");
        std::fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(15000U));
        Halt("BATCH_HARD_POWER_CUT_NOT_OBSERVED");
#else
        (void)reset;
        (void)armed;
        nvs_close(session);
        std::uint32_t completed = 0U;
        std::uint32_t minimumHeap = UINT32_MAX;
        std::uint32_t minimumStack = UINT32_MAX;
        const auto started = clock.Now().Nanoseconds();
        for (std::uint32_t segment = 1U; segment <= 16U; ++segment) {
            for (std::uint32_t index = 0U; index < 16U; ++index) {
                const auto ticket = (segment - 1U) * 16U + index + 1U;
                const auto framed = EncodeSealedRecord(ticket);
                AdmissionCoordinates[index] = clock.Now().Nanoseconds();
                Require(batch.Offer(std::span<const std::byte>(
                    TypedEdlg.data(), framed)).Status == L::EdlsBatchStatus::Buffered,
                    "BATCH_LOAD_RAM_ADMISSION");
            }
            const auto commitStarted = clock.Now().Nanoseconds();
            const auto result = batch.Service();
            const auto commitFinished = clock.Now().Nanoseconds();
            Require(result.Status == L::EdlsBatchStatus::DurablyAcknowledged &&
                result.AcknowledgedRecords == 16U &&
                result.Archive.AcknowledgedGeneration == segment,
                "BATCH_LOAD_DURABLE_ACK");
            for (std::size_t index = 0U; index < 16U; ++index) {
                RecordLatencies[completed++] = static_cast<std::uint32_t>(
                    (commitFinished - AdmissionCoordinates[index]) / 1000U);
            }
            minimumHeap = std::min(minimumHeap, esp_get_free_heap_size());
            minimumStack = std::min(minimumStack,
                static_cast<std::uint32_t>(uxTaskGetStackHighWaterMark(nullptr)));
            std::printf("EDP_BATCH|COMMIT|segment=%u|records=16|commit_us=%llu\n",
                segment, static_cast<unsigned long long>((commitFinished - commitStarted) / 1000U));
            std::fflush(stdout);
        }
        const auto elapsed = (clock.Now().Nanoseconds() - started) / 1000U;
        Require(completed == 256U && archive.Read().AcknowledgedGeneration == 16U,
            "BATCH_LOAD_FINAL_ACK");
        VerifySealedWindow(store, 15U, 16U);
        std::sort(RecordLatencies.begin(), RecordLatencies.end());
        std::printf("EDP_BATCH|PASS|records=256|ack_segment=16|window=15,16|first=225|last=256"
            "|elapsed_us=%llu|p50_us=%u|p99_us=%u|min_heap=%u|min_stack=%u|dwell_ns=100000000\n",
            static_cast<unsigned long long>(elapsed), RecordLatencies[128U],
            RecordLatencies[253U], minimumHeap, minimumStack);
        std::fflush(stdout);
        for (;;) { vTaskDelay(pdMS_TO_TICKS(1000U)); }
#endif
    }

} // native test-only Bootstrap
