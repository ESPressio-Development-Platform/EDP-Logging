// Real Memory/Task handle ABI, generated full typed CBOR, actual VFS and
// explicit atomic-NVS fake. Callback acceptance is RAM, service confirms durability.
#define EDP_LOGGING_STRICT_TASK_REUSE
#include "GeneratedEdlsAsyncTaskIntegrationTests.cpp"
#undef EDP_LOGGING_STRICT_TASK_REUSE
#include "StrictAckKeyValueFake.hpp"

namespace {
    namespace CF = ESPressio::System::CompositionFramework;
    namespace PK = ESPressio::Persistence;

    /// Deterministic canonical clock for exact partial-batch service boundaries.
    struct BatchServiceClock final :
        CF::Provider<C::Domain, CF::Offers<CF::Offer<C::MonotonicClock>>> {
        std::uint64_t Tick{0U};
        C::MonotonicTimestamp Now() const noexcept {
            return C::MonotonicTimestamp::FromNanoseconds(Tick);
        }
    };

    constexpr auto BatchAckKey = PK::KeyView::Validate("a");
    using Ledger = L::StrictAckTests::TestAckKeyValueStorage;
    using Archive = L::PowerLossAcknowledgedEdls<3U, 2U, 1024U, 4152U, Store, Ledger>;
    using Batch = L::BoundedEdlsBatch<4U, 1024U, Archive, BatchServiceClock, PortableBytes>;
    using BatchDelivery = L::TypedCborProtectedEdlsTaskDelivery<
        Schema, L::AttemptSequenceWidth::Bits16, 3U, 2U, 1024U, 4152U,
        Batch, S::Identity::RuntimeIncarnationId, C::ClockEra>;
    using LegacyDelivery = L::TypedCborProtectedEdlsTaskDelivery<
        Schema, L::AttemptSequenceWidth::Bits16, 3U, 2U, 1024U, 4152U,
        Archive, S::Identity::RuntimeIncarnationId, C::ClockEra>;
    static_assert(L::CoordinatorServiceSink<BatchDelivery>);
    static_assert(!L::CoordinatorServiceSink<LegacyDelivery>);
    static_assert(!std::is_copy_constructible_v<BatchDelivery>);
    static_assert(!std::is_move_constructible_v<BatchDelivery>);
    static_assert(std::is_copy_constructible_v<LegacyDelivery>);

    void RunTypedBatchService() {
        char directory[] = "/tmp/edp-typed-batch-service-XXXXXX";
        char* folder = ::mkdtemp(directory);
        assert(folder != nullptr);
        Resource resource{};
        TestSupport::TestMutexProvider native{};
        MemoryComposition::Select<
            M::SharedReserveAllocationRequirement,
            ESPressio::System::CompositionFramework::SelectUnique> allocator{};
        TypedTaskRuntime memory(native, allocator, resource);
        M::MemoryTopologyInitializationFailure failure{};
        assert(memory.Initialize(failure) == M::MemoryTopologyInitializationResult::Succeeded);
        auto& pool = memory.ObjectPoolFor<Value>();
        using Pool = std::remove_reference_t<decltype(pool)>;
        Pool::LeaseType sourceLease{}, copyLease{};
        std::optional<T::Task<L::RecordSinkSubmission>> task{};
        DeferredTaskExecutor executor{};
        PortableBytes bytes{};
        Store storage(folder, bytes);
        Ledger ledger{};
        BatchServiceClock clock{};
        std::array<std::byte, 4152U> archiveScratch{};
        std::array<std::byte, 1200U> frame{};
        std::array<std::byte, 16U> deviceBytes{};
        deviceBytes.fill(std::byte{0xA6U});
        S::Identity::DeviceIdentifier::Storage rawDevice{};
        rawDevice.fill(0xA6U);
        const S::Identity::DeviceIdentifier device{rawDevice};
        Archive archive(storage, ledger, Paths, BatchAckKey.Value, deviceBytes, 7U);
        assert(archive.InitializeEmpty().Status == L::EdlsAcknowledgedStatus::Initialized);
        Batch batch(archive, clock, archiveScratch);
        assert(batch.ConfigureDwell(C::Duration::FromNanoseconds(100000000)) ==
            L::EdlsBatchConfigurationStatus::Configured);
        Projection encoded{}, decoded{};
        Scratch positional{};
        L::GeneratedLogRecordCborEncoder<
            Schema, L::AttemptSequenceWidth::Bits16, 2U, 12U,
            S::Identity::RuntimeIncarnationId, C::ClockEra> encoder(encoded, positional);
        BatchDelivery delivery(
            batch, decoded, frame, archiveScratch, Paths, deviceBytes, 7U);
        L::AsyncCopyingTaskSink<
            L::EdlgCodec::Cbor, 1024U, Pool, PortableBytes,
            DeferredTaskExecutor, BatchDelivery>
            sink(pool, copyLease, task, bytes, executor, delivery);

        const auto send = [&](std::uint32_t ticket, bool accepted = true) {
            Record source = make_record(device, ticket, 7U);
            const auto admitted = L::EncodeOnceAndSubmitToSinks<
                L::EdlgCodec::Cbor, 1024U>(pool, sourceLease, source, encoder, sink);
            assert(admitted.Status == L::OwnedRepresentationStatus::SubmittedAndReleased);
            assert(sourceLease.IsEmpty() && !copyLease.IsEmpty());
            assert(sink.TryFinalize().Status == L::AsyncSinkCompletionStatus::Pending);
            source = Record{};
            encoded = Projection{};
            executor.Execute();
            const auto terminal = sink.TryFinalize();
            assert(terminal.Status == (accepted
                ? L::AsyncSinkCompletionStatus::AcceptedAndReleased
                : L::AsyncSinkCompletionStatus::ProviderFailureAndReleased));
            assert(copyLease.IsEmpty() && !task.has_value());
            assert(delivery.LastCompletion == (accepted
                ? L::TypedEdlsTaskCompletion::BufferedForBatch
                : L::TypedEdlsTaskCompletion::ArchiveFailure));
        };

        // Four terminal Tasks have released their copies but no record is durably ACKed.
        for (std::uint32_t ticket = 1U; ticket <= 4U; ++ticket) {
            send(ticket);
            assert(archive.Read().AcknowledgedGeneration == 0U);
        }
        auto serviced = delivery.ServiceCoordinator();
        assert(serviced.Status == L::CoordinatorSinkServiceStatus::Ready);
        assert(serviced.DurableSinkCompletions == 4U && !serviced.NextDeadline.has_value());
        assert(archive.Read().AcknowledgedGeneration == 1U);
        assert(delivery.ServiceCoordinator().DurableSinkCompletions == 0U);

        // No subsequent input is needed to expose the exact partial-flush deadline.
        assert(batch.ConfigureDwell(C::Duration::FromNanoseconds(50000000)) ==
            L::EdlsBatchConfigurationStatus::Configured);
        clock.Tick = 1000U;
        send(5U);
        serviced = delivery.ServiceCoordinator();
        assert(serviced.DurableSinkCompletions == 0U);
        assert(serviced.NextDeadline->Nanoseconds() == 50001000U);
        clock.Tick = 50000999U;
        assert(delivery.ServiceCoordinator().DurableSinkCompletions == 0U);
        assert(archive.Read().AcknowledgedGeneration == 1U);
        clock.Tick = 50001000U;
        assert(delivery.ServiceCoordinator().DurableSinkCompletions == 1U);
        assert(archive.Read().AcknowledgedGeneration == 2U);

        // Byte/count pressure may ACK the PRIOR batch during a callback.
        // A later service poll must report that delta once, while current input stays RAM.
        assert(batch.ConfigureDwell(C::Duration::FromNanoseconds(100000000)) ==
            L::EdlsBatchConfigurationStatus::Configured);
        for (std::uint32_t ticket = 6U; ticket <= 10U; ++ticket) {
            send(ticket);
        }
        assert(archive.Read().AcknowledgedGeneration == 3U);
        serviced = delivery.ServiceCoordinator();
        assert(serviced.DurableSinkCompletions == 4U && serviced.NextDeadline.has_value());
        assert(delivery.ServiceCoordinator().DurableSinkCompletions == 0U);
        clock.Tick = serviced.NextDeadline->Nanoseconds();
        assert(delivery.ServiceCoordinator().DurableSinkCompletions == 1U);
        assert(archive.Read().AcknowledgedGeneration == 4U);

        // An atomic NVS commit with a lost response stays UNKNOWN to the callback.
        // It emits no durable completion, latches recovery-required, and never retries.
        assert(batch.ConfigureDwell(C::Duration{}) == L::EdlsBatchConfigurationStatus::Configured);
        send(11U);
        ledger.commitThenFail = true;
        serviced = delivery.ServiceCoordinator();
        assert(serviced.Status == L::CoordinatorSinkServiceStatus::RecoveryRequired);
        assert(serviced.DurableSinkCompletions == 0U && !serviced.NextDeadline.has_value());
        assert(archive.Read().AcknowledgedGeneration == 5U); // real committed fake ledger
        const auto stores = ledger.stores;
        assert(delivery.ServiceCoordinator().Status ==
            L::CoordinatorSinkServiceStatus::RecoveryRequired);
        send(12U, false);
        assert(ledger.stores == stores);
        ledger.commitThenFail = false;
        assert(archive.Recover(archiveScratch).Status ==
            L::EdlsAcknowledgedStatus::CommittedOldestRetired);

        // Independent physical reopen and all typed payloads: ticket12 was never written.
        Store reopened(folder, bytes);
        const auto history = L::InspectEdlsArchiveWithScratch<3U, 1024U>(
            reopened, Paths, archiveScratch);
        assert(history.Condition == L::EdlsArchiveCondition::Unambiguous);
        std::array<std::byte, 4152U> media{};
        std::size_t records = 0U;
        for (std::size_t slot = 0U; slot < Paths.size(); ++slot) {
            const auto& item = history.Slots[slot];
            if (item.Condition == L::EdlsSlotCondition::Empty) {
                continue;
            }
            assert(item.Condition == L::EdlsSlotCondition::Clean);
            assert(item.VerifiedFrames == 1U);
            const auto read = reopened.ReadFileAt(
                Paths[slot], PK::StorageOffset{},
                PK::DestinationBufferView{media.data(), media.size()});
            assert(read.Status == PK::FileReadStatus::Succeeded);
            L::EdlgFrameView packet{};
            assert(L::ReadEdlgFrame(
                std::span<const std::byte>(media).subspan(
                    L::EdlsHeaderBytes,
                    read.BytesTransferred - L::EdlsHeaderBytes - L::EdlsSealBytes),
                packet, 1024U) == L::EdlgFrameReadResult::Succeeded);
            const auto parsed = Serial::Deserialise<
                Serial::Cbor, Serial::RootProfile::TypedEnvelope>(packet.Payload, decoded);
            assert(parsed.IsSuccessful());
            const auto expected = item.Header.Generation == 4U ? 10U : 11U;
            assert(item.Header.Generation == 4U || item.Header.Generation == 5U);
            assert(decoded.Attempt.Value() == expected);
            assert(decoded.DiagnosticSchemaFingerprint ==
                Schema::OwningContract::DiagnosticSchemaFingerprint);
            assert(std::get<0>(decoded.Arguments) == 0x01020304U);
            assert(std::get<1>(decoded.Arguments) == 0x0102030405060708ULL);
            ++records;
        }
        assert(records == 2U);
        assert(memory.TearDown() == M::MemoryTopologyTeardownResult::Succeeded);
        for (std::size_t index = 0U; index < Paths.size(); ++index) {
            char path[256U]{};
            assert(std::snprintf(path, sizeof(path), "%s/slot%zu", folder, index) > 0);
            assert(::unlink(path) == 0 || errno == ENOENT);
        }
        assert(::rmdir(folder) == 0);
    }
}

int main() {
    RunTypedBatchService();
    std::puts("TYPED_BATCH_RAM_VS_DURABLE_SERVICE_AND_UNCERTAIN_ACK=PASS");
}
