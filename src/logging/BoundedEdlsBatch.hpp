#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include <bounded/Bytes.hpp>
#include <clock/ClockComposition.hpp>
#include <clock/Duration.hpp>
#include <clock/MonotonicTimestamp.hpp>

#include "EdlsPowerLossAcknowledgement.hpp"

namespace ESPressio::Logging {

    /// Result of changing the empty batch's Bootstrap-selected dwell.
    enum class EdlsBatchConfigurationStatus : std::uint8_t {
        Configured = 0U,
        InvalidDwell = 1U,
        PendingBatch = 2U,
        RecoveryRequired = 3U,
        InsufficientScratch = 4U
    };

    /// Admission, service and durable completion outcomes for one batch.
    enum class EdlsBatchStatus : std::uint8_t {
        Buffered = 0U,
        FlushRequired = 1U,
        Waiting = 2U,
        Empty = 3U,
        NotConfigured = 4U,
        InvalidFrame = 5U,
        FrameTooLarge = 6U,
        ClockRangeExhausted = 7U,
        ClockRegressed = 8U,
        DurablyAcknowledged = 9U,
        DurablyAcknowledgedPendingRetirement = 10U,
        RecoveryRequired = 11U
    };

    /// Separate volatile admission and durable segment-completion evidence.
    struct EdlsBatchOutcome final {

        /// Exact operation result; Buffered means RAM admission only.
        EdlsBatchStatus Status{EdlsBatchStatus::Empty};

        /// Number of frames still owned by the volatile batch after this call.
        std::size_t PendingRecords{0U};

        /// Records ACKed by THIS call; zero for admission, waiting or failure.
        std::size_t AcknowledgedRecords{0U};

        /// Existing archive result; meaningful when Flush invoked the owner.
        EdlsAcknowledgedObservation Archive{};

    };

    /// Finite single-writer accumulator over the existing strict EDLS owner.
    ///
    /// Bootstrap owns this object, its archive, canonical Clock and a separate
    /// non-aliased complete-segment scratch span. Bytes owns the inline finite
    /// accumulator and uses its existing Memory ByteOperations provider.
    /// Caller serialization covers Offer, Service, Flush and configuration.
    /// The existing application Threading worker waits until NextFlushDeadline,
    /// or wakes for new work; this object creates no timer, thread or queue.
    ///
    /// N retained archive generations mean N SEGMENTS, each containing at most
    /// TMaximumRecords records. A returned completion ACKs the whole segment.
    /// A failed/uncertain archive operation latches RecoveryRequired and retains
    /// bytes without retry: Bootstrap must reconcile the archive before
    /// replacing this accumulator. Destruction never commits or ACKs pending RAM.
    ///
    /// @tparam TMaximumRecords Positive compile-time bound, at most 64 records.
    /// @tparam TMaximumBytes Compile-time framed-byte capacity, excluding EDLS header.
    /// @tparam TArchive Existing PowerLossAcknowledgedEdls owner type.
    /// @tparam TMonotonicClockProvider Bootstrap-selected canonical MonotonicClock.
    /// @tparam TByteOperationsProvider Stateless Bytes-compatible Memory byte provider.
    /// @tparam TMaximumDwellNanoseconds Compile-time upper bound for Bootstrap dwell.
    template<
        std::size_t TMaximumRecords,
        std::size_t TMaximumBytes,
        class TArchive,
        class TMonotonicClockProvider,
        class TByteOperationsProvider,
        std::int64_t TMaximumDwellNanoseconds = 100000000
    >
    class BoundedEdlsBatch final {
    private:

        // Provider and resource contracts.

        static_assert(TMaximumRecords >= 1U && TMaximumRecords <= 64U);
        static_assert(TMaximumBytes >= 20U);
        static_assert(TMaximumDwellNanoseconds >= 0);
        static_assert(
            TMaximumBytes + EdlsHeaderBytes +
                (TMaximumRecords > 1U ? EdlsSealBytes : 0U)
                <= TArchive::MaximumSegmentBytes()
        );
        static_assert(
            TMonotonicClockProvider::CompositionOffers::template Contains<
                ESPressio::Clock::MonotonicClock
            >
        );
        static_assert(
            std::is_same_v<
                decltype(std::declval<const TMonotonicClockProvider&>().Now()),
                ESPressio::Clock::MonotonicTimestamp
            >
        );
        static_assert(
            noexcept(std::declval<const TMonotonicClockProvider&>().Now())
        );

        // Application-owned stable providers and buffers.

        /// Sole archive transaction owner; it outlives this accumulator.
        TArchive* archive_;

        /// Canonical application Clock; no private timebase is introduced.
        const TMonotonicClockProvider* clock_;

        /// Stable caller scratch, disjoint from accumulator and source frames.
        std::span<std::byte> scratch_;

        /// Existing bounded Bytes owns complete framed records, not source borrows.
        ESPressio::Bounded::Bytes<TMaximumBytes, TByteOperationsProvider> bytes_{};

        // Batch lifecycle.

        /// Exact admitted complete-frame count, bounded by TMaximumRecords.
        std::size_t records_{0U};

        /// First record's canonical admission time; valid only for a pending batch.
        ESPressio::Clock::MonotonicTimestamp first_{};

        /// Bootstrap dwell, immutable while a batch is pending.
        ESPressio::Clock::Duration dwell_{};

        /// Whether Bootstrap supplied a valid dwell and sufficient scratch.
        bool configured_{false};

        /// Sticky uncertainty: no automatic retry or fresh admission is permitted.
        bool recoveryRequired_{false};

        // Result construction.

        /// Reports non-durable progress without inventing a segment ACK.
        [[nodiscard]] EdlsBatchOutcome Observe(EdlsBatchStatus status) const noexcept {
            EdlsBatchOutcome result{};
            result.Status = status;
            result.PendingRecords = records_;
            return result;
        }

        /// Indicates that a pending batch should be flushed at this coordinate.
        [[nodiscard]] bool IsDue(
            ESPressio::Clock::MonotonicTimestamp now
        ) const noexcept {
            return records_ == TMaximumRecords ||
                bytes_.Size() == TMaximumBytes ||
                now.Nanoseconds() - first_.Nanoseconds() >=
                    static_cast<std::uint64_t>(dwell_.Nanoseconds());
        }

    public:

        // Construction and Bootstrap configuration.

        /// Borrows stable providers and whole-segment scratch; call ConfigureDwell.
        BoundedEdlsBatch(
            TArchive& archive,
            const TMonotonicClockProvider& clock,
            std::span<std::byte> scratch
        ) noexcept :
            archive_(&archive),
            clock_(&clock),
            scratch_(scratch) {}

        /// This semantic batch owner must not be copied or relocated.
        BoundedEdlsBatch(const BoundedEdlsBatch&) = delete;

        /// Copy assignment cannot transfer pending acknowledgement ownership.
        BoundedEdlsBatch& operator=(const BoundedEdlsBatch&) = delete;

        /// Existing worker references require a lifetime-stable address.
        BoundedEdlsBatch(BoundedEdlsBatch&&) = delete;

        /// Move assignment cannot relocate pending frame ownership.
        BoundedEdlsBatch& operator=(BoundedEdlsBatch&&) = delete;

        /// Select zero/immediate or finite dwell within the compile-time maximum.
        /// A pending batch's deadline cannot be extended by reconfiguration.
        [[nodiscard]] EdlsBatchConfigurationStatus ConfigureDwell(
            ESPressio::Clock::Duration dwell
        ) noexcept {
            if (recoveryRequired_) {
                return EdlsBatchConfigurationStatus::RecoveryRequired;
            }
            if (records_ != 0U) {
                return EdlsBatchConfigurationStatus::PendingBatch;
            }
            if (dwell.Nanoseconds() < 0 ||
                dwell.Nanoseconds() > TMaximumDwellNanoseconds) {
                return EdlsBatchConfigurationStatus::InvalidDwell;
            }
            if (scratch_.size() < EdlsHeaderBytes + TMaximumBytes +
                    (TMaximumRecords > 1U ? EdlsSealBytes : 0U) ||
                scratch_.size() <
                    EdlsHeaderBytes + TArchive::MaximumFramePayloadBytes() + 20U) {
                return EdlsBatchConfigurationStatus::InsufficientScratch;
            }
            dwell_ = dwell;
            configured_ = true;
            return EdlsBatchConfigurationStatus::Configured;
        }

        // Producer admission and worker scheduling.

        /// Copies ONE complete validated CBOR EDLG record into finite owned Bytes.
        /// Buffered never means durable. FlushRequired does NOT consume this input.
        /// No provider I/O is performed until Service or Flush calls the archive.
        [[nodiscard]] EdlsBatchOutcome Offer(
            std::span<const std::byte> completeFrame
        ) noexcept {
            if (recoveryRequired_) {
                return Observe(EdlsBatchStatus::RecoveryRequired);
            }
            if (!configured_) {
                return Observe(EdlsBatchStatus::NotConfigured);
            }
            EdlgFrameView frame{};
            if (ReadEdlgFrame(
                    completeFrame,
                    frame,
                    TArchive::MaximumFramePayloadBytes()
                ) != EdlgFrameReadResult::Succeeded ||
                frame.Consumed != completeFrame.size() ||
                frame.Codec != EdlgCodec::Cbor) {
                return Observe(EdlsBatchStatus::InvalidFrame);
            }
            if (completeFrame.size() > TMaximumBytes) {
                return Observe(EdlsBatchStatus::FrameTooLarge);
            }
            const auto now = clock_->Now();
            if (records_ != 0U && now < first_) {
                recoveryRequired_ = true;
                return Observe(EdlsBatchStatus::ClockRegressed);
            }
            if (records_ != 0U &&
                (IsDue(now) || completeFrame.size() > TMaximumBytes - bytes_.Size())) {
                return Observe(EdlsBatchStatus::FlushRequired);
            }
            if (records_ == 0U &&
                now.Nanoseconds() >
                    std::numeric_limits<std::uint64_t>::max() -
                    static_cast<std::uint64_t>(dwell_.Nanoseconds())) {
                return Observe(EdlsBatchStatus::ClockRangeExhausted);
            }
            if (bytes_.Append(
                    reinterpret_cast<const std::uint8_t*>(completeFrame.data()),
                    completeFrame.size()
                ) != ESPressio::Bounded::BytesAppendResult::Succeeded) {
                return Observe(EdlsBatchStatus::FrameTooLarge);
            }
            if (records_ == 0U) {
                first_ = now;
            }
            ++records_;
            return Observe(EdlsBatchStatus::Buffered);
        }

        /// Deadline for the existing worker's canonical WaitUntil, or no pending work.
        /// A full batch returns its already-reached first-record coordinate.
        [[nodiscard]] std::optional<ESPressio::Clock::MonotonicTimestamp>
        NextFlushDeadline() const noexcept {
            if (records_ == 0U || recoveryRequired_) {
                return std::nullopt;
            }
            if (records_ == TMaximumRecords || bytes_.Size() == TMaximumBytes) {
                return first_;
            }
            return ESPressio::Clock::MonotonicTimestamp::FromNanoseconds(
                first_.Nanoseconds() + static_cast<std::uint64_t>(dwell_.Nanoseconds())
            );
        }

        /// One bounded worker quantum: commit only when full or dwell has expired.
        [[nodiscard]] EdlsBatchOutcome Service() noexcept {
            if (recoveryRequired_) {
                return Observe(EdlsBatchStatus::RecoveryRequired);
            }
            if (!configured_) {
                return Observe(EdlsBatchStatus::NotConfigured);
            }
            if (records_ == 0U) {
                return Observe(EdlsBatchStatus::Empty);
            }
            const auto now = clock_->Now();
            if (now < first_) {
                recoveryRequired_ = true;
                return Observe(EdlsBatchStatus::ClockRegressed);
            }
            return IsDue(now) ? Flush() : Observe(EdlsBatchStatus::Waiting);
        }

        /// Explicit partial-batch drain (for shutdown or an incoming oversized fit).
        /// Exactly one existing archive transaction; unknown ACK never auto-retries.
        [[nodiscard]] EdlsBatchOutcome Flush() noexcept {
            if (recoveryRequired_) {
                return Observe(EdlsBatchStatus::RecoveryRequired);
            }
            if (!configured_) {
                return Observe(EdlsBatchStatus::NotConfigured);
            }
            if (records_ == 0U) {
                return Observe(EdlsBatchStatus::Empty);
            }
            const auto committed = archive_->template PublishBatch<TMaximumRecords>(
                std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(bytes_.Data()),
                    bytes_.Size()
                ),
                scratch_
            );
            const bool acknowledged =
                committed.Status == EdlsAcknowledgedStatus::AcknowledgedWithinCapacity ||
                committed.Status == EdlsAcknowledgedStatus::AcknowledgedAndRetired ||
                committed.Status == EdlsAcknowledgedStatus::AcknowledgedPendingRetirement;
            if (!acknowledged) {
                recoveryRequired_ = true;
                auto result = Observe(EdlsBatchStatus::RecoveryRequired);
                result.Archive = committed;
                return result;
            }
            EdlsBatchOutcome result{};
            result.Status =
                committed.Status == EdlsAcknowledgedStatus::AcknowledgedPendingRetirement
                    ? EdlsBatchStatus::DurablyAcknowledgedPendingRetirement
                    : EdlsBatchStatus::DurablyAcknowledged;
            result.AcknowledgedRecords = records_;
            result.Archive = committed;
            records_ = 0U;
            bytes_.Clear();
            return result;
        }

    };

} // ESPressio::Logging
