#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Memory.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>
#include <ESPressio_Threading.hpp>
#include <clock/MonotonicClockProvider.hpp>
#include <execution/ExecutionContextProvider.hpp>
#include <logging/AsyncCopyingTaskSink.hpp>
#include <memory/MemoryResourceProvider.hpp>
#include <synchronization/SpinLockProvider.hpp>
#include <timing/EspTimerMonotonicTimebaseProvider.hpp>

namespace ESPressio::Demos::OwnedAsyncSink {

    namespace C = ESPressio::Clock;
    namespace L = ESPressio::Logging;
    namespace M = ESPressio::Memory;
    namespace P = ESPressio::Platform;
    namespace T = ESPressio::Threading;
    namespace CF = ESPressio::System::CompositionFramework;

    /// One-shot Bootstrap, delivery and owner-shutdown disposition.
    enum class Result : std::uint8_t {
        /// Independent copied bytes were consumed and every runtime was reclaimed.
        Completed = 0U,
        /// The application could not bind its canonical monotonic Clock.
        ClockUnavailable = 1U,
        /// The finite Memory topology could not initialize.
        MemoryInitializationFailed = 2U,
        /// The selected Threading topology could not initialize.
        ThreadingInitializationFailed = 3U,
        /// The selected topology could not start its existing Worker.
        ThreadingStartFailed = 4U,
        /// The nonwaiting Task admission policy refused dispatch.
        DispatchRejected = 5U,
        /// No terminal Task was observed inside the finite polling interval.
        CompletionUnconfirmed = 6U,
        /// Delivery or independent Memory release was unconfirmed.
        DeliveryUnconfirmed = 7U,
        /// The Threading owner could not confirm shutdown and finalization.
        ShutdownUnconfirmed = 8U,
        /// Memory could not confirm final topology teardown.
        MemoryTeardownUnconfirmed = 9U
    };

    /// Selected existing raw Memory resource.
    using Resource = P::Portable::Memory::MemoryResourceProvider;
    /// Selected existing ordinary mutex.
    using Mutex = P::FreeRTOS::Synchronization::MutexProvider;
    /// Selected existing latched signal.
    using Signal = P::FreeRTOS::Synchronization::SignalProvider;
    /// Selected existing Memory copy/compare capability.
    using Bytes = P::Portable::Memory::ByteOperationsProvider;
    /// Application pool object whose lifetime is owned by EDP-Memory.
    using Payload = L::OwnedEncodedPayload<32U>;
    /// Exactly one independent payload slot and no shared reserve.
    using MemoryTopology = M::MemoryTopology<
        Resource,
        M::SharedReserve<0U, Resource>,
        M::ObjectPoolSpec<Payload, M::DedicatedInstances<1U>>
    >;
    /// Application-selected Memory resource composition.
    using MemoryComposition = M::MemoryComposition<Resource>;
    /// Existing allocation provider selected for the zero-sized shared reserve.
    using Allocator = MemoryComposition::Select<
        M::SharedReserveAllocationRequirement,
        CF::SelectUnique
    >;
    /// Runtime owner of the fixed payload capacity.
    using MemoryRuntime = M::MemoryRuntime<MemoryTopology, MemoryComposition, Mutex, Signal>;

    /// Synchronous callback executed inside the existing Threading Worker.
    struct Delivery final {

        // Bootstrap-controlled copy-lifetime demonstration and observations.

        /// Prevents reading the payload before Bootstrap mutates its original source.
        std::atomic<bool> ReadAllowed{false};
        /// Number of actual Worker callback entries.
        std::atomic<unsigned> Calls{0U};
        /// Whether the Worker observed the exact original encoded bytes.
        std::atomic<bool> Matched{false};

        /// Consumes borrowed bytes while the Task's independent Memory lease remains live.
        L::RecordSinkSubmission operator()(
            L::EdlgCodec codec,
            std::span<const std::byte> encoded
        ) noexcept {
            Calls.fetch_add(1U);
            const auto started = C::MonotonicNow();
            while (!ReadAllowed.load(std::memory_order_acquire)) {
                if (C::Delta(
                    started,
                    C::MonotonicNow()
                ).Nanoseconds() >= 2000000000LL) {
                    return L::RecordSinkSubmission::ProviderFailure;
                }
                vTaskDelay(1U);
            }
            constexpr std::array<std::byte, 4U> expected{
                std::byte{0x83U},
                std::byte{0x01U},
                std::byte{0x02U},
                std::byte{0x03U}
            };
            const Bytes bytes{};
            const bool matched =
                codec == L::EdlgCodec::Cbor &&
                encoded.size() == expected.size() &&
                bytes.CompareBytes(
                    encoded.data(),
                    expected.data(),
                    expected.size()
                ) == M::ByteComparison::Equal;
            Matched.store(matched, std::memory_order_release);
            return matched ? L::RecordSinkSubmission::Accepted
                           : L::RecordSinkSubmission::Rejected;
        }
    };

    /// Existing Logging Task callable with independently leased payload bytes.
    using Work = L::AsyncOwnedDeliveryWork<L::EdlgCodec::Cbor, 32U, Delivery>;
    /// One topology-owned persistent Worker for the selected callable.
    using TaskTopology = T::ThreadingTopology<
        T::DedicatedWorkerLease<
            Work,
            T::TaskRecordCapacity<1U>,
            T::CallableCapacity<64U>,
            T::ResultCapacity<16U>,
            T::StackCapacity<6144U>,
            T::Priority<T::ThreadPriority::Normal>,
            T::AnyAffinity
        >
    >;
    /// Native runtime that owns execution, synchronization and Task records.
    using TaskRuntime = T::StaticThreadingRuntime<
        TaskTopology,
        std::tuple<>,
        Signal,
        P::ESPIDF::Execution::ExecutionContextProvider,
        P::ESPIDF::Synchronization::SpinLockProvider,
        Mutex
    >;

    /// Application binding of AsyncCopyingTaskSink dispatch to its existing Dedicated Worker.
    struct Executor final {

        /// Borrowed topology owner retained through terminal Task handling.
        TaskRuntime *Runtime;

        /// Delegates the exact callable and admission policy to EDP-Threading.
        auto Dispatch(
            Work work,
            T::TaskDispatchPolicy policy
        ) {
            return Runtime->DispatchDedicated<Work>(
                std::move(work),
                policy
            );
        }
    };

    /// Runs one finite Bootstrap composition. Static owners survive uncertain failure returns.
    [[nodiscard]] inline Result Run() noexcept {
        using Timebase = P::ESPIDF::Timing::EspTimerMonotonicTimebaseProvider;
        static Timebase timebase{};
        static C::MonotonicClockProvider<Timebase> clock(timebase);
        if (!C::BindMonotonicClock(clock)) {
            return Result::ClockUnavailable;
        }

        // These application-owned resources stay alive until verified teardown.
        static Resource resource{};
        static Mutex mutex{};
        static Allocator allocator{};
        static MemoryRuntime memory(
            mutex,
            allocator,
            resource
        );
        M::MemoryTopologyInitializationFailure failure{};
        if (memory.Initialize(failure) != M::MemoryTopologyInitializationResult::Succeeded) {
            return Result::MemoryInitializationFailed;
        }

        auto &pool = memory.ObjectPoolFor<Payload>();
        using Pool = std::remove_reference_t<decltype(pool)>;
        static typename Pool::LeaseType lease{};
        static std::optional<T::Task<L::RecordSinkSubmission>> task{};
        static Bytes bytes{};
        static Delivery delivery{};
        static TaskRuntime runtime(std::tuple<>{});
        if (runtime.Initialize() != T::ThreadingInitializationResult::Succeeded) {
            return Result::ThreadingInitializationFailed;
        }
        if (runtime.Start() != T::ThreadingStartResult::Succeeded) {
            return Result::ThreadingStartFailed;
        }
        static Executor executor{&runtime};
        static L::AsyncCopyingTaskSink<
            L::EdlgCodec::Cbor,
            32U,
            Pool,
            Bytes,
            Executor,
            Delivery
        > sink(
            pool,
            lease,
            task,
            bytes,
            executor,
            delivery
        );
        std::array<std::byte, 4U> source{
            std::byte{0x83U},
            std::byte{0x01U},
            std::byte{0x02U},
            std::byte{0x03U}
        };
        if (sink.Submit(
            L::EdlgCodec::Cbor,
            source
        ) != L::AsyncSinkAdmissionStatus::Queued) {
            return Result::DispatchRejected;
        }

        // The admitted Task owns a separate Memory copy before source mutation.
        source[1U] = std::byte{0xFFU};
        delivery.ReadAllowed.store(
            true,
            std::memory_order_release
        );
        const auto completionStarted = C::MonotonicNow();
        while (!task->IsFinished() &&
               C::Delta(
                   completionStarted,
                   C::MonotonicNow()
               ).Nanoseconds() < 2000000000LL) {
            vTaskDelay(1U);
        }
        const auto completed = sink.TryFinalize();
        if (completed.Status == L::AsyncSinkCompletionStatus::Pending) {
            return Result::CompletionUnconfirmed;
        }
        if (completed.Status != L::AsyncSinkCompletionStatus::AcceptedAndReleased ||
            completed.Release != M::ObjectPoolLeaseReleaseResult::Released ||
            !lease.IsEmpty() || task.has_value() ||
            delivery.Calls.load() != 1U || !delivery.Matched.load(std::memory_order_acquire)) {
            return Result::DeliveryUnconfirmed;
        }
        if (runtime.BeginShutdown() != T::ThreadingShutdownResult::Accepted) {
            return Result::ShutdownUnconfirmed;
        }
        const auto shutdownStarted = C::MonotonicNow();
        while (!runtime.IsExecutionQuiescent() &&
               C::Delta(
                   shutdownStarted,
                   C::MonotonicNow()
               ).Nanoseconds() < 2000000000LL) {
            vTaskDelay(1U);
        }
        if (!runtime.IsExecutionQuiescent() ||
            runtime.FinalizeShutdown() != T::ThreadingFinalizationResult::Completed) {
            return Result::ShutdownUnconfirmed;
        }
        if (memory.TearDown() != M::MemoryTopologyTeardownResult::Succeeded) {
            return Result::MemoryTeardownUnconfirmed;
        }
        return Result::Completed;
    }

} // namespace ESPressio::Demos::OwnedAsyncSink
