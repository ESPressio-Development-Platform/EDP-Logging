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

#ifndef EDP_WORKER_DWELL_NS
#define EDP_WORKER_DWELL_NS 100000000LL
#endif

#ifndef EDP_WORKER_RECORDS_PER_PRODUCER
#define EDP_WORKER_RECORDS_PER_PRODUCER 1000U
#endif

#ifndef EDP_WORKER_SCHEDULE_RPS
#define EDP_WORKER_SCHEDULE_RPS 100U
#endif

namespace {


#if defined(EDP_TEST_BYTE_CAPACITY)
    /// Application test budget, frozen by archive construction before genesis/recovery.
    constexpr std::uint64_t WorkerSinkByteCapacity = EDP_TEST_BYTE_CAPACITY;
#else
    constexpr std::uint64_t WorkerSinkByteCapacity = L::EdlsHeaderBytes + 3ULL * SealedSegmentBytes;
#endif

    namespace BatchWorkerHIL {


        namespace T = ESPressio::Threading;
        namespace M = ESPressio::Memory;
        namespace Platform = ESPressio::Platform;
        namespace CF = S::CompositionFramework;
        /// Explicit fixture configuration; the existing default100ms profile stays separately
        /// selectable.
        constexpr std::int64_t WorkerDwellNanoseconds = EDP_WORKER_DWELL_NS;
        /// Maximum canonical wait for a coordinator claim or retirement transition.
        constexpr std::int64_t WorkerCoordinatorQueueWaitNs = 5000000LL;
#if defined(EDP_WORKER_OPTIMISED_BUILD)
        /// Actual compiler arguments are independently checked by the build runner.
        constexpr const char* WorkerOptimisation = "o2";
#else
        /// Original actual compiler command uses debug optimisation.
        constexpr const char* WorkerOptimisation = "og";
#endif
#if defined(NDEBUG)
#error "Native worker qualification must retain assertion checks"
#endif

        static_assert(WorkerDwellNanoseconds == 100000000LL ||
                      WorkerDwellNanoseconds == 500000000LL);
        /// Reuses the existing public compile-time ceiling and runtime dwell API.
        using WorkerBatch = L::BoundedEdlsBatch<SealedRecords,
                                                SealedBodyBytes,
                                                SealedArchive,
                                                NativeClock,
                                                Bytes,
                                                WorkerDwellNanoseconds>;

        struct CoordinatorIdentity final {};
        using WorkerHandle = T::Thread<CoordinatorIdentity>;
        /// Creates a bounded test deadline in the already-bound canonical monotonic origin.
        static C::MonotonicTimestamp After(std::uint64_t nanoseconds) noexcept {
            return C::MonotonicTimestamp::FromNanoseconds(C::MonotonicNow().Nanoseconds() +
                                                          nanoseconds);
        }
        /// One exclusive upper bound covers every ticket including refused attempts.
        constexpr unsigned RecordsPerProducer = EDP_WORKER_RECORDS_PER_PRODUCER;
        static_assert(RecordsPerProducer == 1000U || RecordsPerProducer == 3000U || RecordsPerProducer == 15000U);
        /// Each of two managed producers uses a native-clock absolute 40/50 Hz schedule.
        constexpr unsigned ScheduledRecords = 2U * RecordsPerProducer;
        constexpr unsigned ScheduleRps = EDP_WORKER_SCHEDULE_RPS;
        static_assert(ScheduleRps == 100U || ScheduleRps == 80U);
        constexpr std::uint64_t SourcePeriodNs = 2000000000ULL / ScheduleRps;
#if defined(EDP_WORKER_DUAL_POOL_PROFILE)
        // Two independent identical MemoryRuntime owners; the resulting lease
        // type is identical and each lease retains its originating pool pointer.
        constexpr std::size_t WorkerSourceQueueSlots = 496U;
        constexpr std::size_t WorkerSourceDedicatedPerPool = 252U;
        constexpr unsigned PrimeAttemptsPerProducer = 64U;
        constexpr unsigned ExpectedPrimeRefusals = 0U;
#elif defined(EDP_WORKER_WIDE_QUEUE_PROFILE)
        constexpr std::size_t WorkerSourceQueueSlots = 252U;
        constexpr std::size_t WorkerSourceDedicatedPerPool = WorkerSourceQueueSlots + 2U;
        constexpr unsigned PrimeAttemptsPerProducer = 64U;
        constexpr unsigned ExpectedPrimeRefusals = 0U;
#else
        constexpr std::size_t WorkerSourceQueueSlots = 128U;
        constexpr std::size_t WorkerSourceDedicatedPerPool = WorkerSourceQueueSlots + 2U;
        constexpr unsigned PrimeAttemptsPerProducer = 80U;
        constexpr unsigned ExpectedPrimeRefusals = 32U;
#endif
        // Application-level finite absolute queue+Memory admission budgets.
        // The pressure test retains its original 100ms failure criterion.
        constexpr std::uint64_t PressureAdmissionBudgetNs = 100000000ULL;
        // Measured native sequence: callback max 706237us + service max 582835us
        // against ~1.28s buffering at 100 records/s in the 128-slot queue.
        // The finite 300ms scheduled admission objective is profile-specific.
        // It did NOT solve repeated native Flash service bursts at 100 rps;
        // the record format, sink and durability contract remain unchanged.
        // Keep the original 128-slot pressure-and-schedule profile's 100ms
        // deadline; only the independent 252/496 finite capacity experiments
        // select a separately qualified 300ms schedule admission budget.
        constexpr std::uint64_t ScheduledAdmissionBudgetNs =
            WorkerSourceQueueSlots == 128U ? 100000000ULL : 300000000ULL;

        /// Pressure128 accepted and one isolated idle partial precede the schedule.
        constexpr unsigned TotalAccepted = 129U + ScheduledRecords;
        /// Attempt tickets also include the32 deliberate pressure refusals.
        constexpr std::size_t MaximumAttempts = 162U + ScheduledRecords;
        // This finite fixture deliberately stays inside its selected16-bit attempt horizon.
        static_assert(MaximumAttempts <= 65535U);
        /// Long qualification keeps a bounded final6000-record latency window.
        constexpr std::size_t AckSampleCapacity = ScheduledRecords > 6000U ? 6000U : MaximumAttempts;
        /// Legacy modes still report every scheduled record's quantiles.
        constexpr std::size_t AckSampleStart = ScheduledRecords > 6000U ? 0U : 129U;
        /// Exact scope of reported quantiles, never an implicit all-run percentile.
        constexpr std::size_t AckSampleCount = ScheduledRecords > 6000U ? 6000U : ScheduledRecords;
        /// Same-worker aggregate statistics cover all scheduled records in the long mode.
        static std::uint64_t ScheduledLatencySumUs = 0U;
        /// Same-worker full scheduled sample count, checked after quiescence.
        static unsigned ScheduledLatencyCount = 0U;
        /// Full-run scheduled minimum, independent of the bounded percentile window.
        static std::uint32_t ScheduledLatencyMinimumUs = UINT32_MAX;
        /// Full-run scheduled maximum, independent of the bounded percentile window.
        static std::uint32_t ScheduledLatencyMaximumUs = 0U;
        /// Original producer coordinates retained only as test evidence for independent replay.
        constexpr std::size_t EvidenceSlots = WorkerSourceQueueSlots > 128U ? 1024U : 512U;
        // Covers queued128, two retained batches, pending batch, deferred service
        // completions and two producer reservations. This is test evidence, not work transport.
        static_assert(EvidenceSlots > WorkerSourceQueueSlots + 4U * SealedRecords + 2U);
        /// Original ticket tags make stale coordinate reuse an explicit test failure.
        static std::array<std::uint32_t, EvidenceSlots> CoordinateTickets{};
        /// Only live and retained source coordinates need history; exact latency samples stay separate.
        static std::array<std::uint64_t, EvidenceSlots> SourceCoordinates{};
        /// Test telemetry of successful offer order, not a work queue or payload owner.
        static std::array<std::uint32_t, EvidenceSlots> OrderedTickets{};
        /// Occurrence-to-confirmed-ACK latency for each offered record, written only by the worker.
        static std::array<std::uint32_t, AckSampleCapacity> AckLatencies{};
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
        /// Largest bounded admission transaction elapsed wall time, across both producers.
        static std::atomic<unsigned> MaximumAdmissionWaitUs{0U};

        /// Assembles the original generated fifteen-field record without shared projection scratch.
        static void BuildSource(TypedRecord& record,
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
                SourceCoordinates[ticket % EvidenceSlots] = captured.Monotonic.Nanoseconds();
                CoordinateTickets[ticket % EvidenceSlots] = ticket;
                BuildSource(
                    Value,
                    ticket,
                    SourceCoordinates[ticket % EvidenceSlots]
                );
            }
        };
        using Resource = Platform::Portable::Memory::MemoryResourceProvider;
        using Mutex = Platform::FreeRTOS::Synchronization::MutexProvider;
        using Signal = Platform::FreeRTOS::Synchronization::SignalProvider;
        using Encoded = L::OwnedEncodedPayload<1024U>;
        /// Existing source topology is shared only by producers and source reclamation.
        using MemoryTopology =
            M::MemoryTopology<Resource,
                              M::SharedReserve<0U, Resource>,
                              M::ObjectPoolSpec<Source, M::DedicatedInstances<WorkerSourceDedicatedPerPool>>>;
        /// Existing worker-exclusive representation topology avoids unrelated NoWait entrance
        /// contention.
        using EncodingTopology =
            M::MemoryTopology<Resource,
                              M::SharedReserve<0U, Resource>,
                              M::ObjectPoolSpec<Encoded, M::DedicatedInstances<1U>>>;
        using MemoryComposition = M::MemoryComposition<Resource>;
        using Allocator =
            MemoryComposition::Select<M::SharedReserveAllocationRequirement, CF::SelectUnique>;
        using MemoryRuntime = M::MemoryRuntime<MemoryTopology, MemoryComposition, Mutex, Signal>;
        /// Same existing Memory runtime component, independently configured for the one
        /// representation pool.
        using EncodingMemoryRuntime =
            M::MemoryRuntime<EncodingTopology, MemoryComposition, Mutex, Signal>;
        using SourcePool = MemoryRuntime::ObjectPoolType<Source>;
        using EncodingPool = EncodingMemoryRuntime::ObjectPoolType<Encoded>;
        using Wakes = T::BoundedWaitWakeProvider<Signal, 3U>;
        using Lifecycle = L::AdmissionLifecycleCoordinator<2U, Wakes>;
        struct QueueIdentity final {};
        struct SequencerIdentity final {};
        using QueueMutex = T::OrdinaryMutexProvider<QueueIdentity, Mutex>;
        using SequencerMutex = T::OrdinaryMutexProvider<SequencerIdentity, Mutex>;
        using Queue =
            L::TransactionalRecordQueue<WorkerSourceQueueSlots, SourcePool::LeaseType, QueueMutex, Lifecycle>;
        using Delivery = L::TypedCborProtectedEdlsTaskDelivery<TypedSchema,
                                                               L::AttemptSequenceWidth::Bits16,
                                                               3U,
                                                               2U,
                                                               1024U,
                                                               SealedSegmentBytes,
                                                               WorkerBatch,
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
            EncodingPool* Pool;
            /// Existing generated canonical-CBOR encoder with worker-exclusive scratch.
            Encoder* Encode;
            /// Existing typed batch delivery, called and serviced only by this coordinator.
            Delivery* Deliver;
            /// Stable Memory lease retained if a provider cannot confirm reclamation.
            EncodingPool::LeaseType EncodedLease{};
            /// Number of successfully copied frames represented by telemetry order.
            unsigned Offered{0U};
            /// Consumed-once count of records with an independently confirmed strict archive ACK.
            unsigned Confirmed{0U};

            // Current-phase wall-time observations; only the existing coordinator writes these.

            /// Phase whose ordinary-context samples are currently accumulated.
            unsigned ProfilePhase{0U};
            /// Generated source encoder calls in the current phase.
            std::uint64_t EncoderCalls{0U};
            /// Encoder elapsed wall nanoseconds, including any scheduling delay.
            std::uint64_t EncoderWallNs{0U};
            /// Typed delivery calls without an archive publication.
            std::uint64_t DeliveryNoFlushCalls{0U};
            /// Decode/canonical-validation/frame/copy wall time without an archive flush.
            std::uint64_t DeliveryNoFlushWallNs{0U};
            /// Typed delivery calls which also observed an archive publication.
            std::uint64_t DeliveryFlushCalls{0U};
            /// Delivery wall time including those explicit pressure/dwell publications.
            std::uint64_t DeliveryFlushWallNs{0U};
            /// Ordinary service polls in this phase.
            std::uint64_t ServiceCalls{0U};
            /// Ordinary service elapsed wall time, including strict archive commits.
            std::uint64_t ServiceWallNs{0U};
            /// Maximum uninterrupted native service-call wall duration (no additional timer).
            std::uint64_t ServiceMaxNs{0U};
            /// Number of service calls exceeding one producer's 100ms budget.
            std::uint64_t ServiceOver100Ms{0U};
            /// Number of service calls exceeding half of that budget.
            std::uint64_t ServiceOver50Ms{0U};
            /// Maximum source callback wall duration on the same coordinator.
            std::uint64_t CallbackMaxNs{0U};
            /// Total source callback elapsed wall time, including Memory acquire/reclaim.
            std::uint64_t CallbackWallNs{0U};

            // Same-owner measurement lifecycle; no timer, worker or provider is introduced.

            /// Starts a fresh measurement epoch when Bootstrap advances the test phase.
            void ObserveProfilePhase() noexcept {
                const auto phase = Phase.load();
                if (ProfilePhase == phase) {
                    return;
                }
                ProfilePhase = phase;
#if defined(EDP_TEST_STORAGE_IO_PROFILE)
                NativeStorageIoActive = phase == 3U;
#endif
                EncoderCalls = 0U;
                EncoderWallNs = 0U;
                DeliveryNoFlushCalls = 0U;
                DeliveryNoFlushWallNs = 0U;
                DeliveryFlushCalls = 0U;
                DeliveryFlushWallNs = 0U;
                ServiceCalls = 0U;
                ServiceWallNs = 0U;
                ServiceMaxNs = 0U;
                ServiceOver100Ms = 0U;
                ServiceOver50Ms = 0U;
                CallbackMaxNs = 0U;
                CallbackWallNs = 0U;
            }

            /// Bootstrap reads these counters only after actual managed-thread quiescence.
            void PrintProfile() const noexcept {
                std::printf(
                    "EDP_WORKER|WALL_PROFILE|phase=%u|application_optimisation=%s|cpu_mhz=%u|encoder_calls=%llu|encoder_us=%llu"
                    "|delivery_no_flush_calls=%llu|delivery_no_flush_us=%llu"
                    "|delivery_flush_calls=%llu|delivery_flush_us=%llu"
                    "|service_calls=%llu|service_us=%llu|callback_us=%llu"
                    "|service_max_us=%llu|service_over_50ms=%llu|service_over_100ms=%llu"
                    "|callback_max_us=%llu\n",
                    ProfilePhase,
                    WorkerOptimisation,
                    static_cast<unsigned>(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ),
                    static_cast<unsigned long long>(EncoderCalls),
                    static_cast<unsigned long long>(EncoderWallNs / 1000U),
                    static_cast<unsigned long long>(DeliveryNoFlushCalls),
                    static_cast<unsigned long long>(DeliveryNoFlushWallNs / 1000U),
                    static_cast<unsigned long long>(DeliveryFlushCalls),
                    static_cast<unsigned long long>(DeliveryFlushWallNs / 1000U),
                    static_cast<unsigned long long>(ServiceCalls),
                    static_cast<unsigned long long>(ServiceWallNs / 1000U),
                    static_cast<unsigned long long>(CallbackWallNs / 1000U),
                    static_cast<unsigned long long>(ServiceMaxNs / 1000U),
                    static_cast<unsigned long long>(ServiceOver50Ms),
                    static_cast<unsigned long long>(ServiceOver100Ms),
                    static_cast<unsigned long long>(CallbackMaxNs / 1000U)
                );
#if defined(EDP_TEST_STORAGE_IO_PROFILE)
                PrintNativeStorageIoProfile();
#endif
                std::fflush(stdout);
            }

            /// Encodes once and copies to the existing bounded batch before source retirement.
            L::RecordSinkSubmission operator()(const Source& source) noexcept {
                ObserveProfilePhase();
                const auto callbackStarted = C::MonotonicNow().Nanoseconds();
                auto measuredEncoder = [&](const TypedRecord& record, std::span<std::byte> output) noexcept {
                    const auto started = C::MonotonicNow().Nanoseconds();
                    const auto result = (*Encode)(
                        record,
                        output
                    );
                    EncoderWallNs += C::MonotonicNow().Nanoseconds() - started;
                    ++EncoderCalls;
                    return result;
                };
                auto measuredDelivery = [&](L::EdlgCodec codec, std::span<const std::byte> input) noexcept {
                    const auto started = C::MonotonicNow().Nanoseconds();
                    const auto result = (*Deliver)(
                        codec,
                        input
                    );
                    const auto elapsed = C::MonotonicNow().Nanoseconds() - started;
                    if (Deliver->LastAcknowledgement.AcknowledgedGeneration == 0U) {
                        ++DeliveryNoFlushCalls;
                        DeliveryNoFlushWallNs += elapsed;
                    } else {
                        ++DeliveryFlushCalls;
                        DeliveryFlushWallNs += elapsed;
                    }
                    return result;
                };
                WorkerCoreMask.fetch_or(1U << xPortGetCoreID());
                const auto result =
                    L::EncodeOnceAndSubmitToSinks<L::EdlgCodec::Cbor, 1024U>(
                        *Pool,
                        EncodedLease,
                        source.Value,
                        measuredEncoder,
                        measuredDelivery
                    );
                const auto callbackElapsed = C::MonotonicNow().Nanoseconds() - callbackStarted;
                CallbackWallNs += callbackElapsed;
                CallbackMaxNs = std::max(CallbackMaxNs, callbackElapsed);
                if (result.Status != L::OwnedRepresentationStatus::SubmittedAndReleased) {
                    std::printf(
                        "EDP_WORKER|SINK_FAILURE|ticket=%u|representation=%u|acquisition=%u|"
                        "release=%u"
                        "|delivery=%u|completion=%u|archive_ack_status=%u|encoded_lease_empty=%u\n",
                        static_cast<unsigned>(source.Value.Attempt.Value()),
                        static_cast<unsigned>(result.Status),
                        static_cast<unsigned>(result.Acquisition),
                        static_cast<unsigned>(result.Release),
                        static_cast<unsigned>(result.Submissions[0U]),
                        static_cast<unsigned>(Deliver->LastCompletion),
                        static_cast<unsigned>(Deliver->LastAcknowledgement.Status),
                        EncodedLease.IsEmpty() ? 1U : 0U
                    );
                    std::fflush(stdout);
                    ++Failures;
                    return L::RecordSinkSubmission::ProviderFailure;
                }
                OrderedTickets[Offered++ % EvidenceSlots] = source.Value.Attempt.Value();
                return L::RecordSinkSubmission::Accepted;
            }
            /// Services the same batch and records only newly confirmed durable completions.
            L::CoordinatorSinkServiceOutcome ServiceCoordinator() noexcept {
                ObserveProfilePhase();
                const auto serviceStarted = C::MonotonicNow().Nanoseconds();
                const auto serviced = Deliver->ServiceCoordinator();
                if (serviced.Status == L::CoordinatorSinkServiceStatus::RecoveryRequired) {
                    std::printf(
                        "EDP_WORKER|SERVICE_FAILURE|completion=%u|archive_ack_status=%u\n",
                        static_cast<unsigned>(Deliver->LastCompletion),
                        static_cast<unsigned>(Deliver->LastAcknowledgement.Status)
                    );
                    std::fflush(stdout);
                }
                const auto now = C::MonotonicNow().Nanoseconds();
                const auto serviceElapsed = now - serviceStarted;
                ServiceWallNs += serviceElapsed;
                ServiceMaxNs = std::max(ServiceMaxNs, serviceElapsed);
                ServiceOver50Ms += serviceElapsed >= 50000000ULL ? 1U : 0U;
                ServiceOver100Ms += serviceElapsed >= 100000000ULL ? 1U : 0U;
                if (serviceElapsed >= 100000000ULL && ServiceOver100Ms <= 20U) {
                    std::printf(
                        "EDP_WORKER|SLOW_SERVICE|index=%llu|elapsed_us=%llu"
                        "|offered=%u|confirmed=%u|pending=%u|ack_gen=%llu\n",
                        static_cast<unsigned long long>(ServiceCalls),
                        static_cast<unsigned long long>(serviceElapsed / 1000U),
                        Offered, Confirmed, Offered - Confirmed,
                        static_cast<unsigned long long>(
                            Deliver->LastAcknowledgement.AcknowledgedGeneration)
                    );
                    std::fflush(stdout);
                }
                ++ServiceCalls;
                for (std::uint64_t n = 0U; n < serviced.DurableSinkCompletions; ++n) {
                    Require(
                        Confirmed < Offered,
                        "WORKER_NO_FALSE_COMPLETION"
                    );
                    const auto ticket = OrderedTickets[Confirmed % EvidenceSlots];
                    Require(
                        CoordinateTickets[ticket % EvidenceSlots] == ticket,
                        "WORKER_ACK_COORDINATE_NOT_OVERWRITTEN"
                    );
                    // Only the final protected-media readback below emits
                    // ORIGINAL coordinates for normal completion. Reporting
                    // additional ACKed-but-already-retired coordinates here
                    // duplicates retained records and violates the strict
                    // independent raw-media receipt's one-witness-per-record rule.
#if defined(EDP_WORKER_POWER_CUT)
                    if (Confirmed < 128U) {
                        // Original source evidence leaves the MCU before any power cut.
                        std::printf(
                            "EDP_WORKER|ORIGINAL|ticket=%u|monotonic_ns=%llu\n",
                            static_cast<unsigned>(ticket),
                            static_cast<unsigned long long>(SourceCoordinates[ticket % EvidenceSlots])
                        );
                    }
#endif
                    const auto latencyUs = static_cast<std::uint32_t>(
                        (now - SourceCoordinates[ticket % EvidenceSlots]) / 1000U);
                    if constexpr (ScheduledRecords > 6000U) {
                        AckLatencies[Confirmed % AckSampleCapacity] = latencyUs;
                        if (Confirmed >= 129U) {
                            ScheduledLatencySumUs += latencyUs;
                            ++ScheduledLatencyCount;
                            ScheduledLatencyMinimumUs = std::min(
                                ScheduledLatencyMinimumUs,
                                latencyUs
                            );
                            ScheduledLatencyMaximumUs = std::max(
                                ScheduledLatencyMaximumUs,
                                latencyUs
                            );
                        }
                    } else {
                        AckLatencies[Confirmed] = latencyUs;
                    }
                    ++Confirmed;
#if defined(EDP_WORKER_POWER_CUT)
                    if (Confirmed == 64U || Confirmed == 128U) {
                        std::printf(
                            "EDP_BND|ACK|gen=%u\n",
                            Confirmed / 64U
                        );
                        std::fflush(stdout);
                    }
#endif
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
            explicit MeasuredCoordinator(Coordinator&& inner) noexcept
                : Inner(std::move(inner)) {}
            /// Transfers the pointer-only callable into the existing Threading binding.
            MeasuredCoordinator(MeasuredCoordinator&&) noexcept = default;
            /// Executes only inside the existing managed Threading context and publishes final
            /// stack evidence.
            void operator()(T::ThreadContext& context) noexcept {
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
            Queue* Records;
            /// Existing admission shutdown and outstanding-lease owner.
            Lifecycle* Life;
            /// Existing bounded Threading wake owner for producer capacity changes.
            Wakes* WakeOwner;
            /// Existing Memory source pool, acquired only after queue capacity reservation.
            SourcePool* Pool;
            /// Existing ordinary mutex protects per-incarnation ticket issuance.
            SequencerMutex* Sequencer;
            /// Existing non-rewinding attempt sequencer shared by the two producers.
            L::AttemptSequenceCounter<L::AttemptSequenceWidth::Bits16>* Sequence;
            /// Stable handle populated before producer activation, used only for advisory wake.
            std::optional<WorkerHandle>* Worker;
            /// Executes only inside the existing managed Threading context and publishes final
            /// stack evidence.
            void operator()(T::ThreadContext& context) noexcept {
                unsigned completed = 0U;
                Capture capture{};
                while (!context.IsStopRequested()) {
                    const auto phase = Phase.load();
                    if (phase == 0U || phase == completed) {
                        (void)context.WaitFor(C::Duration::FromNanoseconds(20000000));
                        continue;
                    }
                    const unsigned count =
                        phase == 1U ? PrimeAttemptsPerProducer
                                : (phase == 2U ? (Slot == 0U ? 1U : 0U) : RecordsPerProducer);
                    for (unsigned index = 0U; index < count; ++index) {
                        if (context.IsStopRequested()) {
                            break;
                        }
                        if (phase == 3U) {
                            const auto due = ScheduleStart.load() + index * SourcePeriodNs;
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
                        const auto admissionStarted = C::MonotonicNow().Nanoseconds();
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
                                After(
                                    phase == 3U ? ScheduledAdmissionBudgetNs
                                                : PressureAdmissionBudgetNs
                                ),
                                stopping,
                                0U
                            );
                        const auto admissionElapsed =
                            C::MonotonicNow().Nanoseconds() - admissionStarted;
                        auto oldWait = MaximumAdmissionWaitUs.load();
                        const auto waitUs = static_cast<unsigned>(admissionElapsed / 1000U);
                        while (waitUs > oldWait &&
                               !MaximumAdmissionWaitUs.compare_exchange_weak(oldWait, waitUs)) {}
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
                                "u|commit=%u|reservation_active=%u|queue=%u|memory=%u|"
                                "memory_attempted=%u|admission_elapsed_us=%u"
                                "|max_admission_us=%u|schedule_lateness_us=%u\n",
                                phase,
                                static_cast<unsigned>(produced.Status),
                                static_cast<unsigned>(produced.Admission.Status),
                                static_cast<unsigned>(produced.Commit),
                                reservation.IsActive() ? 1U : 0U,
                                static_cast<unsigned>(produced.Admission.Queue),
                                static_cast<unsigned>(produced.Admission.Memory),
                                produced.Admission.MemoryAttempted ? 1U : 0U,
                                waitUs, MaximumAdmissionWaitUs.load(),
                                MaximumScheduleLatenessUs.load()
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
        static void Await(TPredicate predicate, unsigned milliseconds, const char* why) {
            const auto deadline = After(static_cast<std::uint64_t>(milliseconds) * 1000000ULL);
            while (!predicate() && C::MonotonicNow().Nanoseconds() < deadline.Nanoseconds()) {
                vTaskDelay(pdMS_TO_TICKS(10U));
            }
            Require(
                predicate(),
                why
            );
        }

        /// Reopens retained frames and compares canonical bytes with supplied source evidence.
        /// Normal completion uses original RAM coordinates. Recovery reconstruction is structural
        /// only; the separate host audit must compare the original pre-cut coordinates.
        static void VerifyFinalMedia(Store& store, std::uint64_t ack, bool emitOriginal = true) {
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
                const auto& item = history.Slots[slot];
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
                        ticket < MaximumAttempts && CoordinateTickets[ticket % EvidenceSlots] == ticket &&
                            SourceCoordinates[ticket % EvidenceSlots] != 0U,
                        "WORKER_FINAL_ORIGINAL_TICKET"
                    );
                    BuildSource(
                        TypedSource,
                        ticket,
                        SourceCoordinates[ticket % EvidenceSlots]
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
                    if (emitOriginal)
                        std::printf(
                            "EDP_WORKER|ORIGINAL|ticket=%u|monotonic_ns=%llu\n",
                            static_cast<unsigned>(ticket),
                            static_cast<unsigned long long>(SourceCoordinates[ticket % EvidenceSlots])
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

#if defined(EDP_WORKER_POWER_CUT)
        /// Reconstructs only test telemetry after recovery. Actual original coordinates
        /// remain in the pre-cut host transcript and must pass the independent raw audit.
        [[noreturn]] static void RecoverInterruptedManagedBatch(Store& store, Bytes& bytes) {
            SealedNvs ledger(bytes);
            Require(
                ledger.Open("batchack") == E::NvsOpenStatus::Succeeded,
                "WORKER_CUT_LEDGER_OPEN"
            );
            SealedArchive archive(
                store,
                ledger,
                Paths,
                SealedAckKey.Value,
                DeviceIdentifier,
                7U,
                WorkerSinkByteCapacity
            );
            const auto reset = esp_reset_reason();
            std::printf(
                "EDP_BND|RESET_OBSERVED|reason=%u|por=%u\n",
                static_cast<unsigned>(reset),
                reset == ESP_RST_POWERON ? 1U : 0U
            );
            const auto before = archive.Read();
            Require(
                before.Status == L::EdlsAcknowledgedStatus::Ready &&
                    before.AcknowledgedGeneration == 2U,
                "WORKER_CUT_ACK2"
            );
            const auto recovered = archive.Recover(SealedScratch);
            Require(
                recovered.Status == L::EdlsAcknowledgedStatus::Ready ||
                    recovered.Status == L::EdlsAcknowledgedStatus::UnacknowledgedCandidateRemoved,
                "WORKER_CUT_RECONCILE_SPARE"
            );
            unsigned count = 0U;
            for (unsigned slot = 0U; slot < 2U; ++slot) {
                const auto read = store.ReadFileAt(
                    Paths[slot],
                    P::StorageOffset{},
                    P::DestinationBufferView{SealedReadback.data(), SealedReadback.size()}
                );
                Require(
                    read.Status == P::FileReadStatus::Succeeded &&
                        read.BytesTransferred >= L::EdlsHeaderBytes + L::EdlsSealBytes,
                    "WORKER_CUT_MEDIA_READ"
                );
                std::size_t position = L::EdlsHeaderBytes;
                const auto end = read.BytesTransferred - L::EdlsSealBytes;
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
                        ) == L::EdlgFrameReadResult::Succeeded &&
                            Serial::Deserialise<Serial::Cbor, Serial::RootProfile::TypedEnvelope>(
                                frame.Payload,
                                TypedDecoded
                            )
                                .IsSuccessful(),
                        "WORKER_CUT_FRAME"
                    );
                    const auto ticket = TypedDecoded.Attempt.Value();
                    Require(
                        ticket > 0U && ticket <= 160U &&
                            CoordinateTickets[ticket % EvidenceSlots] == 0U &&
                            TypedDecoded.Monotonic.Nanoseconds() > 0U,
                        "WORKER_CUT_BOUNDED_TICKET"
                    );
                    CoordinateTickets[ticket % EvidenceSlots] = ticket;
                    SourceCoordinates[ticket % EvidenceSlots] = TypedDecoded.Monotonic.Nanoseconds();
                    position += frame.Consumed;
                    ++count;
                }
                Require(
                    position == end,
                    "WORKER_CUT_EXACT_BODY"
                );
            }
            Require(
                count == 128U,
                "WORKER_CUT_ALL_PRESSURE_RECORDS"
            );
            // Same canonical reconstruction checks every other field; the host independently
            // compares the original occurrence coordinates instead of trusting this reconstruction.
            VerifyFinalMedia(
                store,
                2U,
                false
            );
#if defined(EDP_WORKER_CAPACITY_CUT)
            RemoveUnprotectedPadding(store);
            VerifyFinalMedia(
                store,
                2U,
                false
            );
            Require(
                archive.Read().AcknowledgedGeneration == 2U,
                "WORKER_CAPACITY_ACK_UNCHANGED"
            );
#endif
            std::printf("EDP_WORKER|POWER_CUT_RECOVERED|ack_segment=2|records=128\n");
            std::printf("EDP_BND|RECOVERED|newest=2|older=1|valid=2|contiguous=1\n");
#if defined(EDP_WORKER_CAPACITY_CUT)
            std::printf("EDP_BND|REPLAY_VERIFIED|MANAGED_BATCH128_AFTER_CAPACITY_CUT\n");
#else
            std::printf("EDP_BND|REPLAY_VERIFIED|MANAGED_BATCH128_AFTER_WRITE_ACTIVITY_CUT\n");
#endif
            std::fflush(stdout);
            for (;;) {
                vTaskDelay(pdMS_TO_TICKS(1000U));
            }
        }
#endif

        /// Composes existing domain owners and executes pressure, idle, schedule and shutdown
        /// gates.
        [[noreturn]] static void Run(Store& store,
                                     Bytes& bytes,
                                     nvs_handle_t session,
                                     esp_err_t phase) {
#if defined(EDP_WORKER_POWER_CUT)
            if (phase == ESP_OK) {
                std::uint8_t recoveredArmed = 0U;
                Require(
                    nvs_get_u8(
                        session,
                        "armed",
                        &recoveredArmed
                    ) == ESP_OK &&
                        recoveredArmed == 1U,
                    "WORKER_CUT_SESSION_PROVENANCE"
                );
                nvs_close(session);
                RecoverInterruptedManagedBatch(
                    store,
                    bytes
                );
            }
#endif
            Require(
                phase == ESP_ERR_NVS_NOT_FOUND,
                "WORKER_FRESH_TEST_PROVENANCE"
            );
#if !defined(EDP_WORKER_POWER_CUT)
            nvs_close(session);
#endif
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
#if defined(EDP_WORKER_DUAL_POOL_PROFILE)
            static Resource resourceB{};
            static Mutex nativeB{};
            static Allocator allocatorB{};
            static MemoryRuntime memoryB(nativeB, allocatorB, resourceB);
            Require(
                memoryB.Initialize(failure) ==
                    M::MemoryTopologyInitializationResult::Succeeded,
                "WORKER_SECOND_SOURCE_MEMORY_INITIALIZE"
            );
#endif
            // Reuse the existing G10 native Bootstrap pattern, not a new allocator or Memory API.
            static Resource encodingResource{};
            static Mutex encodingNative{};
            static Allocator encodingAllocator{};
            static EncodingMemoryRuntime encodingMemory(encodingNative,
                                                        encodingAllocator,
                                                        encodingResource);
            Require(
                encodingMemory.Initialize(failure) ==
                    M::MemoryTopologyInitializationResult::Succeeded,
                "WORKER_ENCODING_MEMORY_INITIALIZE"
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
#if defined(EDP_TEST_BYTE_CAPACITY)
            // Exercise exclusion without writing: even an oversized hypothetical
            // unrelated seed write must not affect this sink's observation.
            CheckSinkByteBoundary(
                store.Actual,
                Seed.Value,
                EDP_TEST_BYTE_CAPACITY + 1U
            );
            Require(
                ObservedSinkPeakBytes == 0U,
                "BYTE_CAPACITY_UNRELATED_PATH_EXCLUDED"
            );
#endif
            static SealedArchive archive(
                store,
                ledger,
                Paths,
                SealedAckKey.Value,
                DeviceIdentifier,
                7U,
                WorkerSinkByteCapacity
            );
            Require(
                archive.InitializeEmpty().Status == L::EdlsAcknowledgedStatus::Initialized,
                "WORKER_ACK_GENESIS"
            );
            static WorkerBatch batch(archive, clock, SealedScratch);
            Require(
                batch.ConfigureDwell(C::Duration::FromNanoseconds(WorkerDwellNanoseconds)) ==
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
            static SourceSink sink{&encodingMemory.ObjectPoolFor<Encoded>(), &encoder, &delivery};
            static L::DedicatedCoordinatorReport report{};
            static std::optional<WorkerHandle> worker{};
            static ThreadingRuntime runtime(std::make_tuple(
                T::BindDedicatedThread<CoordinatorIdentity>(MeasuredCoordinator(
                    Coordinator(
                        L::CoordinatorQueueLockWait<WorkerCoordinatorQueueWaitNs>{},
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
#if defined(EDP_WORKER_DUAL_POOL_PROFILE)
                                                                   &memoryB.ObjectPoolFor<Source>(),
#else
                                                                   &memory.ObjectPoolFor<Source>(),
#endif
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
                Published.load() == 128U && Refused.load() == ExpectedPrimeRefusals &&
                    Failures.load() == 0U &&
                    report.DurableSinkCompletions.load() == 0U &&
                    archive.Read().AcknowledgedGeneration == 0U,
                "WORKER_PRESSURE_RAM_NOT_DURABLE"
            );
#if defined(EDP_WORKER_WIDE_QUEUE_PROFILE) || defined(EDP_WORKER_DUAL_POOL_PROFILE)
            std::printf("EDP_WORKER|PRIME|attempts=128|published=128|deadline_refused=0"
                        "|ack_records=0|queue_slots=%zu\n", WorkerSourceQueueSlots);
#else
            std::printf("EDP_WORKER|PRESSURE|attempts=160|published=128|deadline_refused=32|ack_"
                        "records=0\n");
#endif
            Require(
                worker->Start() == T::ThreadStartResult::Started,
                "WORKER_COORDINATOR_START"
            );
            (void)worker->Wake();
#if defined(EDP_WORKER_CAPACITY_CUT)
            // Read-only diagnostic in the disposable physical cut fixture.
            // Preserve the original finite 15 s deadline, but expose whether
            // the worker is processing the backlog or waiting with it queued.
            std::uint64_t nextDrainTrace =
                C::MonotonicNow().Nanoseconds() + 2500000000ULL;
            Await(
                [&] {
                    const auto durable = report.DurableSinkCompletions.load();
                    const auto now = C::MonotonicNow().Nanoseconds();
                    if (durable != 128U && now >= nextDrainTrace) {
                        std::printf(
                            "EDP_WORKER|PRESSURE_DRAIN_TRACE|retired=%llu|"
                            "accepted=%llu|durable=%llu|offered=%u|confirmed=%u|"
                            "sink_service_calls=%llu|worker_exit=%u|"
                            "idle_timeouts=%llu|queue_contentions=%llu\n",
                            static_cast<unsigned long long>(report.RecordsRetired.load()),
                            static_cast<unsigned long long>(report.AllSinksAccepted.load()),
                            static_cast<unsigned long long>(durable),
                            sink.Offered,
                            sink.Confirmed,
                            static_cast<unsigned long long>(sink.ServiceCalls),
                            static_cast<unsigned>(report.Exit.load()),
                            static_cast<unsigned long long>(report.IdleTimeouts.load()),
                            static_cast<unsigned long long>(report.QueueContentions.load())
                        );
                        std::fflush(stdout);
                        nextDrainTrace = now + 2500000000ULL;
                    }
                    return durable == 128U;
                },
                15000U,
                "WORKER_PRESSURE_DRAIN"
            );
#else
            Await(
                [&] { return report.DurableSinkCompletions.load() == 128U; },
                15000U,
                "WORKER_PRESSURE_DRAIN"
            );
#endif
#if defined(EDP_WORKER_POWER_CUT)
            Require(
                nvs_set_u8(
                    session,
                    "armed",
                    1U
                ) == ESP_OK &&
                    nvs_commit(session) == ESP_OK,
                "WORKER_CUT_ARM"
            );
            nvs_close(session);
            // Published before the new scheduled phase and its queue admission; no pending batch remains.
#if defined(EDP_WORKER_CAPACITY_CUT)
            FillUnprotectedAndRefuse(
                store,
                false
            );
            const auto beforeCandidateCalls = SealedReplaceCalls;
            std::size_t capacityTotal = 0U, capacityUsed = 0U;
            Require(
                esp_littlefs_info(
                    "edplfs",
                    &capacityTotal,
                    &capacityUsed
                ) == ESP_OK &&
                    capacityTotal == 0x90000U && capacityUsed <= capacityTotal &&
                    capacityTotal - capacityUsed <= 8192U,
                "WORKER_CAPACITY_REAL_EXHAUSTION"
            );
#else
            SealedWriteActivityArmed = true;
#endif
            ScheduleStart.store(C::MonotonicNow().Nanoseconds() + 100000000ULL);
            Phase.store(3U);
            (void)first.Wake();
            (void)second.Wake();
#if defined(EDP_WORKER_CAPACITY_CUT)
            Await(
                [&] { return report.SinkServiceFailures.load() != 0U; },
                15000U,
                "WORKER_CAPACITY_FAILURE_OBSERVED"
            );
            (void)first.RequestStop();
            (void)second.RequestStop();
            (void)worker->RequestStop();
            (void)first.Wake();
            (void)second.Wake();
            (void)worker->Wake();
            Await(
                [&] { return runtime.IsExecutionQuiescent(); },
                5000U,
                "WORKER_CAPACITY_QUIESCENT_OWNERS"
            );
            const auto pending = batch.Service();
            const auto durable = archive.Read();
            Require(
                SealedReplaceCalls > beforeCandidateCalls &&
                    SealedLastReplace != P::FileReplaceStatus::Succeeded &&
                    pending.Status == L::EdlsBatchStatus::RecoveryRequired &&
                    pending.PendingRecords > 0U && pending.AcknowledgedRecords == 0U &&
                    durable.Status == L::EdlsAcknowledgedStatus::Ready &&
                    durable.AcknowledgedGeneration == 2U &&
                    report.DurableSinkCompletions.load() == 128U,
                "WORKER_CAPACITY_RETAIN_RAM_AND_OLD_ACK"
            );
            std::printf(
                "EDP_BND|MANAGED_CAPACITY_SAFE|ack=2|pending_records=%u|native_replace=%u|durable=128|free_bytes=%u\n",
                static_cast<unsigned>(pending.PendingRecords),
                static_cast<unsigned>(SealedLastReplace),
                static_cast<unsigned>(capacityTotal - capacityUsed)
            );
            std::fflush(stdout);
            // Ownership remains resident after the fatal service result; no implicit retry,
            // source replay or Memory teardown is invented before the physical cut.
#endif
            vTaskDelay(pdMS_TO_TICKS(15000U));
            Halt("WORKER_WRITE_ACTIVITY_CUT_NOT_OBSERVED");
#endif
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
                static_cast<std::uint64_t>(IdleAckLatencyUs.load()) >=
                    static_cast<std::uint64_t>(WorkerDwellNanoseconds / 1000),
                "WORKER_DWELL_NOT_EARLY"
            );
            std::printf(
                "EDP_WORKER|IDLE|partial_records=1|subsequent_input=0|dwell_ns=%llu|"
                "ack_latency_us=%u\n",
                static_cast<unsigned long long>(WorkerDwellNanoseconds),
                IdleAckLatencyUs.load()
            );
            const auto started = C::MonotonicNow().Nanoseconds() + 100000000ULL;
            ScheduleStart.store(started);
            std::printf(
                "EDP_WORKER|ADMISSION_PROFILE|pressure_budget_us=%llu"
                "|scheduled_budget_us=%llu|queue_slots=%zu|schedule_hz=%u|coordinator_lock_us=%llu\n",
                static_cast<unsigned long long>(PressureAdmissionBudgetNs / 1000U),
                static_cast<unsigned long long>(ScheduledAdmissionBudgetNs / 1000U),
                WorkerSourceQueueSlots,
                ScheduleRps,
                static_cast<unsigned long long>(WorkerCoordinatorQueueWaitNs / 1000U)
            );
            std::fflush(stdout);
            Phase.store(3U);
            (void)first.Wake();
            (void)second.Wake();
            unsigned minimumHeap = esp_get_free_heap_size();
            std::uint64_t maximumSampledBacklog = 0U;
            const auto end = started + RecordsPerProducer * SourcePeriodNs + 70000000000ULL;
            auto nextProgress = started + 10000000000ULL;
            while ((CompletedPhase[0U].load() != 3U || CompletedPhase[1U].load() != 3U ||
                    report.DurableSinkCompletions.load() != TotalAccepted) &&
                   C::MonotonicNow().Nanoseconds() < end && Failures.load() == 0U) {
                minimumHeap =
                    std::min(
                        minimumHeap,
                        static_cast<unsigned>(esp_get_free_heap_size())
                    );
                const auto retired = report.RecordsRetired.load();
                const auto published = static_cast<std::uint64_t>(Published.load());
                const auto backlog = published > retired ? published - retired : 0U;
                maximumSampledBacklog = std::max(maximumSampledBacklog, backlog);
                if constexpr (ScheduledRecords > 6000U) {
                    const auto now = C::MonotonicNow().Nanoseconds();
                    if (now >= nextProgress) {
                        std::printf(
                            "EDP_WORKER|PROGRESS|scheduled_records=%u|published=%u|retired=%llu|durable=%llu|failures=%u|min_heap=%u"
                            "|time_us=%llu|sampled_backlog=%llu|max_sampled_backlog=%llu\n",
                            ScheduledRecords,
                            Published.load(),
                            static_cast<unsigned long long>(report.RecordsRetired.load()),
                            static_cast<unsigned long long>(report.DurableSinkCompletions.load()),
                            Failures.load(),
                            minimumHeap,
                            static_cast<unsigned long long>(now / 1000U),
                            static_cast<unsigned long long>(backlog),
                            static_cast<unsigned long long>(maximumSampledBacklog)
                        );
                        std::fflush(stdout);
                        nextProgress += 10000000000ULL;
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(10U));
            }
            const auto elapsedUs = (LastAckTime.load() - started) / 1000U;
            std::printf(
                "EDP_WORKER|QUEUE_SAMPLE|max_published_minus_retired=%llu|capacity=%zu|min_heap=%u\n",
                static_cast<unsigned long long>(maximumSampledBacklog),
                WorkerSourceQueueSlots,
                minimumHeap
            );

            const bool isFullyCompleted = Failures.load() == 0U && Published.load() == TotalAccepted &&
                                          report.DurableSinkCompletions.load() == TotalAccepted &&
                                          report.RecordsRetired.load() == TotalAccepted && report.PartialSubmissions.load() == 0U;
            if (!isFullyCompleted) {
                // Failure disposition is explicit: stop, retain queued/RAM ownership, never infer a flush.
                (void)first.RequestStop();
                (void)second.RequestStop();
                (void)worker->RequestStop();
                (void)first.Wake();
                (void)second.Wake();
                (void)worker->Wake();
                Await(
                    [&] { return runtime.IsExecutionQuiescent(); },
                    5000U,
                    "WORKER_DIAGNOSTIC_QUIESCENCE"
                );
                sink.PrintProfile();
                std::printf(
                    "EDP_WORKER|SCHEDULE_RESULT|published=%u|refused=%u|failures=%u|retired=%llu"
                    "|accepted=%llu|durable=%llu|partial=%llu|exit=%u|reclaim_result=%u|phase0=%u|phase1=%u\n",
                    Published.load(),
                    Refused.load(),
                    Failures.load(),
                    static_cast<unsigned long long>(report.RecordsRetired.load()),
                    static_cast<unsigned long long>(report.AllSinksAccepted.load()),
                    static_cast<unsigned long long>(report.DurableSinkCompletions.load()),
                    static_cast<unsigned long long>(report.PartialSubmissions.load()),
                    static_cast<unsigned>(report.Exit.load()),
                    static_cast<unsigned>(report.ReclaimFailure.load()),
                    CompletedPhase[0U].load(),
                    CompletedPhase[1U].load()
                );
                std::fflush(stdout);
                Halt("WORKER_SCHEDULED_ALL_DURABLE");
            }
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
            sink.PrintProfile();
            std::printf(
                "EDP_WORKER|SCHEDULE_RESULT|published=%u|refused=%u|failures=%u|retired=%llu"
                "|accepted=%llu|durable=%llu|partial=%llu|exit=%u|reclaim_result=%u|phase0=%u|phase1=%u\n",
                Published.load(),
                Refused.load(),
                Failures.load(),
                static_cast<unsigned long long>(report.RecordsRetired.load()),
                static_cast<unsigned long long>(report.AllSinksAccepted.load()),
                static_cast<unsigned long long>(report.DurableSinkCompletions.load()),
                static_cast<unsigned long long>(report.PartialSubmissions.load()),
                static_cast<unsigned>(report.Exit.load()),
                static_cast<unsigned>(report.ReclaimFailure.load()),
                CompletedPhase[0U].load(),
                CompletedPhase[1U].load()
            );
            std::fflush(stdout);
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
#if defined(EDP_WORKER_DUAL_POOL_PROFILE)
            Require(
                memoryB.TearDown() == M::MemoryTopologyTeardownResult::Succeeded,
                "WORKER_SECOND_SOURCE_MEMORY_TEARDOWN"
            );
#endif
            Require(
                encodingMemory.TearDown() == M::MemoryTopologyTeardownResult::Succeeded,
                "WORKER_ENCODING_MEMORY_TEARDOWN"
            );
            if constexpr (ScheduledRecords > 6000U) {
                Require(
                    ScheduledLatencyCount == ScheduledRecords,
                    "WORKER_ALL_SCHEDULED_LATENCY_SAMPLES"
                );
                std::printf(
                    "EDP_WORKER|ALL_SCHEDULED_LATENCY|records=%u|min_us=%u|max_us=%u|mean_us=%llu\n",
                    ScheduledLatencyCount,
                    ScheduledLatencyMinimumUs,
                    ScheduledLatencyMaximumUs,
                    static_cast<unsigned long long>(ScheduledLatencySumUs / ScheduledLatencyCount)
                );
            }
            std::sort(
                AckLatencies.begin() + AckSampleStart,
                AckLatencies.begin() + AckSampleStart + AckSampleCount
            );
#if defined(EDP_TEST_BYTE_CAPACITY)
            Require(
                ObservedSinkPeakBytes > 0U && ObservedSinkPeakBytes <= WorkerSinkByteCapacity,
                "BYTE_CAPACITY_FINAL_PEAK"
            );
            std::printf(
                "EDP_BYTE_CAPACITY|PASS|limit=%llu|peak=%llu\n",
                static_cast<unsigned long long>(WorkerSinkByteCapacity),
                static_cast<unsigned long long>(ObservedSinkPeakBytes)
            );
#endif
            std::printf(
                "EDP_WORKER|PASS|scheduled_records=%u|producers=2|target_rps=%u|elapsed_us=%llu"
                "|latency_scope=%s|latency_samples=%u|p50_us=%u|p95_us=%u|p99_us=%u|max_schedule_late_us=%u|min_heap=%u|producer0_"
                "stack=%u"
                "|producer1_stack=%u|worker_stack=%u|producer_core_mask=%u|worker_core_mask=%u"
                "|queue_contentions=%llu|main_stack=%u|sizeof_source=%u|sizeof_queue=%u"
                "|scheduled_dwell_ns=%llu|application_optimisation=%s|cpu_mhz=%u|ack_segment=%llu\n",
                ScheduledRecords,
                ScheduleRps,
                static_cast<unsigned long long>(elapsedUs),
                ScheduledRecords > 6000U ? "last_6000" : "all_scheduled",
                static_cast<unsigned>(AckSampleCount),
                static_cast<unsigned>(AckLatencies[AckSampleStart + AckSampleCount * 50U / 100U]),
                static_cast<unsigned>(AckLatencies[AckSampleStart + AckSampleCount * 95U / 100U]),
                static_cast<unsigned>(AckLatencies[AckSampleStart + AckSampleCount * 99U / 100U]),
                MaximumScheduleLatenessUs.load(),
                minimumHeap,
                StackMargins[0U].load(),
                StackMargins[1U].load(),
                StackMargins[2U].load(),
                ProducerCoreMask.load(),
                WorkerCoreMask.load(),
                static_cast<unsigned long long>(report.QueueContentions.load()),
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                static_cast<unsigned>(sizeof(Source)),
                static_cast<unsigned>(sizeof(Queue)),
                static_cast<unsigned long long>(WorkerDwellNanoseconds),
                WorkerOptimisation,
                static_cast<unsigned>(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ),
                static_cast<unsigned long long>(ack)
            );
            std::fflush(stdout);
            for (;;)
                vTaskDelay(pdMS_TO_TICKS(1000U));
        }

    } // namespace BatchWorkerHIL

} // anonymous namespace
