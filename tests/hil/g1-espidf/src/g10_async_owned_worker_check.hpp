#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

#include <logging/AsyncCopyingTaskSink.hpp>
#include <memory/ByteOperationsProvider.hpp>

namespace Test {

    /// Existing HIL assertion/Clock utilities are implemented by main.cpp.
    bool Check(bool value, const char* label) noexcept;
    ESPressio::Clock::MonotonicTimestamp Deadline(std::uint64_t ms) noexcept;

    /// Faults one Memory lock acquisition and otherwise delegates to the native mutex.
    /// @tparam TNativeMutex Selected native ordinary mutex provider.
    template<class TNativeMutex>
    class G10MemoryReleaseMutex final {
        using LockAcquireResult = ESPressio::Platform::Synchronization::LockAcquireResult;
        using LockReleaseResult = ESPressio::Platform::Synchronization::LockReleaseResult;
        using WaitTimeout = ESPressio::Platform::Synchronization::WaitTimeout;

        /// Native provider used for every acquisition outside the injected fault.
        TNativeMutex _native;
        /// Bootstrap-only countdown. Only the test's Memory runtime uses this instance.
        unsigned _acquiresUntilFailure{0U};

      public:
        /// Preserves the selected native provider's composition identity.
        using CompositionDomain = typename TNativeMutex::CompositionDomain;
        /// Preserves the selected native mutex capability and wait resolution.
        using CompositionOffers = typename TNativeMutex::CompositionOffers;

        /// Arms exactly one acquisition failure.
        void FailOnAcquire(unsigned count) noexcept {
            _acquiresUntilFailure = count;
        }

        /// Forwards the requested native wait unless the selected acquisition faults.
        LockAcquireResult Acquire(WaitTimeout timeout) noexcept {
            if (_acquiresUntilFailure != 0U && --_acquiresUntilFailure == 0U) {
                return LockAcquireResult::ProviderFailure;
            }
            return _native.Acquire(timeout);
        }

        /// Releases through the native provider.
        LockReleaseResult Release() noexcept {
            return _native.Release();
        }
    };

    /// Real ESP32 task execution and native independently owned representation,
    /// not the synthetic deferred executor used in host unit tests.
    template<
        class TMemoryResource,
        class TMutex,
        class TSignal,
        class TAllocator,
        class TMemoryComposition
    >
    bool CheckG10AsyncOwnedWorkerOnDevice() noexcept {
        namespace L = ESPressio::Logging;
        namespace M = ESPressio::Memory;
        namespace T = ESPressio::Threading;
        namespace P = ESPressio::Platform;

        // The application Bootstrap owns exactly one isolated test Worker.
        // This dedicated HIL topology is not a second product runtime.
        struct NativeSink final {

            /// Number of actual Task callback entries.
            std::atomic<unsigned> Calls{0U};

            /// Exact captured payload second byte after source was mutated.
            std::atomic<unsigned> Captured{0U};

            /// Keeps a running delivery active while Bootstrap exercises its lifecycle.
            std::atomic<bool> Hold{false};

            /// Final native callback outcome, distinct from Task dispatch.
            L::RecordSinkSubmission Result{L::RecordSinkSubmission::Accepted};

            /// Worker thread's synchronous consumption of copied payload bytes.
            L::RecordSinkSubmission operator()(
                L::EdlgCodec codec,
                std::span<const std::byte> bytes
            ) noexcept {
                if (codec != L::EdlgCodec::Cbor || bytes.size() != 3U) {
                    return L::RecordSinkSubmission::ProviderFailure;
                }
                Calls.fetch_add(1U, std::memory_order_release);
                const auto bound = Deadline(4000U);
                while (Hold.load(std::memory_order_acquire)) {
                    if (ESPressio::Clock::MonotonicNow() >= bound) {
                        return L::RecordSinkSubmission::ProviderFailure;
                    }
                    vTaskDelay(1U);
                }
                Captured.store(
                    static_cast<unsigned>(
                        std::to_integer<std::uint8_t>(bytes[1U])
                    ),
                    std::memory_order_release
                );
                return Result;
            }

        };

        using Buffer = L::OwnedEncodedPayload<32U>;
        using MemTopology = M::MemoryTopology<
            TMemoryResource,
            M::SharedReserve<0U, TMemoryResource>,
            M::ObjectPoolSpec<Buffer, M::DedicatedInstances<2U>>
        >;
        using MemRuntime = M::MemoryRuntime<
            MemTopology, TMemoryComposition, G10MemoryReleaseMutex<TMutex>, TSignal
        >;
        using Work = L::AsyncOwnedDeliveryWork<
            L::EdlgCodec::Cbor, 32U, NativeSink
        >;
        using TaskTopology = T::ThreadingTopology<
            T::DedicatedWorkerLease<
                Work,
                T::TaskRecordCapacity<2U>,
                T::CallableCapacity<64U>,
                T::ResultCapacity<16U>,
                T::StackCapacity<6144U>,
                T::Priority<T::ThreadPriority::Normal>,
                T::AnyAffinity
            >
        >;
        using Native = T::StaticThreadingRuntime<
            TaskTopology,
            std::tuple<>,
            TSignal,
            P::ESPIDF::Execution::ExecutionContextProvider,
            P::ESPIDF::Synchronization::SpinLockProvider,
            TMutex
        >;

        /// Narrow app-owned binding of the actual EDP-Threading Dedicated Worker.
        struct Executor final {

            /// The stable EDP-Threading Runtime selected by test Bootstrap.
            Native* Runtime;

            /// Dispatches the exact public Logging callable Type to Threading.
            auto Dispatch(
                Work work,
                T::TaskDispatchPolicy policy
            ) {
                return Runtime->template DispatchDedicated<Work>(
                    std::move(work),
                    policy
                );
            }

        };

        using ByteOps = P::Portable::Memory::ByteOperationsProvider;
        TMemoryResource resource;
        G10MemoryReleaseMutex<TMutex> nativeMutex;
        TAllocator allocator;
        MemRuntime memory(nativeMutex, allocator, resource);
        M::MemoryTopologyInitializationFailure initialization{};
        if (!Check(
            memory.Initialize(initialization) ==
                M::MemoryTopologyInitializationResult::Succeeded,
            "g10-async-native-memory-init"
        )) {
            return false;
        }

        auto& pool = memory.template ObjectPoolFor<Buffer>();
        using Pool = std::remove_reference_t<decltype(pool)>;
        using Lease = typename Pool::LeaseType;
        Lease firstLease;
        Lease secondLease;
        std::optional<T::Task<L::RecordSinkSubmission>> firstTask;
        std::optional<T::Task<L::RecordSinkSubmission>> secondTask;
        ByteOps bytes;
        static Native taskRuntime(std::tuple<>{});
        if (!Check(
            taskRuntime.Initialize() == T::ThreadingInitializationResult::Succeeded,
            "g10-async-worker-init"
        )) {
            return false;
        }
        if (!Check(
            taskRuntime.Start() == T::ThreadingStartResult::Succeeded,
            "g10-async-worker-start"
        )) {
            return false;
        }

        static NativeSink firstSink{};
        static NativeSink secondSink{};
        firstSink.Hold.store(true, std::memory_order_relaxed);
        secondSink.Result = L::RecordSinkSubmission::Rejected;

        Executor executor{&taskRuntime};
        using Adapter = L::AsyncCopyingTaskSink<
            L::EdlgCodec::Cbor,
            32U,
            Pool,
            ByteOps,
            Executor,
            NativeSink
        >;
        Adapter first(
            pool, firstLease, firstTask, bytes, executor, firstSink
        );
        Adapter second(
            pool, secondLease, secondTask, bytes, executor, secondSink
        );

        std::array<std::byte, 3U> original{
            std::byte{0x81U},
            std::byte{0xA1U},
            std::byte{0x02U}
        };
        if (!Check(
            first.Submit(L::EdlgCodec::Cbor, original) ==
                L::AsyncSinkAdmissionStatus::Queued,
            "g10-async-first-task-admitted"
        )) {
            return false;
        }

        // Wait until the actual EDP-Threading Worker has entered the
        // deliberately slow callback, keeping one native Memory owner live.
        const auto startDeadline = Deadline(1500U);
        while (
            firstSink.Calls.load(std::memory_order_acquire) == 0U &&
            ESPressio::Clock::MonotonicNow() < startDeadline
        ) {
            vTaskDelay(pdMS_TO_TICKS(5U));
        }
        if (!Check(
            firstSink.Calls.load(std::memory_order_acquire) == 1U &&
            !firstLease.IsEmpty(),
            "g10-async-real-worker-entered"
        )) {
            return false;
        }

        const std::array<std::byte, 3U> later{
            std::byte{0x81U},
            std::byte{0xB2U},
            std::byte{0x03U}
        };
        // EDP-Threading::AbandonImmediately is genuinely nonwaiting:
        // a busy Dedicated Worker rejects this admission instead of
        // borrowing the first sink's lease or secretly queuing work.
        if (!Check(
            second.Submit(L::EdlgCodec::Cbor, later) ==
                L::AsyncSinkAdmissionStatus::DispatchRejected &&
            !secondTask.has_value() && secondLease.IsEmpty() &&
            secondSink.Calls.load() == 0U,
            "g10-async-busy-worker-finite-rejection"
        )) {
            return false;
        }
        original[1U] = std::byte{0xFFU};

        // A genuinely running native Task may accept a cancellation request
        // without acknowledging it. Ownership MUST remain through its real
        // terminal callback, not be reclaimed when Cancel first returns.
        if (!Check(
            first.TryFinalize().Status ==
                L::AsyncSinkCompletionStatus::Pending &&
            first.RequestCancellation() == T::TaskCancelResult::Accepted &&
            first.TryFinalize().Status ==
                L::AsyncSinkCompletionStatus::Pending &&
            !firstLease.IsEmpty(),
            "g10-async-running-cancel-retains-memory"
        )) {
            return false;
        }

        firstSink.Hold.store(false, std::memory_order_release);
        const auto finishDeadline = Deadline(2000U);
        while (
            firstTask.has_value() &&
            !firstTask->IsFinished() &&
            ESPressio::Clock::MonotonicNow() < finishDeadline
        ) {
            vTaskDelay(pdMS_TO_TICKS(5U));
        }
        const auto finished = first.TryFinalize();
        if (!Check(
            finished.Status == L::AsyncSinkCompletionStatus::AcceptedAndReleased &&
            firstLease.IsEmpty() && !firstTask.has_value() &&
            firstSink.Captured.load(std::memory_order_acquire) == 0xA1U,
            "g10-async-native-owner-survived-source-mutation"
        )) {
            return false;
        }

        // Once the original Task finishes and its independent lease is
        // reclaimed, the same bounded worker admits another Task normally.
        if (!Check(
            second.Submit(L::EdlgCodec::Cbor, later) ==
                L::AsyncSinkAdmissionStatus::Queued,
            "g10-async-worker-after-busy-retry"
        )) {
            return false;
        }
        const auto repeatDeadline = Deadline(2000U);
        while (
            secondTask.has_value() &&
            !secondTask->IsFinished() &&
            ESPressio::Clock::MonotonicNow() < repeatDeadline
        ) {
            vTaskDelay(pdMS_TO_TICKS(5U));
        }
        // Fail Memory's second coordination acquisition, after object destruction
        // and before the dedicated slot can be returned. Threading uses its own mutex.
        if (!Check(
            secondTask.has_value() && secondTask->IsCompleted(),
            "g10-async-native-fault-task-completed"
        )) {
            return false;
        }
        nativeMutex.FailOnAcquire(2U);
        const auto rejected = second.TryFinalize();
        if (!Check(
            rejected.Status == L::AsyncSinkCompletionStatus::ReclaimUnconfirmed &&
            rejected.Delivery == L::RecordSinkSubmission::Rejected &&
            rejected.Release == M::ObjectPoolLeaseReleaseResult::ProviderFailure &&
            !secondTask.has_value() && !secondLease.IsEmpty() &&
            secondLease.Get() == nullptr &&
            secondSink.Calls.load() == 1U &&
            secondSink.Captured.load() == 0xB2U &&
            second.Submit(L::EdlgCodec::Cbor, later) ==
                L::AsyncSinkAdmissionStatus::Busy,
            "g10-async-native-release-fault-retains-slot"
        )) {
            return false;
        }
        if (!Check(
            second.RetryReclaim() == M::ObjectPoolLeaseReleaseResult::Released &&
            second.RetryReclaim() == M::ObjectPoolLeaseReleaseResult::AlreadyEmpty &&
            secondLease.IsEmpty(),
            "g10-async-native-release-fault-retry"
        )) {
            return false;
        }
        const auto noWait = P::Synchronization::WaitTimeout::NoWait();
        if (!Check(
            pool.Acquire(firstLease, noWait) == M::ObjectPoolAcquisitionResult::Succeeded &&
            pool.Acquire(secondLease, noWait) == M::ObjectPoolAcquisitionResult::Succeeded &&
            firstLease.Release() == M::ObjectPoolLeaseReleaseResult::Released &&
            secondLease.Release() == M::ObjectPoolLeaseReleaseResult::Released,
            "g10-async-native-release-fault-full-capacity"
        )) {
            return false;
        }
        std::printf("EDP_LOGGING_HIL|PASS|G10_ASYNC_NATIVE_MEMORY_RELEASE_FAULT\n");

        // Begin real Threading shutdown while the Worker owns and reads its copy.
        firstSink.Hold.store(true, std::memory_order_relaxed);
        if (!Check(
            first.Submit(L::EdlgCodec::Cbor, later) ==
                L::AsyncSinkAdmissionStatus::Queued,
            "g10-async-native-shutdown-task-admitted"
        )) {
            return false;
        }
        const auto shutdownEntry = Deadline(2000U);
        while (
            firstSink.Calls.load(std::memory_order_acquire) != 2U &&
            ESPressio::Clock::MonotonicNow() < shutdownEntry
        ) {
            vTaskDelay(1U);
        }
        const bool workerEntered = firstSink.Calls.load(std::memory_order_acquire) == 2U;
        const auto began = taskRuntime.BeginShutdown();
        const bool retainedDuringShutdown =
            workerEntered && firstTask.has_value() && !firstTask->IsFinished() &&
            first.TryFinalize().Status == L::AsyncSinkCompletionStatus::Pending &&
            !firstLease.IsEmpty() && !taskRuntime.IsExecutionQuiescent() &&
            taskRuntime.FinalizeShutdown() ==
                T::ThreadingFinalizationResult::ExecutionNotQuiescent;
        firstSink.Hold.store(false, std::memory_order_release);

        const auto shutdownBound = Deadline(2000U);
        while (
            ((firstTask.has_value() && !firstTask->IsFinished()) ||
                !taskRuntime.IsExecutionQuiescent()) &&
            ESPressio::Clock::MonotonicNow() < shutdownBound
        ) {
            vTaskDelay(1U);
        }
        const auto shutdownTask = first.TryFinalize();
        const bool releasedBeforeTeardown =
            shutdownTask.Status == L::AsyncSinkCompletionStatus::AcceptedAndReleased &&
            firstLease.IsEmpty() && !firstTask.has_value();
        if (!Check(
            workerEntered && began == T::ThreadingShutdownResult::Accepted &&
            retainedDuringShutdown && releasedBeforeTeardown &&
            firstSink.Captured.load(std::memory_order_acquire) == 0xB2U &&
            taskRuntime.IsExecutionQuiescent() &&
            taskRuntime.FinalizeShutdown() == T::ThreadingFinalizationResult::Completed,
            "g10-async-native-worker-shutdown"
        )) {
            return false;
        }
        std::printf("EDP_LOGGING_HIL|PASS|G10_ASYNC_NATIVE_ACTIVE_SHUTDOWN\n");
        if (!Check(
            memory.TearDown() == M::MemoryTopologyTeardownResult::Succeeded,
            "g10-async-native-memory-teardown"
        )) {
            return false;
        }
        std::printf("EDP_LOGGING_HIL|PASS|G10_ASYNC_TASK_OWNED_WORKER\n");
        return true;
    }

} // Test
