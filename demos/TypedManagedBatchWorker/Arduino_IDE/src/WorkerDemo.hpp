#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <tuple>
#include <utility>

#include <BatchDemo.hpp>
#include <ESPressio_Memory.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>
#include <ESPressio_Threading.hpp>
#include <GeneratedLocalisationContract.hpp>
#include <EDP-Platform-ESP-IDF/src/concurrency/AtomicWord32Provider.hpp>
#include <execution/ExecutionContextProvider.hpp>
#include <logging/DedicatedCoordinator.hpp>
#include <logging/DiagnosticClockEraArguments.hpp>
#include <logging/GeneratedEdlsTaskAdapters.hpp>
#include <logging/LogRecordSourceAssembly.hpp>
#include <logging/RecordProduction.hpp>
#include <memory/MemoryResourceProvider.hpp>
#include <synchronization/SpinLockProvider.hpp>

namespace ESPressio::Demos::TypedManagedBatchWorker {


    namespace B = ESPressio::Demos::ConfigurableBoundedEdlsBatch;
    namespace C = ESPressio::Clock;
    namespace CF = ESPressio::System::CompositionFramework;
    namespace L = ESPressio::Logging;
    namespace M = ESPressio::Memory;
    namespace P = ESPressio::Persistence;
    namespace Platform = ESPressio::Platform;
    namespace S = ESPressio::System;
    namespace Serial = ESPressio::Serialisation;
    namespace T = ESPressio::Threading;

    /// Application stage that failed; failed ownership remains resident.
    enum class Failure : std::uint8_t {
        Native = 0U,
        Identity = 1U,
        Mount = 2U,
        Memory = 3U,
        Archive = 4U,
        Source = 5U,
        Threading = 6U,
        Admission = 7U,
        Completion = 8U,
        Stop = 9U,
        Replay = 10U
    };

    /// Generated through the existing EDP-Localisation owner by build.sh.
    using Schema = Fixture::Diagnostic::DiagnosticMessageSchema<1U, 0U, 5U>;
    using Record = L::BasicLogRecord<L::AttemptSequenceWidth::Bits16, 2U, 12U>;
    using Projection = L::GeneratedLogRecordWireProjection<
        L::AttemptSequenceWidth::Bits16,
        S::Identity::RuntimeIncarnationId,
        C::ClockEra>;
    using Positional = L::OwnedPositionalScratch<S::Identity::RuntimeIncarnationId, C::ClockEra>;

    /// Finite source count; eight full-batch records plus one idle partial.
    constexpr unsigned SourceCount = 9U;
    static_assert(B::DwellNanoseconds >= 100000000LL);
    /// Four maximum-sized EDLG frames fit without sharing archive scratch.
    constexpr std::size_t BodyBytes = 4U * (1024U + 20U);
    /// Header, framed body and complete-segment seal.
    constexpr std::size_t SegmentBytes = BodyBytes + L::EdlsHeaderBytes + L::EdlsSealBytes;
    /// Bootstrap evidence only; no queued source borrows this array.
    static std::array<std::uint64_t, SourceCount> OriginalCoordinates{};
    /// Independent caller scratch and replay buffer.
    static std::array<std::byte, SegmentBytes> Scratch{};
    /// Readback is separate from scratch used by complete archive inspection.
    static std::array<std::byte, SegmentBytes> ReplayBytes{};

    // Failure disposition and canonical Bootstrap deadlines.

    /// Parks the application with ownership resident; never retries or implies a flush.
    [[noreturn]] inline void Fail(Failure stage) noexcept {
        std::printf(
            "EDP_TYPED_WORKER_DEMO|FAIL|stage=%u\n",
            static_cast<unsigned>(stage)
        );
        std::fflush(stdout);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }

    /// Uses the same canonical Clock already bound to Threading waits.
    [[nodiscard]] inline C::MonotonicTimestamp After(std::uint64_t nanoseconds) noexcept {
        return C::MonotonicTimestamp::FromNanoseconds(
            C::MonotonicNow().Nanoseconds() + nanoseconds);
    }

    /// Waits only from Bootstrap, preserving a finite canonical deadline.
    /// @tparam TPredicate Read-only observation of the managed owner.
    template <class TPredicate>
    inline void Await(TPredicate predicate, std::uint64_t nanoseconds, Failure failure) noexcept {
        const auto deadline = After(nanoseconds);
        while (!predicate() && C::MonotonicNow().Nanoseconds() < deadline.Nanoseconds()) {
            vTaskDelay(1U);
        }
        if (!predicate()) {
            Fail(failure);
        }
    }

    /// Reconstructs the exact original source for delivery and independent replay.
    inline void Assemble(
        Record& output,
        const S::Identity::SystemIdentity& identity,
        L::AttemptSequence<L::AttemptSequenceWidth::Bits16> attempt,
        L::QualifiedLogTime captured) noexcept {
        if (L::AssembleLogRecordSource<Schema>(
            output,
            L::Severity::Warning,
            identity.Device(),
            identity.Incarnation(),
            attempt,
            captured,
            S::Identity::RuntimeIncarnationId{11U},
            C::ClockEra::FromValue(22U)
        ) != L::LogRecordAssemblyStatus::Prepared) {
            Fail(Failure::Source);
        }
    }

    /// EDP-Memory owns each complete, independently captured source.
    struct Source final {

        // Source value and construction.

        /// All diagnostic values are owned; no Bootstrap/producer stack borrow survives.
        Record Value{};

        /// Immutable Bootstrap dependency, bound before any source construction.
        /// This facade reference is never stored in a queued Record or diagnostic argument.
        static inline const S::Identity::SystemIdentity* Identity{nullptr};

        /// Constructs only after canonical ticket/time capture and queue reservation.
        Source(
            L::AttemptSequence<L::AttemptSequenceWidth::Bits16> attempt,
            L::QualifiedLogTime captured) noexcept {
            if (Identity == nullptr || attempt.Value() == 0U || attempt.Value() > SourceCount) {
                Fail(Failure::Source);
            }
            OriginalCoordinates[attempt.Value() - 1U] = captured.Monotonic.Nanoseconds();
            Assemble(
                Value,
                *Identity,
                attempt,
                captured
            );
        }
    };

    using Resource = Platform::Portable::Memory::MemoryResourceProvider;
    using Mutex = Platform::FreeRTOS::Synchronization::MutexProvider;
    using Signal = Platform::FreeRTOS::Synchronization::SignalProvider;
    using Encoded = L::OwnedEncodedPayload<1024U>;
    using SourceTopology = M::MemoryTopology<
        Resource,
        M::SharedReserve<0U, Resource>,
        M::ObjectPoolSpec<Source, M::DedicatedInstances<17U>>>;
    using EncodingTopology = M::MemoryTopology<
        Resource,
        M::SharedReserve<0U, Resource>,
        M::ObjectPoolSpec<Encoded, M::DedicatedInstances<1U>>>;
    using MemoryComposition = M::MemoryComposition<Resource>;
    using Allocator = MemoryComposition::Select<
        M::SharedReserveAllocationRequirement,
        CF::SelectUnique>;
    using SourceRuntime = M::MemoryRuntime<SourceTopology, MemoryComposition, Mutex, Signal>;
    using EncodingRuntime = M::MemoryRuntime<EncodingTopology, MemoryComposition, Mutex, Signal>;
    using SourcePool = SourceRuntime::ObjectPoolType<Source>;
    using EncodingPool = EncodingRuntime::ObjectPoolType<Encoded>;
    using Wakes = T::BoundedWaitWakeProvider<Signal, 2U>;
    using Lifecycle = L::AdmissionLifecycleCoordinator<1U, Wakes, Platform::ESPIDF::Concurrency::AtomicWord32Provider>;

    /// Selects the existing ordinary queue mutex.
    struct QueueIdentity final {};
    /// Selects the existing ordinary per-incarnation ticket mutex.
    struct SequenceIdentity final {};
    /// Selects the one existing managed dedicated worker.
    struct WorkerIdentity final {};

    using QueueMutex = T::OrdinaryMutexProvider<QueueIdentity, Mutex>;
    using SequenceMutex = T::OrdinaryMutexProvider<SequenceIdentity, Mutex>;
    using Queue = L::TransactionalRecordQueue<16U, SourcePool::LeaseType, QueueMutex, Lifecycle>;
    using Archive = L::PowerLossAcknowledgedEdls<3U, 2U, 1024U, SegmentBytes, B::Files, B::Ledger>;
    using Batch = L::BoundedEdlsBatch<4U, BodyBytes, Archive, B::Clock, B::Bytes, 1000000000LL>;
    using Encoder = L::GeneratedLogRecordCborEncoder<
        Schema,
        L::AttemptSequenceWidth::Bits16,
        2U,
        12U,
        S::Identity::RuntimeIncarnationId,
        C::ClockEra>;
    using Delivery = L::TypedCborProtectedEdlsTaskDelivery<
        Schema,
        L::AttemptSequenceWidth::Bits16,
        3U,
        2U,
        1024U,
        SegmentBytes,
        Batch,
        S::Identity::RuntimeIncarnationId,
        C::ClockEra>;

    /// Serializes encoding, copied delivery and idle service in the same coordinator.
    struct SourceSink final {

        // Stable worker-exclusive dependencies.

        /// Independent existing Memory topology for one temporary encoded representation.
        EncodingPool* Pool;
        /// Existing generated typed encoder.
        Encoder* Encode;
        /// Existing strict typed delivery over the configured batch.
        Delivery* Deliver;
        /// Retains uncertain representation ownership; no unconfirmed release is retried.
        EncodingPool::LeaseType Lease{};

        // The same ordinary worker calls both methods.

        /// Accepted means a copied frame in bounded RAM; it is not durable completion.
        [[nodiscard]] L::RecordSinkSubmission operator()(const Source& source) noexcept {
            const auto result = L::EncodeOnceAndSubmitToSinks<L::EdlgCodec::Cbor, 1024U>(
                *Pool,
                Lease,
                source.Value,
                *Encode,
                *Deliver
            );
            return result.Status == L::OwnedRepresentationStatus::SubmittedAndReleased
                       ? L::RecordSinkSubmission::Accepted
                       : L::RecordSinkSubmission::ProviderFailure;
        }

        /// Returns consumed-once durable deltas and the canonical partial-batch deadline.
        [[nodiscard]] L::CoordinatorSinkServiceOutcome ServiceCoordinator() noexcept {
            return Deliver->ServiceCoordinator();
        }
    };

    using Coordinator = L::DedicatedCoordinator<4U, 5000000LL, Queue, SourceSink>;
    using WorkerBinding = decltype(T::BindDedicatedThread<WorkerIdentity>(
        std::declval<Coordinator>()));
    using ThreadTopology = T::ThreadingTopology<T::DedicatedThread<
        WorkerIdentity,
        T::StackCapacity<8192U>,
        T::Priority<T::ThreadPriority::Normal>,
        T::AnyAffinity>>;
    using ThreadRuntime = T::StaticThreadingRuntime<
        ThreadTopology,
        std::tuple<WorkerBinding>,
        Signal,
        Platform::ESPIDF::Execution::ExecutionContextProvider,
        Platform::ESPIDF::Synchronization::SpinLockProvider,
        Mutex>;

    /// Captures unsynchronized temporal evidence honestly in the canonical origin.
    struct Capture final {

        // Producer-entry temporal evidence.

        /// No synchronized time or Clock Era is invented.
        [[nodiscard]] L::CoherentLogClockSample CaptureCoherent() const noexcept {
            return {
                C::MonotonicNow(),
                {C::SynchronizedTimestamp{}, C::SynchronizationUncertainty::Maximum(), C::SynchronizationState::NeverSynchronized},
                C::ClockEra{},
                false};
        }
    };

    /// Distinct provider-relative names preserve unrelated example files.
    constexpr std::array<P::FilePathView, 3U> Paths{
        P::FilePathView::Validate("typed0").Value,
        P::FilePathView::Validate("typed1").Value,
        P::FilePathView::Validate("typed2").Value};
    /// Shares the durable identity namespace but has its own strict segment watermark.
    constexpr auto AckKey = P::KeyView::Validate("workerack").Value;

    // Independent source reconstruction after managed-worker quiescence.

    /// Checks all original fields by canonical re-encoding, not just ticket/count.
    inline void Replay(
        B::Files& files,
        const S::Identity::SystemIdentity& identity,
        const std::array<std::byte, 16U>& device,
        std::uint64_t acknowledged,
        Projection& wire,
        Positional& positional) noexcept {
        const auto history = L::InspectEdlsArchiveWithScratch<3U, 1024U>(
            files,
            Paths,
            Scratch
        );
        if (history.Condition != L::EdlsArchiveCondition::Unambiguous ||
            history.NextGeneration != acknowledged + 1U) {
            Fail(Failure::Replay);
        }
        std::array<bool, SourceCount> seen{};
        unsigned count = 0U;
        for (std::size_t slot = 0U; slot < Paths.size(); ++slot) {
            const auto& item = history.Slots[slot];
            if (item.Condition == L::EdlsSlotCondition::Empty) {
                continue;
            }
            if (item.Condition != L::EdlsSlotCondition::Clean ||
                item.Header.DeviceIdentifier != device ||
                item.Header.RuntimeIncarnation != identity.Incarnation().Value() ||
                item.Header.Generation < acknowledged - 1U ||
                item.Header.Generation > acknowledged) {
                Fail(Failure::Replay);
            }
            const auto read = files.ReadFileAt(
                Paths[slot],
                P::StorageOffset{},
                P::DestinationBufferView{ReplayBytes.data(), ReplayBytes.size()}
            );
            if (read.Status != P::FileReadStatus::Succeeded ||
                read.BytesTransferred != item.VerifiedPrefixBytes) {
                Fail(Failure::Replay);
            }
            const auto bodyEnd = read.BytesTransferred -
                                 (item.Header.Profile == L::EdlsContentProfile::SealedFrames ? L::EdlsSealBytes : 0U);
            std::size_t position = L::EdlsHeaderBytes;
            while (position < bodyEnd) {
                L::EdlgFrameView frame{};
                Projection decoded{};
                if (L::ReadEdlgFrame(
                    std::span<const std::byte>(
                            ReplayBytes.data() + position,
                            bodyEnd - position
                        ),
                    frame,
                    1024U
                ) != L::EdlgFrameReadResult::Succeeded ||
                    !Serial::Deserialise<Serial::Cbor, Serial::RootProfile::TypedEnvelope>(
                        frame.Payload,
                        decoded
                    )
                         .IsSuccessful()) {
                    Fail(Failure::Replay);
                }
                const auto ticket = decoded.Attempt.Value();
                if (ticket < 5U || ticket > SourceCount || seen[ticket - 1U]) {
                    Fail(Failure::Replay);
                }
                Record expected{};
                const L::QualifiedLogTime captured{
                    C::MonotonicTimestamp::FromNanoseconds(OriginalCoordinates[ticket - 1U]),
                    C::SynchronizedTimestamp{},
                    C::SynchronizationUncertainty::Maximum(),
                    C::ClockEra{},
                    C::SynchronizationState::NeverSynchronized,
                    L::LogTemporalEvidenceStatus::Unavailable};
                Assemble(
                    expected,
                    identity,
                    L::AttemptSequence<L::AttemptSequenceWidth::Bits16>::FromValidatedValue(ticket),
                    captured
                );
                if (!L::ProjectGeneratedLogRecord<
                         Schema,
                         L::AttemptSequenceWidth::Bits16,
                         S::Identity::RuntimeIncarnationId,
                         C::ClockEra>(
                             expected,
                             wire,
                             positional
                         )
                         .IsSuccessful()) {
                    Fail(Failure::Replay);
                }
                std::array<std::byte, 1024U> encoded{};
                const auto result = Serial::Serialise<
                    Serial::Cbor,
                    Serial::RootProfile::TypedEnvelope>(
                        wire,
                        encoded
                    );
                if (!result.IsSuccessful() || result.BytesWritten != frame.Payload.size() ||
                    !std::equal(
                        encoded.begin(),
                        encoded.begin() + result.BytesWritten,
                        frame.Payload.begin()
                    )) {
                    Fail(Failure::Replay);
                }
                seen[ticket - 1U] = true;
                ++count;
                position += frame.Consumed;
            }
            if (position != bodyEnd) {
                Fail(Failure::Replay);
            }
        }
        if (count != 5U) {
            Fail(Failure::Replay);
        }
    }

    /// Runs once; all owners remain stable until confirmed thread/lease quiescence.
    [[noreturn]] inline void Run() noexcept {
        if (nvs_flash_init() != ESP_OK) {
            Fail(Failure::Native);
        }
        static B::Bytes bytes{};
        static B::Ledger ledger(bytes);
        if (ledger.Open("batchdemo") != ESPressio::Persistence::EspIdf::NvsOpenStatus::Succeeded) {
            Fail(Failure::Native);
        }
        static std::array<std::byte, 16U> device{};
        std::uint32_t incarnation = 0U;
        if (B::EstablishIdentity(
            ledger,
            device,
            incarnation
        ) != B::Result::Completed) {
            Fail(Failure::Identity);
        }
        S::Identity::DeviceIdentifier::Storage raw{};
        for (std::size_t index = 0U; index < raw.size(); ++index) {
            raw[index] = std::to_integer<std::uint8_t>(device[index]);
        }
        static S::Identity::SystemIdentity identity(
            S::Identity::DeviceIdentifier{raw},
            S::Identity::RuntimeIncarnationId{incarnation}
        );
        Source::Identity = &identity;
        static ESPressio::Persistence::EspIdf::LittleFsVfsMount mount{};
        if (mount.Mount({"/edplfs", "edplfs", false, false}) !=
            ESPressio::Persistence::EspIdf::LittleFsMountStatus::Succeeded) {
            Fail(Failure::Mount);
        }
        static B::Files files(
            "/edplfs",
            bytes
        );
        static B::Timebase timebase{};
        static B::Clock clock(timebase);
        if (!C::BindMonotonicClock(clock)) {
            Fail(Failure::Native);
        }

        static Resource sourceResource{};
        static Mutex sourceNative{};
        static Allocator sourceAllocator{};
        static SourceRuntime sourceMemory(sourceNative, sourceAllocator, sourceResource);
        static Resource encodingResource{};
        static Mutex encodingNative{};
        static Allocator encodingAllocator{};
        static EncodingRuntime encodingMemory(encodingNative, encodingAllocator, encodingResource);
        M::MemoryTopologyInitializationFailure memoryFailure{};
        if (sourceMemory.Initialize(memoryFailure) != M::MemoryTopologyInitializationResult::Succeeded ||
            encodingMemory.Initialize(memoryFailure) != M::MemoryTopologyInitializationResult::Succeeded) {
            Fail(Failure::Memory);
        }
        static Wakes wakes{};
        static Lifecycle lifecycle(wakes);
        static QueueMutex queueMutex{};
        static Queue queue(queueMutex, lifecycle);
        static SequenceMutex sequencer{};
        static L::AttemptSequenceCounter<L::AttemptSequenceWidth::Bits16> sequence{};
        static Archive archive(files, ledger, Paths, AckKey, device, incarnation);
        const auto initial = archive.InitializeEmpty();
        if (initial.Status != L::EdlsAcknowledgedStatus::Initialized &&
            initial.Status != L::EdlsAcknowledgedStatus::Ready) {
            Fail(Failure::Archive);
        }
        const auto recovered = archive.Recover(Scratch);
        if (recovered.Status != L::EdlsAcknowledgedStatus::Ready &&
            recovered.Status != L::EdlsAcknowledgedStatus::UnacknowledgedCandidateRemoved &&
            recovered.Status != L::EdlsAcknowledgedStatus::CommittedOldestRetired) {
            Fail(Failure::Archive);
        }
        const auto before = archive.Read().AcknowledgedGeneration;
        static Batch batch(archive, clock, Scratch);
        if (batch.ConfigureDwell(C::Duration::FromNanoseconds(B::DwellNanoseconds)) !=
            L::EdlsBatchConfigurationStatus::Configured) {
            Fail(Failure::Archive);
        }
        static Projection wire{};
        static Projection decoded{};
        static Positional positional{};
        static std::array<std::byte, 1044U> frame{};
        static Delivery delivery(batch, decoded, frame, Scratch, Paths, device, incarnation);
        static Encoder encoder(wire, positional);
        static SourceSink sink{&encodingMemory.ObjectPoolFor<Encoded>(), &encoder, &delivery};
        static L::DedicatedCoordinatorReport report{};
        static ThreadRuntime runtime(std::make_tuple(
            T::BindDedicatedThread<WorkerIdentity>(
                Coordinator(
                    L::CoordinatorQueueLockWait<5000000>{},
                    queue,
                    report,
                    sink
                ))));
        if (runtime.Initialize() != T::ThreadingInitializationResult::Succeeded ||
            runtime.Start() != T::ThreadingStartResult::Succeeded) {
            Fail(Failure::Threading);
        }
        auto worker = runtime.ThreadHandle<WorkerIdentity>();
        Capture capture{};
        const auto isStopping = []() noexcept { return false; };
        for (unsigned index = 0U; index < SourceCount; ++index) {
            SourcePool::LeaseType sourceLease{};
            Queue::Reservation reservation{};
            const auto produced = L::ProduceRecordUntil<
                L::AttemptSequenceWidth::Bits16,
                16U,
                16U>(
                    sequence,
                    sequencer,
                    capture,
                    queue,
                    lifecycle,
                    wakes,
                    0U,
                    sourceMemory.ObjectPoolFor<Source>(),
                    sourceLease,
                    reservation,
                    After(1000000000ULL),
                    isStopping
                );
            if (produced.Status != L::RecordProductionStatus::Published ||
                !sourceLease.IsEmpty() || reservation.IsActive()) {
                Fail(Failure::Admission);
            }
        }
        if (report.DurableSinkCompletions.load() != 0U ||
            archive.Read().AcknowledgedGeneration != before) {
            Fail(Failure::Completion);
        }
        std::printf("EDP_TYPED_WORKER_DEMO|RAM|published=9|durable=0\n");
        if (worker.Start() != T::ThreadStartResult::Started ||
            worker.Wake() != T::ThreadWakeResult::Woken) {
            Fail(Failure::Threading);
        }
        bool isRamRetirementObserved = false;
        const auto completionDeadline = After(15000000000ULL);
        while (report.DurableSinkCompletions.load() != SourceCount &&
               C::MonotonicNow().Nanoseconds() < completionDeadline.Nanoseconds() &&
               report.SinkServiceFailures.load() == 0U &&
               report.PartialSubmissions.load() == 0U) {
            if (!isRamRetirementObserved && report.RecordsRetired.load() == SourceCount &&
                report.DurableSinkCompletions.load() < SourceCount) {
                isRamRetirementObserved = true;
                std::printf(
                    "EDP_TYPED_WORKER_DEMO|RAM_RETIRED|retired=9|durable=%llu\n",
                    static_cast<unsigned long long>(report.DurableSinkCompletions.load())
                );
            }
            vTaskDelay(1U);
        }
        const auto complete = isRamRetirementObserved && report.DurableSinkCompletions.load() == SourceCount &&
                              report.RecordsRetired.load() == SourceCount &&
                              report.PartialSubmissions.load() == 0U && report.SinkServiceFailures.load() == 0U;
        if (worker.RequestStop() != T::ThreadStopRequestResult::Accepted ||
            worker.Wake() != T::ThreadWakeResult::Woken) {
            Fail(Failure::Stop);
        }
        Await(
            [&] { return runtime.IsExecutionQuiescent(); },
            3000000000ULL,
            Failure::Stop
        );
        if (!complete || report.Exit.load() != L::DedicatedCoordinatorExit::CooperativeStop) {
            Fail(Failure::Completion);
        }
        const auto after = archive.Read().AcknowledgedGeneration;
        if (after != before + 3U) {
            Fail(Failure::Completion);
        }
        Replay(
            files,
            identity,
            device,
            after,
            wire,
            positional
        );
        if (runtime.BeginShutdown() != T::ThreadingShutdownResult::Accepted ||
            runtime.FinalizeShutdown() != T::ThreadingFinalizationResult::Completed ||
            lifecycle.ShutdownUntil<16U>(After(1000000000ULL)).Status !=
                L::AdmissionShutdownStatus::Quiescent ||
            sourceMemory.TearDown() != M::MemoryTopologyTeardownResult::Succeeded ||
            encodingMemory.TearDown() != M::MemoryTopologyTeardownResult::Succeeded) {
            Fail(Failure::Stop);
        }
        ledger.Close();
        if (mount.Unmount() != ESPressio::Persistence::EspIdf::LittleFsMountStatus::Succeeded) {
            Fail(Failure::Mount);
        }
        std::printf(
            "EDP_TYPED_WORKER_DEMO|PASS|incarnation=%u|records=9|durable=9|retired=9|"
            "ack_segment=%llu|retained_records=5|dwell_ns=%lld\n",
            static_cast<unsigned>(incarnation),
            static_cast<unsigned long long>(after),
            static_cast<long long>(B::DwellNanoseconds)
        );
        std::fflush(stdout);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }

} // namespace ESPressio::Demos::TypedManagedBatchWorker
