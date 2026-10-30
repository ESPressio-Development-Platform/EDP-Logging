#pragma once
// Application-only native composition; no unmanaged worker or second logging queue.
#include <ESPressio_Memory.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>
#include <ESPressio_Threading.hpp>
#include <atomic>
#include <execution/ExecutionContextProvider.hpp>
#include <logging/DedicatedCoordinator.hpp>
#include <logging/GeneratedEdlsTaskAdapters.hpp>
#include <memory/MemoryResourceProvider.hpp>
#include <synchronization/SpinLockProvider.hpp>
#include <tuple>

namespace {

    namespace BatchWorkerHIL {

        namespace T = ESPressio::Threading;
        namespace M = ESPressio::Memory;
        namespace Platform = ESPressio::Platform;
        namespace CF = S::CompositionFramework;
        struct CoordinatorIdentity final {};
        using WorkerHandle = T::Thread<CoordinatorIdentity>;
        /// Creates a bounded test deadline in the already-bound canonical monotonic origin.
        static C::MonotonicTimestamp After(std::uint64_t nanoseconds) noexcept {
            return C::MonotonicTimestamp::FromNanoseconds(C::MonotonicNow().Nanoseconds() +
                                                          nanoseconds);
        }
        /// One exclusive upper bound covers every ticket including refused attempts.
        constexpr std::size_t MaximumAttempts = 2162U;
        /// Original producer coordinates retained only as test evidence for independent replay.
        static std::array<std::uint64_t, MaximumAttempts> SourceCoordinates{};
        /// Test telemetry of successful offer order, not a work queue or payload owner.
        static std::array<std::uint32_t, MaximumAttempts> OrderedTickets{};
        /// Occurrence-to-confirmed-ACK latency for each offered record, written only by the worker.
        static std::array<std::uint32_t, MaximumAttempts> AckLatencies{};
        /// Bootstrap-selected finite test phase observed by both managed producers.
        static std::atomic<unsigned> Phase{0U};
        /// Per-producer phase completion published to unmanaged Bootstrap.
        static std::array<std::atomic<unsigned>, 2U> CompletedPhase{};
        /// Count of records actually committed to the canonical source FIFO.
        static std::atomic<unsigned> Published{0U};
        /// Expected finite capacity refusals before any Memory acquisition.
        static std::atomic<unsigned> Refused{0U};
        /// Unexpected producer, delivery or ownership failures.
        static std::atomic<unsigned> Failures{0U};
        /// Native core observations prove which processors executed producers.
        static std::atomic<unsigned> ProducerCoreMask{0U};
        /// Native core observations for the existing coordinator.
        static std::atomic<unsigned> WorkerCoreMask{0U};
        /// Per-managed-thread stack headroom sampled inside its final callable boundary.
        static std::array<std::atomic<unsigned>, 3U> StackMargins{};
        /// One shared absolute origin for both 50Hz producer schedules.
        static std::atomic<std::uint64_t> ScheduleStart{0U};
        /// Canonical timestamp of the latest confirmed service completion.
        static std::atomic<std::uint64_t> LastAckTime{0U};
        /// Atomic cross-thread observation of the isolated no-new-input partial flush.
        static std::atomic<unsigned> IdleAckLatencyUs{0U};
        /// Maximum observed schedule lateness; never interpreted as CPU time.
        static std::atomic<unsigned> MaximumScheduleLatenessUs{0U};

        /// Assembles the original generated fifteen-field record without shared projection scratch.
        static void BuildSource(TypedRecord &record,
                                std::uint32_t ticket,
                                std::uint64_t coordinate) noexcept {
            record = TypedRecord{};
            S::Identity::DeviceIdentifier::Storage raw{};
            raw.fill(0xA6U);
            const L::QualifiedLogTime time{C::MonotonicTimestamp::FromNanoseconds(coordinate),
                                           C::SynchronizedTimestamp{},
                                           C::SynchronizationUncertainty::Maximum(),
                                           C::ClockEra{},
                                           C::SynchronizationState::NeverSynchronized,
                                           L::LogTemporalEvidenceStatus::Unavailable};
            Require(
                L::AssembleLogRecordSource<TypedSchema>(
                    record,
                    L::Severity::Warning,
                    S::Identity::DeviceIdentifier{raw},
                    S::Identity::RuntimeIncarnationId{7U},
                    L::AttemptSequence<L::AttemptSequenceWidth::Bits16>::FromValidatedValue(ticket),
                    time,
                    S::Identity::RuntimeIncarnationId{0x01020304U},
                    C::ClockEra::FromValue(0x0102030405060708ULL)
                ) ==
                    L::LogRecordAssemblyStatus::Prepared,
                "WORKER_SOURCE_ASSEMBLY"
            );
        }

        /// Memory-owned source value constructed by the existing generic producer.
        struct Source final {
            /// Fully owned generated source; no producer stack borrow survives publication.
            TypedRecord Value{};
            /// Captures the original occurrence coordinate before the canonical queue commit.
            Source(L::AttemptSequence<L::AttemptSequenceWidth::Bits16> attempt,
                   L::QualifiedLogTime captured,
                   unsigned) noexcept {
                const auto ticket = attempt.Value();
                Require(
                    ticket < MaximumAttempts,
                    "WORKER_TICKET_BOUND"
                );
                SourceCoordinates[ticket] = captured.Monotonic.Nanoseconds();
                BuildSource(
                    Value,
                    ticket,
                    SourceCoordinates[ticket]
                );
            }
        };
        using Resource = Platform::Portable::Memory::MemoryResourceProvider;
        using Mutex = Platform::FreeRTOS::Synchronization::MutexProvider;
        using Signal = Platform::FreeRTOS::Synchronization::SignalProvider;
        using Encoded = L::OwnedEncodedPayload<1024U>;
        using MemoryTopology =
            M::MemoryTopology<Resource,
                              M::SharedReserve<0U, Resource>,
                              M::ObjectPoolSpec<Source, M::DedicatedInstances<130U>>,
                              M::ObjectPoolSpec<Encoded, M::DedicatedInstances<1U>>>;
        using MemoryComposition = M::MemoryComposition<Resource>;
        using Allocator =
            MemoryComposition::Select<M::SharedReserveAllocationRequirement, CF::SelectUnique>;
        using MemoryRuntime = M::MemoryRuntime<MemoryTopology, MemoryComposition, Mutex, Signal>;
        using SourcePool = MemoryRuntime::ObjectPoolType<Source>;
        using EncodingPool = MemoryRuntime::ObjectPoolType<Encoded>;
        using Wakes = T::BoundedWaitWakeProvider<Signal, 3U>;
        using Lifecycle = L::AdmissionLifecycleCoordinator<2U, Wakes>;
        struct QueueIdentity final {};
        struct SequencerIdentity final {};
        using QueueMutex = T::OrdinaryMutexProvider<QueueIdentity, Mutex>;
        using SequencerMutex = T::OrdinaryMutexProvider<SequencerIdentity, Mutex>;
        using Queue =
            L::TransactionalRecordQueue<128U, SourcePool::LeaseType, QueueMutex, Lifecycle>;
        using Delivery = L::TypedCborProtectedEdlsTaskDelivery<TypedSchema,
                                                               L::AttemptSequenceWidth::Bits16,
                                                               3U,
                                                               2U,
                                                               1024U,
                                                               SealedSegmentBytes,
                                                               SealedBatch,
                                                               S::Identity::RuntimeIncarnationId,
                                                               C::ClockEra>;
        using Encoder = L::GeneratedLogRecordCborEncoder<TypedSchema,
                                                         L::AttemptSequenceWidth::Bits16,
                                                         2U,
                                                         12U,
                                                         S::Identity::RuntimeIncarnationId,
                                                         C::ClockEra>;

        /// Bootstrap adapter keeps encoding, typed delivery and deadline service in one owner.
        struct SourceSink final {
            /// Existing EDP-Memory pool owns the temporary encoded representation.
            EncodingPool *Pool;
            /// Existing generated canonical-CBOR encoder with worker-exclusive scratch.
            Encoder *Encode;
            /// Existing typed batch delivery, called and serviced only by this coordinator.
            Delivery *Deliver;
            /// Stable Memory lease retained if a provider cannot confirm reclamation.
            EncodingPool::LeaseType EncodedLease{};
            /// Number of successfully copied frames represented by telemetry order.
            unsigned Offered{0U};
            /// Consumed-once count of records with an independently confirmed strict archive ACK.
            unsigned Confirmed{0U};
            /// Encodes once and copies to the existing bounded batch before source retirement.
            L::RecordSinkSubmission operator()(const Source &source) noexcept {
                WorkerCoreMask.fetch_or(1U << xPortGetCoreID());
                const auto result =
                    L::EncodeOnceAndSubmitToSinks<L::EdlgCodec::Cbor, 1024U>(
                        *Pool,
                        EncodedLease,
                        source.Value,
                        *Encode,
                        *Deliver
                    );
                if (result.Status != L::OwnedRepresentationStatus::SubmittedAndReleased) {
                    ++Failures;
                    return L::RecordSinkSubmission::ProviderFailure;
                }
                OrderedTickets[Offered++] = source.Value.Attempt.Value();
                return L::RecordSinkSubmission::Accepted;
            }
            /// Services the same batch and records only newly confirmed durable completions.
            L::CoordinatorSinkServiceOutcome ServiceCoordinator() noexcept {
                const auto serviced = Deliver->ServiceCoordinator();
                const auto now = C::MonotonicNow().Nanoseconds();
                for (std::uint64_t n = 0U; n < serviced.DurableSinkCompletions; ++n) {
                    Require(
                        Confirmed < Offered,
                        "WORKER_NO_FALSE_COMPLETION"
                    );
                    const auto ticket = OrderedTickets[Confirmed];
                    AckLatencies[Confirmed++] =
                        static_cast<std::uint32_t>((now - SourceCoordinates[ticket]) / 1000U);
                }
                if (Confirmed == 129U) {
                    IdleAckLatencyUs.store(static_cast<unsigned>(AckLatencies[128U]));
                }
                if (serviced.DurableSinkCompletions != 0U)
                    LastAckTime.store(now);
                return serviced;
            }
        };
        using Coordinator = L::DedicatedCoordinator<64U, 5000000LL, Queue, SourceSink>;
        /// Measures the existing dedicated callable without creating another execution provider.
        struct MeasuredCoordinator final {
            /// Existing Logging coordinator over the canonical queue and source sink.
            Coordinator Inner;
            /// Binds the already-configured coordinator into the native test wrapper.
            explicit MeasuredCoordinator(Coordinator &&inner) noexcept : Inner(std::move(inner)) {}
            /// Transfers the pointer-only callable into the existing Threading binding.
            MeasuredCoordinator(MeasuredCoordinator &&) noexcept = default;
            /// Executes only inside the existing managed Threading context and publishes final
            /// stack evidence.
            void operator()(T::ThreadContext &context) noexcept {
                Inner(context);
                StackMargins[2U].store(uxTaskGetStackHighWaterMark(nullptr));
            }
        };
        /// Canonical unsynchronized occurrence capture; no fabricated synchronized evidence.
        struct Capture final {
            /// Obtains one producer-entry sample from the already-bound Clock owner.
            L::CoherentLogClockSample CaptureCoherent() const noexcept {
                return {C::MonotonicNow(),
                        {C::SynchronizedTimestamp{},
                         C::SynchronizationUncertainty::Maximum(),
                         C::SynchronizationState::NeverSynchronized},
                        C::ClockEra{},
                        false};
            }
        };
        struct ProducerAIdentity final {};
        struct ProducerBIdentity final {};
        /// Finite multi-phase producer executed by an existing topology-owned Dedicated Thread.
        struct Producer final {
            /// Unique admission/wake slot assigned to this producer.
            unsigned Slot;
            /// Canonical Logging transactional queue, never a second transport.
            Queue *Records;
            /// Existing admission shutdown and outstanding-lease owner.
            Lifecycle *Life;
            /// Existing bounded Threading wake owner for producer capacity changes.
            Wakes *WakeOwner;
            /// Existing Memory source pool, acquired only after queue capacity reservation.
            SourcePool *Pool;
            /// Existing ordinary mutex protects per-incarnation ticket issuance.
            SequencerMutex *Sequencer;
            /// Existing non-rewinding attempt sequencer shared by the two producers.
            L::AttemptSequenceCounter<L::AttemptSequenceWidth::Bits16> *Sequence;
            /// Stable handle populated before producer activation, used only for advisory wake.
            std::optional<WorkerHandle> *Worker;
            /// Executes only inside the existing managed Threading context and publishes final
            /// stack evidence.
            void operator()(T::ThreadContext &context) noexcept {
                unsigned completed = 0U;
                Capture capture{};
                while (!context.IsStopRequested()) {
                    const auto phase = Phase.load();
                    if (phase == 0U || phase == completed) {
                        (void)context.WaitFor(C::Duration::FromNanoseconds(20000000));
                        continue;
                    }
                    const unsigned count =
                        phase == 1U ? 80U : (phase == 2U ? (Slot == 0U ? 1U : 0U) : 1000U);
                    for (unsigned index = 0U; index < count; ++index) {
                        if (phase == 3U) {
                            const auto due = ScheduleStart.load() + index * 20000000ULL;
                            while (!context.IsStopRequested() &&
                                   C::MonotonicNow().Nanoseconds() < due) {
                                Require(
                                    context.WaitUntil(C::MonotonicTimestamp::FromNanoseconds(
                                            due)) != T::ThreadWaitResult::ProviderFailure,
                                    "WORKER_PRODUCER_ABSOLUTE_WAIT"
                                );
                            }
                            const auto late = static_cast<unsigned>(
                                (C::MonotonicNow().Nanoseconds() - due) / 1000U);
                            auto old = MaximumScheduleLatenessUs.load();
                            while (late > old &&
                                   !MaximumScheduleLatenessUs.compare_exchange_weak(
                                       old,
                                       late
                                   )) {
                            }
                        }
                        ProducerCoreMask.fetch_or(1U << xPortGetCoreID());
                        SourcePool::LeaseType scratch{};
                        Queue::Reservation reservation{};
                        const auto stopping = [&context]() noexcept {
                            return context.IsStopRequested();
                        };
                        const auto produced =
                            L::ProduceRecordUntil<L::AttemptSequenceWidth::Bits16, 16U, 16U>(
                                *Sequence,
                                *Sequencer,
                                capture,
                                *Records,
                                *Life,
                                *WakeOwner,
                                Slot,
                                *Pool,
                                scratch,
                                reservation,
                                After(100000000U),
                                stopping,
                                0U
                            );
                        if (produced.Status == L::RecordProductionStatus::Published) {
                            ++Published;
                            if (phase != 1U)
                                Require(
                                    (*Worker)->Wake() == T::ThreadWakeResult::Woken,
                                    "WORKER_PRODUCER_WAKE"
                                );
                        } else if (phase == 1U &&
                                   produced.Status == L::RecordProductionStatus::AdmissionFailed &&
                                   produced.Admission.Status ==
                                       L::RecordAdmissionStatus::DeadlineElapsed &&
                                   !produced.Admission.MemoryAttempted) {
                            ++Refused;
                        } else {
                            ++Failures;
                            std::printf(
                                "EDP_WORKER|PRODUCER_FAILURE|phase=%u|status=%u|admission=%"
                                        "u|commit=%u|reservation_active=%u\n",
                                phase,
                                static_cast<unsigned>(produced.Status),
                                static_cast<unsigned>(produced.Admission.Status),
                                static_cast<unsigned>(produced.Commit),
                                reservation.IsActive() ? 1U : 0U
                            );
                        }
                        Require(
                            scratch.IsEmpty() && !reservation.IsActive(),
                            "WORKER_PRODUCER_NO_LEASE_LEAK"
                        );
                    }
                    completed = phase;
                    CompletedPhase[Slot].store(phase);
                }
                StackMargins[Slot].store(uxTaskGetStackHighWaterMark(nullptr));
            }
        };
        using WorkerBinding = decltype(T::BindDedicatedThread<CoordinatorIdentity>(
            std::declval<MeasuredCoordinator>()));
        using ProducerABinding =
            decltype(T::BindDedicatedThread<ProducerAIdentity>(std::declval<Producer>()));
        using ProducerBBinding =
            decltype(T::BindDedicatedThread<ProducerBIdentity>(std::declval<Producer>()));
        using Topology =
            T::ThreadingTopology<T::DedicatedThread<CoordinatorIdentity,
                                                    T::StackCapacity<8192U>,
                                                    T::Priority<T::ThreadPriority::Normal>,
                                                    T::AnyAffinity>,
                                 T::DedicatedThread<ProducerAIdentity,
                                                    T::StackCapacity<4096U>,
                                                    T::Priority<T::ThreadPriority::Normal>,
                                                    T::Affinity<0U>>,
                                 T::DedicatedThread<ProducerBIdentity,
                                                    T::StackCapacity<4096U>,
                                                    T::Priority<T::ThreadPriority::Normal>,
                                                    T::Affinity<1U>>>;
        using ThreadingRuntime =
            T::StaticThreadingRuntime<Topology,
                                      std::tuple<WorkerBinding, ProducerABinding, ProducerBBinding>,
                                      Signal,
                                      Platform::ESPIDF::Execution::ExecutionContextProvider,
                                      Platform::ESPIDF::Synchronization::SpinLockProvider,
                                      Mutex>;

        /// Waits from unmanaged Bootstrap within one finite canonical deadline.
        /// @tparam TPredicate Read-only completion predicate for the current test gate.
        template <class TPredicate>
        static void Await(TPredicate predicate, unsigned milliseconds, const char *why) {
            const auto deadline = After(static_cast<std::uint64_t>(milliseconds) * 1000000ULL);
            while (!predicate() && C::MonotonicNow().Nanoseconds() < deadline.Nanoseconds()) {
                vTaskDelay(pdMS_TO_TICKS(10U));
            }
            Require(
                predicate(),
                why
            );
        }

        /// Reopens every retained frame and compares all canonical bytes with original source
        /// evidence.
        static void VerifyFinalMedia(Store &store, std::uint64_t ack) {
            const auto history =
                L::InspectEdlsArchiveWithScratch<3U, 1024U>(
                    store,
                    Paths,
                    SealedScratch
                );
            Require(
                history.Condition == L::EdlsArchiveCondition::Unambiguous &&
                        history.NextGeneration == ack + 1U,
                "WORKER_FINAL_MEDIA_HISTORY"
            );
            unsigned count = 0U, segments = 0U;
            for (unsigned slot = 0U; slot < 3U; ++slot) {
                const auto &item = history.Slots[slot];
                if (item.Condition == L::EdlsSlotCondition::Empty)
                    continue;
                Require(
                    item.Condition == L::EdlsSlotCondition::Clean &&
                            item.Header.Generation >= ack - 1U && item.Header.Generation <= ack &&
                            item.Header.DeviceIdentifier == DeviceIdentifier &&
                            item.Header.RuntimeIncarnation == 7U,
                    "WORKER_FINAL_PROTECTED_IDENTITY"
                );
                const auto read = store.ReadFileAt(
                    Paths[slot],
                    P::StorageOffset{},
                    P::DestinationBufferView{SealedReadback.data(), SealedReadback.size()}
                );
                Require(
                    read.Status == P::FileReadStatus::Succeeded,
                    "WORKER_MEDIA_READ"
                );
                std::size_t position = L::EdlsHeaderBytes;
                const auto end = read.BytesTransferred - L::EdlsSealBytes;
                unsigned frames = 0U;
                while (position < end) {
                    L::EdlgFrameView frame{};
                    Require(
                        L::ReadEdlgFrame(
                            std::span<const std::byte>(
                                    SealedReadback.data() + position,
                                    end - position
                                ),
                            frame,
                            1024U
                        ) == L::EdlgFrameReadResult::Succeeded,
                        "WORKER_FINAL_FRAME_CRC"
                    );
                    Require(
                        Serial::Deserialise<Serial::Cbor, Serial::RootProfile::TypedEnvelope>(
                            frame.Payload,
                            TypedDecoded
                        )
                                .IsSuccessful(),
                        "WORKER_FINAL_TYPED_DECODE"
                    );
                    const auto ticket = TypedDecoded.Attempt.Value();
                    Require(
                        ticket < MaximumAttempts && SourceCoordinates[ticket] != 0U,
                        "WORKER_FINAL_ORIGINAL_TICKET"
                    );
                    BuildSource(
                        TypedSource,
                        ticket,
                        SourceCoordinates[ticket]
                    );
                    Require(
                        L::ProjectGeneratedLogRecord<TypedSchema,
                                                         L::AttemptSequenceWidth::Bits16,
                                                         S::Identity::RuntimeIncarnationId,
                                                         C::ClockEra>(
                                                             TypedSource,
                                                             TypedWire,
                                                             TypedPositional
                                                         )
                                .IsSuccessful(),
                        "WORKER_ORIGINAL_PROJECT"
                    );
                    const auto encoded =
                        Serial::Serialise<Serial::Cbor, Serial::RootProfile::TypedEnvelope>(
                            TypedWire,
                            TypedCbor
                        );
                    Require(
                        encoded.IsSuccessful() &&
                                encoded.BytesWritten == frame.Payload.size() &&
                                std::equal(
                                    frame.Payload.begin(),
                                    frame.Payload.end(),
                                    TypedCbor.begin()
                                ),
                        "WORKER_EXACT_ORIGINAL_FIFTEEN_FIELDS"
                    );
                    std::printf(
                        "EDP_WORKER|ORIGINAL|ticket=%u|monotonic_ns=%llu\n",
                        static_cast<unsigned>(ticket),
                        static_cast<unsigned long long>(SourceCoordinates[ticket])
                    );
                    position += frame.Consumed;
                    ++count;
                    ++frames;
                }
                Require(
                    position == end && frames == item.VerifiedFrames,
                    "WORKER_FINAL_SEAL_COUNT"
                );
                ++segments;
            }
            Require(
                segments == 2U && count > 0U,
                "WORKER_TWO_RETAINED_SEGMENTS"
            );
            std::printf(
                "EDP_WORKER|MEDIA|ack=%llu|segments=%u|original_records=%u|all_15_fields=1\n",
                static_cast<unsigned long long>(ack),
                segments,
                count
            );
        }

        /// Composes existing domain owners and executes pressure, idle, schedule and shutdown
        /// gates.
        [[noreturn]] static void Run(Store &store,
                                     Bytes &bytes,
                                     nvs_handle_t session,
                                     esp_err_t phase) {
            Require(
                phase == ESP_ERR_NVS_NOT_FOUND,
                "WORKER_FRESH_TEST_PROVENANCE"
            );
            nvs_close(session);
            static NativeTimebase timebase{};
            static NativeClock clock(timebase);
            Require(
                C::BindMonotonicClock(clock),
                "WORKER_ONE_CANONICAL_ORIGIN"
            );
            static Resource resource{};
            static Mutex native{};
            static Allocator allocator{};
            static MemoryRuntime memory(native, allocator, resource);
            M::MemoryTopologyInitializationFailure failure{};
            Require(
                memory.Initialize(failure) == M::MemoryTopologyInitializationResult::Succeeded,
                "WORKER_MEMORY_INITIALIZE"
            );
            static Wakes wakes{};
            static Lifecycle lifecycle(wakes);
            static QueueMutex queueMutex{};
            static Queue queue(queueMutex, lifecycle);
            static SequencerMutex sequencer{};
            static L::AttemptSequenceCounter<L::AttemptSequenceWidth::Bits16> sequence{};
            static SealedNvs ledger(bytes);
            Require(
                ledger.Open("batchack") == E::NvsOpenStatus::Succeeded,
                "WORKER_NVS_OPEN"
            );
            static SealedArchive archive(
                store,
                ledger,
                Paths,
                SealedAckKey.Value,
                DeviceIdentifier,
                7U
            );
            Require(
                archive.InitializeEmpty().Status == L::EdlsAcknowledgedStatus::Initialized,
                "WORKER_ACK_GENESIS"
            );
            static SealedBatch batch(archive, clock, SealedScratch);
            Require(
                batch.ConfigureDwell(C::Duration::FromNanoseconds(100000000)) ==
                        L::EdlsBatchConfigurationStatus::Configured,
                "WORKER_CONFIGURE_DWELL"
            );
            static Delivery delivery(
                batch,
                TypedDecoded,
                TypedEdlg,
                SealedScratch,
                Paths,
                DeviceIdentifier,
                7U
            );
            static Encoder encoder(TypedWire, TypedPositional);
            static SourceSink sink{&memory.ObjectPoolFor<Encoded>(), &encoder, &delivery};
            static L::DedicatedCoordinatorReport report{};
            static std::optional<WorkerHandle> worker{};
            static ThreadingRuntime runtime(std::make_tuple(
                T::BindDedicatedThread<CoordinatorIdentity>(
                    MeasuredCoordinator(Coordinator(
                        queue,
                        report,
                        sink
                    ))),
                T::BindDedicatedThread<ProducerAIdentity>(Producer{0U,
                                                                   &queue,
                                                                   &lifecycle,
                                                                   &wakes,
                                                                   &memory.ObjectPoolFor<Source>(),
                                                                   &sequencer,
                                                                   &sequence,
                                                                   &worker}),
                T::BindDedicatedThread<ProducerBIdentity>(Producer{1U,
                                                                   &queue,
                                                                   &lifecycle,
                                                                   &wakes,
                                                                   &memory.ObjectPoolFor<Source>(),
                                                                   &sequencer,
                                                                   &sequence,
                                                                   &worker})
            ));
            Require(
                runtime.Initialize() == T::ThreadingInitializationResult::Succeeded &&
                        runtime.Start() == T::ThreadingStartResult::Succeeded,
                "WORKER_THREADING_INITIALIZE"
            );
            worker.emplace(runtime.ThreadHandle<CoordinatorIdentity>());
            auto first = runtime.ThreadHandle<ProducerAIdentity>();
            auto second = runtime.ThreadHandle<ProducerBIdentity>();
            Require(
                first.Start() == T::ThreadStartResult::Started &&
                        second.Start() == T::ThreadStartResult::Started,
                "WORKER_PRODUCERS_START"
            );
            Phase.store(1U);
            (void)first.Wake();
            (void)second.Wake();
            Await(
                [] { return CompletedPhase[0U].load() == 1U && CompletedPhase[1U].load() == 1U; },
                10000U,
                "WORKER_PRESSURE_PRODUCERS_FINITE"
            );
            Require(
                Published.load() == 128U && Refused.load() == 32U && Failures.load() == 0U &&
                        report.DurableSinkCompletions.load() == 0U &&
                        archive.Read().AcknowledgedGeneration == 0U,
                "WORKER_PRESSURE_RAM_NOT_DURABLE"
            );
            std::printf("EDP_WORKER|PRESSURE|attempts=160|published=128|deadline_refused=32|ack_"
                        "records=0\n");
            Require(
                worker->Start() == T::ThreadStartResult::Started,
                "WORKER_COORDINATOR_START"
            );
            (void)worker->Wake();
            Await(
                [&] { return report.DurableSinkCompletions.load() == 128U; },
                15000U,
                "WORKER_PRESSURE_DRAIN"
            );
            Phase.store(2U);
            (void)first.Wake();
            (void)second.Wake();
            Await(
                [&] { return report.RecordsRetired.load() == 129U; },
                2000U,
                "WORKER_IDLE_RAM_RETIRED"
            );
            Require(
                report.DurableSinkCompletions.load() == 128U,
                "WORKER_IDLE_RAM_NOT_ACK"
            );
            Await(
                [&] { return report.DurableSinkCompletions.load() == 129U; },
                5000U,
                "WORKER_IDLE_NO_NEW_INPUT"
            );
            Require(
                IdleAckLatencyUs.load() >= 100000U,
                "WORKER_DWELL_NOT_EARLY"
            );
            std::printf(
                "EDP_WORKER|IDLE|partial_records=1|subsequent_input=0|dwell_ns=100000000|"
                        "ack_latency_us=%u\n",
                IdleAckLatencyUs.load()
            );
            const auto started = C::MonotonicNow().Nanoseconds() + 100000000ULL;
            ScheduleStart.store(started);
            Phase.store(3U);
            (void)first.Wake();
            (void)second.Wake();
            unsigned minimumHeap = esp_get_free_heap_size();
            const auto end = started + 90000000000ULL;
            while ((CompletedPhase[0U].load() != 3U || CompletedPhase[1U].load() != 3U ||
                    report.DurableSinkCompletions.load() != 2129U) &&
                   C::MonotonicNow().Nanoseconds() < end && Failures.load() == 0U) {
                minimumHeap =
                    std::min(
                        minimumHeap,
                        static_cast<unsigned>(esp_get_free_heap_size())
                    );
                vTaskDelay(pdMS_TO_TICKS(10U));
            }
            const auto elapsedUs = (LastAckTime.load() - started) / 1000U;
            Require(
                Failures.load() == 0U && Published.load() == 2129U &&
                        report.DurableSinkCompletions.load() == 2129U &&
                        report.RecordsRetired.load() == 2129U &&
                        report.PartialSubmissions.load() == 0U,
                "WORKER_SCHEDULED_ALL_DURABLE"
            );
            Require(
                first.RequestStop() == T::ThreadStopRequestResult::Accepted &&
                        second.RequestStop() == T::ThreadStopRequestResult::Accepted &&
                        worker->RequestStop() == T::ThreadStopRequestResult::Accepted,
                "WORKER_STOP_REQUEST"
            );
            (void)first.Wake();
            (void)second.Wake();
            (void)worker->Wake();
            Await(
                [&] { return runtime.IsExecutionQuiescent(); },
                5000U,
                "WORKER_FINITE_QUIESCENCE"
            );
            Require(
                report.Exit.load() == L::DedicatedCoordinatorExit::CooperativeStop &&
                        report.SinkServiceFailures.load() == 0U && StackMargins[0U].load() > 0U &&
                        StackMargins[1U].load() > 0U && StackMargins[2U].load() > 0U,
                "WORKER_STOP_REPORT"
            );
            const auto ack = archive.Read().AcknowledgedGeneration;
            VerifyFinalMedia(
                store,
                ack
            );
            Require(
                runtime.BeginShutdown() == T::ThreadingShutdownResult::Accepted &&
                        runtime.FinalizeShutdown() == T::ThreadingFinalizationResult::Completed,
                "WORKER_NATIVE_FINALIZE"
            );
            Require(
                lifecycle.ShutdownUntil<16U>(After(1000000000U)).Status ==
                        L::AdmissionShutdownStatus::Quiescent,
                "WORKER_ADMISSION_QUIESCENT"
            );
            Require(
                memory.TearDown() == M::MemoryTopologyTeardownResult::Succeeded,
                "WORKER_MEMORY_TEARDOWN"
            );
            std::sort(
                AckLatencies.begin() + 129U,
                AckLatencies.begin() + 2129U
            );
            std::printf(
                "EDP_WORKER|PASS|scheduled_records=2000|producers=2|target_rps=100|elapsed_us=%llu"
                "|p50_us=%u|p99_us=%u|max_schedule_late_us=%u|min_heap=%u|producer0_stack=%u"
                "|producer1_stack=%u|worker_stack=%u|producer_core_mask=%u|worker_core_mask=%u|ack_"
                "segment=%llu\n",
                static_cast<unsigned long long>(elapsedUs),
                static_cast<unsigned>(AckLatencies[1129U]),
                static_cast<unsigned>(AckLatencies[2109U]),
                MaximumScheduleLatenessUs.load(),
                minimumHeap,
                StackMargins[0U].load(),
                StackMargins[1U].load(),
                StackMargins[2U].load(),
                ProducerCoreMask.load(),
                WorkerCoreMask.load(),
                static_cast<unsigned long long>(ack)
            );
            std::fflush(stdout);
            for (;;)
                vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    } // namespace BatchWorkerHIL

} // anonymous namespace
