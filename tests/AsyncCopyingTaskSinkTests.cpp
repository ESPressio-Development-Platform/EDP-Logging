#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <new>
#include <optional>
#include <span>
#include <type_traits>

#define EDP_LOGGING_G1_LIFECYCLE_TEST_INCLUDE
#include "AdmissionWakeCoordinatorTests.cpp"
#include <logging/AsyncCopyingTaskSink.hpp>
#include <memory/ByteOperationsProvider.hpp>

namespace {

    namespace L = ESPressio::Logging;
    namespace M = ESPressio::Memory;
    namespace T = ESPressio::Threading;
    namespace P = ESPressio::Platform;

    using Buffer = L::OwnedEncodedPayload<32U>;
    using AsyncTopology = M::MemoryTopology<
        Resource,
        M::SharedReserve<0U, Resource>,
        M::ObjectPoolSpec<Buffer, M::DedicatedInstances<1U>>
    >;
    using MemoryRuntime = M::MemoryRuntime<
        AsyncTopology,
        MemoryComposition,
        TestSupport::TestMutexProvider,
        TestSupport::TestSignalProvider
    >;
    using PortableBytes = P::Portable::Memory::ByteOperationsProvider;

    /// Test-only deterministic Task completion driver using the actual
    /// EDP-Threading Task<TaskResult> API, not a production scheduler.
    struct DeferredTaskExecutor final {

        /// One synthetic Task lifecycle, manipulated by the test only.
        struct State final {
            T::TaskState Lifecycle{T::TaskState::Queued};
            L::RecordSinkSubmission Delivered{L::RecordSinkSubmission::ProviderFailure};
            std::size_t Releases{0U};
        } Current{};

        /// Deliberately deferred callable, not invoked by Dispatch.
        std::function<L::RecordSinkSubmission()> Deferred{};

        /// Rejection is determined before a Task is admitted.
        bool FailDispatch{false};

        /// Counts Task dispatch requests.
        std::size_t Attempts{0U};

        /// Canonical Task handle operations satisfy real EDP-Threading public ABI.
        static constexpr T::Detail::TaskHandleOperations Operations{
            [](const void* state, std::uint32_t, bool) noexcept -> T::TaskState {
                return static_cast<const State*>(state)->Lifecycle;
            },
            [](void* state, std::uint32_t, bool) -> T::TaskWaitResult {
                const auto s = static_cast<State*>(state)->Lifecycle;
                return s == T::TaskState::Completed || s == T::TaskState::Cancelled
                    ? T::TaskWaitResult::Finished
                    : T::TaskWaitResult::TimedOut;
            },
            [](void* state, std::uint32_t, bool, T::Duration) -> T::TaskWaitResult {
                const auto s = static_cast<State*>(state)->Lifecycle;
                return s == T::TaskState::Completed || s == T::TaskState::Cancelled
                    ? T::TaskWaitResult::Finished
                    : T::TaskWaitResult::TimedOut;
            },
            [](void* state, std::uint32_t, bool, T::MonotonicTimestamp) -> T::TaskWaitResult {
                const auto s = static_cast<State*>(state)->Lifecycle;
                return s == T::TaskState::Completed || s == T::TaskState::Cancelled
                    ? T::TaskWaitResult::Finished
                    : T::TaskWaitResult::TimedOut;
            },
            [](void* state, std::uint32_t, bool) noexcept -> T::TaskCancelResult {
                auto* s = static_cast<State*>(state);
                if (s->Lifecycle == T::TaskState::Completed ||
                    s->Lifecycle == T::TaskState::Cancelled) {
                    return T::TaskCancelResult::AlreadyFinished;
                }
                s->Lifecycle = T::TaskState::Cancelled;
                return T::TaskCancelResult::Accepted;
            },
            [](void* state, std::uint32_t, bool) noexcept {
                ++static_cast<State*>(state)->Releases;
            },
            [](void* state, std::uint32_t, bool, void* destination) -> T::TaskTakeStatus {
                auto* s = static_cast<State*>(state);
                if (s->Lifecycle == T::TaskState::Cancelled) {
                    return T::TaskTakeStatus::Cancelled;
                }
                if (s->Lifecycle != T::TaskState::Completed) {
                    return T::TaskTakeStatus::NotCompleted;
                }
                new (destination) L::RecordSinkSubmission(s->Delivered);
                ++s->Releases;
                return T::TaskTakeStatus::Succeeded;
            }
        };

        /// Stores an admitted callable until an independent test step executes it.
        template<class TWork>
        T::TaskDispatchResult<T::Task<L::RecordSinkSubmission>> Dispatch(
            TWork work,
            T::TaskDispatchPolicy policy
        ) {
            assert(policy == T::TaskDispatchPolicy::AbandonImmediately);
            ++Attempts;
            if (FailDispatch) {
                return T::TaskDispatchResult<T::Task<L::RecordSinkSubmission>>(
                    T::TaskDispatchStatus::Unavailable
                );
            }
            assert(!Deferred);
            Current.Lifecycle = T::TaskState::Queued;
            Deferred = [work]() noexcept { return work(); };
            return T::TaskDispatchResult<T::Task<L::RecordSinkSubmission>>(
                T::Task<L::RecordSinkSubmission>(&Current, 0U, false, Operations)
            );
        }

        /// Completes the pending Task only after the original input may be gone.
        void Execute() {
            assert(Deferred);
            Current.Lifecycle = T::TaskState::Running;
            Current.Delivered = Deferred();
            Deferred = {};
            Current.Lifecycle = T::TaskState::Completed;
        }

        /// Test-only cancellation acknowledgment: queued callable is never invoked.
        void DropCancelled() {
            assert(Current.Lifecycle == T::TaskState::Cancelled);
            Deferred = {};
        }
    };

    /// Copies incoming bytes only while the independently owned worker payload lives.
    struct Delivery final {
        std::array<std::byte, 32U> Bytes{};
        std::size_t Used{0U};
        std::size_t Calls{0U};
        L::RecordSinkSubmission Result{L::RecordSinkSubmission::Accepted};

        L::RecordSinkSubmission operator()(
            L::EdlgCodec codec,
            std::span<const std::byte> source
        ) noexcept {
            assert(codec == L::EdlgCodec::Cbor);
            ++Calls;
            Used = source.size();
            for (std::size_t i = 0U; i < source.size(); ++i) {
                Bytes[i] = source[i];
            }
            return Result;
        }
    };

    // The dispatched callable must be externally nameable so that application
    // Bootstrap can declare a genuine EDP-Threading DedicatedWorkerLease.
    using PublicDeliveryWork = L::AsyncOwnedDeliveryWork<
        L::EdlgCodec::Cbor, 32U, Delivery
    >;
    using DedicatedDeliveryWorker = T::DedicatedWorkerLease<
        PublicDeliveryWork,
        T::TaskRecordCapacity<1U>,
        T::CallableCapacity<sizeof(PublicDeliveryWork)>,
        T::ResultCapacity<sizeof(L::RecordSinkSubmission)>,
        T::StackCapacity<4096U>,
        T::Priority<T::ThreadPriority::Normal>,
        T::AnyAffinity
    >;
    static_assert(DedicatedDeliveryWorker::RecordCapacity == 1U);
    static_assert(DedicatedDeliveryWorker::CallableStorageCapacity ==
        sizeof(PublicDeliveryWork));
    static_assert(DedicatedDeliveryWorker::ResultStorageCapacity ==
        sizeof(L::RecordSinkSubmission));

    void TestIndependentCopyCompletion() {
        Resource resource;
        TestSupport::TestMutexProvider native;
        MemoryComposition::Select<
            M::SharedReserveAllocationRequirement,
            ESPressio::System::CompositionFramework::SelectUnique
        > allocator;
        MemoryRuntime memory(native, allocator, resource);
        M::MemoryTopologyInitializationFailure failure{};
        assert(memory.Initialize(failure) ==
            M::MemoryTopologyInitializationResult::Succeeded);
        auto& pool = memory.ObjectPoolFor<Buffer>();
        using Lease = std::remove_reference_t<decltype(pool)>::LeaseType;
        Lease owner;
        std::optional<T::Task<L::RecordSinkSubmission>> task;
        DeferredTaskExecutor executor;
        PortableBytes bytes;
        Delivery delivery;
        L::AsyncCopyingTaskSink<
            L::EdlgCodec::Cbor, 32U,
            std::remove_reference_t<decltype(pool)>,
            PortableBytes,
            DeferredTaskExecutor,
            Delivery
        > sink(pool, owner, task, bytes, executor, delivery);

        std::array<std::byte, 3U> source{
            std::byte{0x81U}, std::byte{0x42U}, std::byte{0x03U}
        };
        assert(sink(L::EdlgCodec::Cbor, source) ==
            L::RecordSinkSubmission::Accepted);
        assert(executor.Attempts == 1U && task.has_value() && !owner.IsEmpty());
        source[1U] = std::byte{0xEEU};
        assert(sink(L::EdlgCodec::Cbor, source) ==
            L::RecordSinkSubmission::Rejected);
        assert(sink.TryFinalize().Status == L::AsyncSinkCompletionStatus::Pending);
        assert(delivery.Calls == 0U);
        assert(task->WaitUntil(
            ESPressio::Clock::MonotonicTimestamp::FromNanoseconds(1U)
        ) == T::TaskWaitResult::TimedOut);
        executor.Execute();
        assert(delivery.Calls == 1U && delivery.Bytes[1U] == std::byte{0x42U});
        assert(task->WaitUntil(
            ESPressio::Clock::MonotonicTimestamp::FromNanoseconds(1U)
        ) == T::TaskWaitResult::Finished);
        const auto done = sink.TryFinalize();
        assert(done.Status == L::AsyncSinkCompletionStatus::AcceptedAndReleased);
        assert(done.Delivery == L::RecordSinkSubmission::Accepted);
        assert(done.Release == M::ObjectPoolLeaseReleaseResult::Released);
        assert(owner.IsEmpty() && !task.has_value());
        assert(sink.TryFinalize().Status == L::AsyncSinkCompletionStatus::NoPendingTask);

        delivery.Result = L::RecordSinkSubmission::Rejected;
        assert(sink.Submit(L::EdlgCodec::Cbor, source) ==
            L::AsyncSinkAdmissionStatus::Queued);
        executor.Execute();
        const auto rejected = sink.TryFinalize();
        assert(rejected.Status == L::AsyncSinkCompletionStatus::RejectedAndReleased);
        assert(rejected.Delivery == L::RecordSinkSubmission::Rejected);

        delivery.Result = L::RecordSinkSubmission::ProviderFailure;
        assert(sink.Submit(L::EdlgCodec::Cbor, source) ==
            L::AsyncSinkAdmissionStatus::Queued);
        executor.Execute();
        assert(sink.TryFinalize().Status ==
            L::AsyncSinkCompletionStatus::ProviderFailureAndReleased);

        assert(sink.Submit(L::EdlgCodec::Json, source) ==
            L::AsyncSinkAdmissionStatus::CodecMismatch);
        assert(sink.Submit(L::EdlgCodec::Cbor, std::span<const std::byte>{}) ==
            L::AsyncSinkAdmissionStatus::InvalidPayload);
        std::array<std::byte, 33U> tooLong{};
        assert(sink.Submit(L::EdlgCodec::Cbor, tooLong) ==
            L::AsyncSinkAdmissionStatus::InvalidPayload);
        assert(executor.Attempts == 3U && owner.IsEmpty());

        executor.FailDispatch = true;
        assert(sink.Submit(L::EdlgCodec::Cbor, source) ==
            L::AsyncSinkAdmissionStatus::DispatchRejected);
        assert(owner.IsEmpty() && !task.has_value());
        executor.FailDispatch = false;

        assert(sink.Submit(L::EdlgCodec::Cbor, source) ==
            L::AsyncSinkAdmissionStatus::Queued);
        assert(sink.RequestCancellation() == T::TaskCancelResult::Accepted);
        assert(delivery.Calls == 3U);
        executor.DropCancelled();
        const auto cancelled = sink.TryFinalize();
        assert(cancelled.Status == L::AsyncSinkCompletionStatus::CancelledAndReleased);
        assert(cancelled.Release == M::ObjectPoolLeaseReleaseResult::Released);
        assert(owner.IsEmpty() && !task.has_value());
        assert(memory.TearDown() == M::MemoryTopologyTeardownResult::Succeeded);
    }


    void TestTwoIndependentAsyncSinks() {
        Resource sourceResource;
        Resource firstResource;
        Resource secondResource;
        TestSupport::TestMutexProvider sourceMutex;
        TestSupport::TestMutexProvider firstMutex;
        TestSupport::TestMutexProvider secondMutex;
        MemoryComposition::Select<
            M::SharedReserveAllocationRequirement,
            ESPressio::System::CompositionFramework::SelectUnique
        > allocator;

        MemoryRuntime sourceRuntime(sourceMutex, allocator, sourceResource);
        MemoryRuntime firstRuntime(firstMutex, allocator, firstResource);
        MemoryRuntime secondRuntime(secondMutex, allocator, secondResource);
        M::MemoryTopologyInitializationFailure failure{};
        assert(sourceRuntime.Initialize(failure) ==
            M::MemoryTopologyInitializationResult::Succeeded);
        assert(firstRuntime.Initialize(failure) ==
            M::MemoryTopologyInitializationResult::Succeeded);
        assert(secondRuntime.Initialize(failure) ==
            M::MemoryTopologyInitializationResult::Succeeded);

        auto& sourcePool = sourceRuntime.ObjectPoolFor<Buffer>();
        auto& firstPool = firstRuntime.ObjectPoolFor<Buffer>();
        auto& secondPool = secondRuntime.ObjectPoolFor<Buffer>();
        using Pool = std::remove_reference_t<decltype(sourcePool)>;
        using Lease = Pool::LeaseType;
        Lease sourceLease;
        Lease firstOwner;
        Lease secondOwner;
        std::optional<T::Task<L::RecordSinkSubmission>> firstTask;
        std::optional<T::Task<L::RecordSinkSubmission>> secondTask;
        DeferredTaskExecutor firstExecutor;
        DeferredTaskExecutor secondExecutor;
        Delivery firstDelivery;
        Delivery secondDelivery;
        firstDelivery.Result = L::RecordSinkSubmission::Accepted;
        secondDelivery.Result = L::RecordSinkSubmission::Partial;
        PortableBytes bytes;
        using Sink = L::AsyncCopyingTaskSink<
            L::EdlgCodec::Cbor, 32U,
            Pool, PortableBytes, DeferredTaskExecutor, Delivery
        >;
        Sink first(
            firstPool, firstOwner, firstTask, bytes, firstExecutor, firstDelivery
        );
        Sink second(
            secondPool, secondOwner, secondTask, bytes, secondExecutor, secondDelivery
        );

        struct Record final {
            std::uint8_t Code;
        } record{0x27U};
        struct Encoder final {
            std::size_t Calls{0U};
            L::RepresentationEncodeResult operator()(
                const Record& value,
                std::span<std::byte> out
            ) noexcept {
                ++Calls;
                out[0U] = std::byte{0x81U};
                out[1U] = std::byte{value.Code};
                return {L::RepresentationEncodeStatus::Succeeded, 2U};
            }
        } encoder;

        const auto admission = L::EncodeOnceAndSubmitToSinks<
            L::EdlgCodec::Cbor, 32U
        >(
            sourcePool, sourceLease, record, encoder, first, second
        );
        assert(admission.Status == L::OwnedRepresentationStatus::SubmittedAndReleased);
        assert(admission.Accepted == 2U && admission.EncoderInvocations == 1U);
        assert(sourceLease.IsEmpty() && !firstOwner.IsEmpty() && !secondOwner.IsEmpty());
        assert(firstTask.has_value() && secondTask.has_value());
        record.Code = 0xFFU;

        // Independent Task completion ordering does not extend the original
        // source representation lifetime or let one sink reclaim the other.
        secondExecutor.Execute();
        const auto partial = second.TryFinalize();
        assert(partial.Status == L::AsyncSinkCompletionStatus::PartialAndReleased);
        assert(partial.Delivery == L::RecordSinkSubmission::Partial);
        assert(secondOwner.IsEmpty() && !firstOwner.IsEmpty());
        assert(secondDelivery.Bytes[1U] == std::byte{0x27U});
        assert(first.TryFinalize().Status == L::AsyncSinkCompletionStatus::Pending);

        // The first consumer remains busy even though the parent source and
        // other consumer are already free. New source has no rollback magic.
        const auto again = L::EncodeOnceAndSubmitToSinks<
            L::EdlgCodec::Cbor, 32U
        >(
            sourcePool, sourceLease, record, encoder, first, second
        );
        assert(again.Status == L::OwnedRepresentationStatus::SubmissionNotAccepted);
        assert(again.Accepted == 1U);
        assert(again.Submissions[0U] == L::RecordSinkSubmission::Rejected);
        assert(again.Submissions[1U] == L::RecordSinkSubmission::Accepted);
        assert(sourceLease.IsEmpty() && !secondOwner.IsEmpty());
        assert(encoder.Calls == 2U);
        secondExecutor.Execute();
        assert(second.TryFinalize().Status ==
            L::AsyncSinkCompletionStatus::PartialAndReleased);

        firstExecutor.Execute();
        const auto delivered = first.TryFinalize();
        assert(delivered.Status == L::AsyncSinkCompletionStatus::AcceptedAndReleased);
        assert(delivered.Delivery == L::RecordSinkSubmission::Accepted);
        assert(firstDelivery.Bytes[1U] == std::byte{0x27U});
        assert(firstOwner.IsEmpty() && secondOwner.IsEmpty());

        // A separately held EDP-Memory slot fails admission without hidden
        // dynamic overflow or touching a Task dispatch.
        Lease blocked;
        assert(firstPool.Acquire(
            blocked,
            P::Synchronization::WaitTimeout::NoWait()
        ) == M::ObjectPoolAcquisitionResult::Succeeded);
        const std::array<std::byte, 1U> one{std::byte{0x11U}};
        assert(first.Submit(L::EdlgCodec::Cbor, one) ==
            L::AsyncSinkAdmissionStatus::PoolCapacityUnavailable);
        assert(blocked.Release() == M::ObjectPoolLeaseReleaseResult::Released);

        assert(sourceRuntime.TearDown() ==
            M::MemoryTopologyTeardownResult::Succeeded);
        assert(firstRuntime.TearDown() ==
            M::MemoryTopologyTeardownResult::Succeeded);
        assert(secondRuntime.TearDown() ==
            M::MemoryTopologyTeardownResult::Succeeded);
    }

}
int main() {
    TestIndependentCopyCompletion();
    TestTwoIndependentAsyncSinks();
}
