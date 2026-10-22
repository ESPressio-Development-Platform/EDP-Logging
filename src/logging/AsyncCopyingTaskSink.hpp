#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include <ESPressio_Memory.hpp>
#include <ESPressio_Platform.hpp>
#include <ESPressio_Threading.hpp>

#include "OwnedRepresentationFanout.hpp"

namespace ESPressio::Logging {

    /// The outcome of submitting a separately owned representation to a Threading Task.
    enum class AsyncSinkAdmissionStatus : std::uint8_t {
        Queued = 0,
        Busy = 1,
        CodecMismatch = 2,
        InvalidPayload = 3,
        PoolCapacityUnavailable = 4,
        PoolUnavailable = 5,
        PoolProviderFailure = 6,
        DispatchRejected = 7,
        ReclaimUnconfirmed = 8
    };

    /// Distinguishes native task completion from admission and Persistence durability.
    enum class AsyncSinkCompletionStatus : std::uint8_t {
        NoPendingTask = 0,
        Pending = 1,
        CancelledAndReleased = 2,
        AcceptedAndReleased = 3,
        RejectedAndReleased = 4,
        PartialAndReleased = 5,
        ProviderFailureAndReleased = 6,
        TaskResultUnavailable = 7,
        ReclaimUnconfirmed = 8
    };

    /// Retains source-independent native owner evidence, without inventing a durable ACK.
    struct AsyncSinkCompletionOutcome final {

        /// Terminal Task and native EDP-Memory release interpretation.
        AsyncSinkCompletionStatus Status{AsyncSinkCompletionStatus::NoPendingTask};

        /// The actual sink result, meaningful after a completed Task.
        RecordSinkSubmission Delivery{RecordSinkSubmission::ProviderFailure};

        /// Actual native release status when a release was attempted.
        ESPressio::Memory::ObjectPoolLeaseReleaseResult Release{
            ESPressio::Memory::ObjectPoolLeaseReleaseResult::AlreadyEmpty
        };

    };

    /// One in-flight, copying, Task-backed sink, using exclusively owner-provided EDP
    /// facilities. A separate Bootstrap-owned instance is required per async sink.
    ///
    /// The Task and Memory lease are externally owned and MUST have stable lifetimes
    /// beyond both this adapter and the final Task termination. The caller must
    /// serialize Submit/Finalize against itself (for example with an already-composed
    /// EDP-Threading OrdinaryMutex); neither a second queue nor a mutex is created.
    ///
    /// Dispatch of a callable referencing this adapter's independently pooled bytes
    /// means Accepted only if EDP-Threading admitted the Task. It does NOT mean
    /// the native sink has completed, flushed or durably stored the representation.
    /// Sinks MUST synchronously consume the worker's borrowed span. No original
    /// LogRecord or initial representation span may survive submission.
    /// One immutable Task callable bound by Bootstrap to a configured
    /// EDP-Threading Task facility or dedicated Worker lease. It retains no
    /// source borrow: Payload points into the independently leased Memory pool.
    template<EdlgCodec TCodec, std::size_t TMaximumPayloadBytes, class TDeliverySink>
    struct AsyncOwnedDeliveryWork final {

        /// Stable native Memory object, retained until Task is terminal.
        const OwnedEncodedPayload<TMaximumPayloadBytes>* Payload;

        /// Bootstrap-supplied sink, whose lifetime outlives Task completion.
        TDeliverySink* Sink;

        /// Executes one synchronous delivery while the independent lease is live.
        RecordSinkSubmission operator()() const noexcept {
            const auto bytes = std::span<const std::byte>(
                Payload->Bytes.data(),
                Payload->Used
            );
            return (*Sink)(TCodec, bytes);
        }

    };

    template<
        EdlgCodec TCodec,
        std::size_t TMaximumPayloadBytes,
        class TPool,
        class TByteOperations,
        class TExecutor,
        class TDeliverySink
    >
    class AsyncCopyingTaskSink final {

        static_assert(TMaximumPayloadBytes > 0U);
        static_assert(TCodec == EdlgCodec::Cbor || TCodec == EdlgCodec::Json);
        static_assert(std::is_same_v<
            typename TPool::ObjectType,
            OwnedEncodedPayload<TMaximumPayloadBytes>
        >);
        static_assert(
            TByteOperations::CompositionOffers::template Contains<
                ESPressio::Memory::ByteOperations
            >,
            "The byte-copy owner must offer EDP-Memory ByteOperations"
        );
        using Lease = typename TPool::LeaseType;
        using Task = ESPressio::Threading::Task<RecordSinkSubmission>;

        // All services and runtime owners are application/Bootstrap supplied.
        TPool* _pool;
        Lease* _lease;
        std::optional<Task>* _task;
        const TByteOperations* _byteOperations;
        TExecutor* _executor;
        TDeliverySink* _sink;

        /// Exactly the Task Type configured in the application's Threading topology.
        using DeliveryWork = AsyncOwnedDeliveryWork<
            TCodec, TMaximumPayloadBytes, TDeliverySink
        >;

    public:

        /// Captures already-initialized native owners; never transfers their lifetime.
        AsyncCopyingTaskSink(
            TPool& pool,
            Lease& lease,
            std::optional<Task>& task,
            const TByteOperations& bytes,
            TExecutor& executor,
            TDeliverySink& sink
        ) noexcept :
            _pool(&pool),
            _lease(&lease),
            _task(&task),
            _byteOperations(&bytes),
            _executor(&executor),
            _sink(&sink) {
            static_assert(
                noexcept(std::declval<TDeliverySink&>()(
                    TCodec,
                    std::declval<std::span<const std::byte>>()
                ))
            );
            static_assert(
                std::is_same_v<
                    decltype(std::declval<TDeliverySink&>()(
                        TCodec,
                        std::declval<std::span<const std::byte>>()
                    )),
                    RecordSinkSubmission
                >
            );
            static_assert(
                std::is_same_v<
                    decltype(std::declval<TExecutor&>().Dispatch(
                        std::declval<DeliveryWork>(),
                        ESPressio::Threading::TaskDispatchPolicy::AbandonImmediately
                    )),
                    ESPressio::Threading::TaskDispatchResult<Task>
                >,
                "EDP-Threading dispatcher must return Task<RecordSinkSubmission>"
            );
            static_assert(
                noexcept(std::declval<const TByteOperations&>().CopyBytes(
                    std::declval<void*>(),
                    std::declval<const void*>(),
                    std::declval<std::size_t>()
                ))
            );
        }

        AsyncCopyingTaskSink(const AsyncCopyingTaskSink&) = delete;
        AsyncCopyingTaskSink& operator=(const AsyncCopyingTaskSink&) = delete;
        AsyncCopyingTaskSink(AsyncCopyingTaskSink&&) = delete;
        AsyncCopyingTaskSink& operator=(AsyncCopyingTaskSink&&) = delete;

        /// Synchronously copies into a real EDP-Memory lease before asynchronous
        /// EDP-Threading admission; never allows the incoming borrow to escape.
        AsyncSinkAdmissionStatus Submit(
            EdlgCodec codec,
            std::span<const std::byte> source
        ) {
            if (codec != TCodec) {
                return AsyncSinkAdmissionStatus::CodecMismatch;
            }
            if (_task->has_value() || !_lease->IsEmpty()) {
                return AsyncSinkAdmissionStatus::Busy;
            }
            if (source.empty() || source.size() > TMaximumPayloadBytes) {
                return AsyncSinkAdmissionStatus::InvalidPayload;
            }
            const auto acquired = _pool->Acquire(
                *_lease,
                ESPressio::Platform::Synchronization::WaitTimeout::NoWait()
            );
            if (acquired != ESPressio::Memory::ObjectPoolAcquisitionResult::Succeeded) {
                if (acquired == ESPressio::Memory::ObjectPoolAcquisitionResult::CapacityUnavailable ||
                    acquired == ESPressio::Memory::ObjectPoolAcquisitionResult::TimedOut) {
                    return AsyncSinkAdmissionStatus::PoolCapacityUnavailable;
                }
                if (acquired == ESPressio::Memory::ObjectPoolAcquisitionResult::NotInitialized ||
                    acquired == ESPressio::Memory::ObjectPoolAcquisitionResult::TopologyUnavailable) {
                    return AsyncSinkAdmissionStatus::PoolUnavailable;
                }
                return AsyncSinkAdmissionStatus::PoolProviderFailure;
            }

            auto* payload = _lease->Get();
            if (payload == nullptr) {
                // Ownership is uncertain: do not destroy the retained Lease.
                return AsyncSinkAdmissionStatus::PoolProviderFailure;
            }
            _byteOperations->CopyBytes(
                payload->Bytes.data(),
                source.data(),
                source.size()
            );
            payload->Used = source.size();

            auto dispatched = _executor->Dispatch(
                DeliveryWork{payload, _sink},
                ESPressio::Threading::TaskDispatchPolicy::AbandonImmediately
            );
            if (!dispatched.IsSucceeded()) {
                const auto released = _lease->Release();
                return released == ESPressio::Memory::ObjectPoolLeaseReleaseResult::Released
                    ? AsyncSinkAdmissionStatus::DispatchRejected
                    : AsyncSinkAdmissionStatus::ReclaimUnconfirmed;
            }
            _task->emplace(dispatched.TakeTask());
            return AsyncSinkAdmissionStatus::Queued;
        }

        /// Presents EDP-Logging's pre-existing RecordSinkSubmission ABI.
        /// Accepted is admission into Threading only, not final native delivery.
        RecordSinkSubmission operator()(
            EdlgCodec codec,
            std::span<const std::byte> source
        ) noexcept {
            const auto result = Submit(codec, source);
            if (result == AsyncSinkAdmissionStatus::Queued) {
                return RecordSinkSubmission::Accepted;
            }
            if (result == AsyncSinkAdmissionStatus::Busy ||
                result == AsyncSinkAdmissionStatus::CodecMismatch ||
                result == AsyncSinkAdmissionStatus::InvalidPayload ||
                result == AsyncSinkAdmissionStatus::PoolCapacityUnavailable ||
                result == AsyncSinkAdmissionStatus::DispatchRejected) {
                return RecordSinkSubmission::Rejected;
            }
            return RecordSinkSubmission::ProviderFailure;
        }

        /// Observes terminal Task truth without waiting/polling and only then
        /// reclaims the independent native lease; called by Bootstrap after the
        /// chosen Task WaitUntil/notification/stop protocol.
        AsyncSinkCompletionOutcome TryFinalize() {
            AsyncSinkCompletionOutcome result{};
            if (!_task->has_value()) {
                result.Status = _lease->IsEmpty()
                    ? AsyncSinkCompletionStatus::NoPendingTask
                    : AsyncSinkCompletionStatus::ReclaimUnconfirmed;
                return result;
            }
            if (!(*_task)->IsFinished()) {
                result.Status = AsyncSinkCompletionStatus::Pending;
                return result;
            }

            if ((*_task)->IsCancelled()) {
                _task->reset();
                result.Status = AsyncSinkCompletionStatus::CancelledAndReleased;
            } else {
                auto taken = (*_task)->TakeResult();
                if (!taken.IsSucceeded()) {
                    result.Status = AsyncSinkCompletionStatus::TaskResultUnavailable;
                    return result;
                }
                result.Delivery = taken.TakeResult();
                _task->reset();
                switch (result.Delivery) {
                    case RecordSinkSubmission::Accepted:
                        result.Status = AsyncSinkCompletionStatus::AcceptedAndReleased;
                        break;
                    case RecordSinkSubmission::Rejected:
                        result.Status = AsyncSinkCompletionStatus::RejectedAndReleased;
                        break;
                    case RecordSinkSubmission::Partial:
                        result.Status = AsyncSinkCompletionStatus::PartialAndReleased;
                        break;
                    case RecordSinkSubmission::ProviderFailure:
                        result.Status = AsyncSinkCompletionStatus::ProviderFailureAndReleased;
                        break;
                }
            }
            result.Release = _lease->Release();
            if (result.Release != ESPressio::Memory::ObjectPoolLeaseReleaseResult::Released) {
                result.Status = AsyncSinkCompletionStatus::ReclaimUnconfirmed;
            }
            return result;
        }

        /// Requests cancellation through the sole EDP-Threading Task handle.
        /// A request is not terminal; the caller must still observe TryFinalize.
        ESPressio::Threading::TaskCancelResult RequestCancellation() noexcept {
            return _task->has_value()
                ? (*_task)->Cancel()
                : ESPressio::Threading::TaskCancelResult::AlreadyFinished;
        }

        /// After a terminal Task with failed native release, make one explicit
        /// caller-owned recovery attempt. Never reclaim while a Task is live.
        ESPressio::Memory::ObjectPoolLeaseReleaseResult RetryReclaim() noexcept {
            if (_task->has_value()) {
                return ESPressio::Memory::ObjectPoolLeaseReleaseResult::ProviderFailure;
            }
            return _lease->Release();
        }

    };

} // ESPressio::Logging
