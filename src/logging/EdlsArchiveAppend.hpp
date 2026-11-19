#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include <ESPressio_Persistence.hpp>

#include "EdlgFrameReader.hpp"
#include "EdlsArchiveInspection.hpp"

namespace ESPressio::Logging {

    /// One deliberately non-rotating, checked EDLS existing-slot append.
    enum class EdlsAppendStatus : std::uint8_t {
        /// The native FileStorage owner accepted the checked append.
        ProviderAccepted = 0U,
        /// The offered bytes do not contain exactly one valid EDLG frame.
        InvalidFrame = 1U,
        /// The existing slot failed the complete clean-segment preflight.
        SegmentUnclean = 2U,
        /// The append would exceed the immutable segment byte bound.
        SegmentCapacityExceeded = 3U,
        /// The observed slot size changed before native append.
        ChangedDuringPreflight = 4U,
        /// The native FileStorage append was not confirmed.
        ProviderRejected = 5U
    };

    /// Separate strict preflight outcome and actual EDP-Persistence owner status.
    struct EdlsAppendOutcome final {
        /// Precise semantic result of one mutation attempt.
        EdlsAppendStatus Status{EdlsAppendStatus::ProviderRejected};

        /// Native EDP-Persistence append status, meaningful only if attempted.
        ESPressio::Persistence::FileAppendStatus Native{
            ESPressio::Persistence::FileAppendStatus::NotReady
        };

        /// Number of bytes offered to native append (zero if never attempted).
        std::size_t OfferedBytes{0U};
    };

    /// Append one valid EDLG frame ONLY after a clean complete EDLS slot scan.
    /// This is a non-rotating, caller-serialized primitive, not a durable
    /// archive lifecycle. Bootstrap retains the exclusive FileStorage
    /// mutation right for the ENTIRE scan+append operation, including media
    /// concurrent-writer exclusion. Without that, the GetSize recheck is not
    /// sufficient to establish atomicity.
    ///
    /// A successful AppendFile means EDP-Persistence accepted the mutation
    /// under its actual declared retention/atomicity profile; it DOES NOT
    /// assert power-cut durability, immutable history or physical flush.
    ///
    /// @tparam TMaximumPayloadBytes Bounded scratch and accepted EDLG payload.
    /// @tparam TMaximumSlotBytes Hard ceiling for this physical EDLS file.
    /// @tparam TStorage App-selected EDP-Persistence abstract FileStorage provider.
    template<
        std::size_t TMaximumPayloadBytes,
        std::size_t TMaximumSlotBytes,
        class TStorage
    >
    [[nodiscard]] EdlsAppendOutcome AppendEdlgToCleanEdlsSlot(
        TStorage& storage,
        ESPressio::Persistence::FilePathView path,
        std::uint16_t expectedSlot,
        std::span<const std::byte> framedBytes
    ) noexcept {
        using namespace ESPressio::Persistence;
        static_assert(TMaximumPayloadBytes > 0U && TMaximumPayloadBytes <= 4096U);
        static_assert(TMaximumSlotBytes >= EdlsHeaderBytes + 20U);
        static_assert(ESPressio::Persistence::Detail::HasAppendOperation<TStorage>);
        if (framedBytes.size() > TMaximumPayloadBytes + 20U) {
            return {EdlsAppendStatus::InvalidFrame};
        }
        EdlgFrameView frame{};
        if (ReadEdlgFrame(
            framedBytes,
            frame,
            TMaximumPayloadBytes
        ) !=
                EdlgFrameReadResult::Succeeded ||
            frame.Consumed != framedBytes.size()) {
            return {EdlsAppendStatus::InvalidFrame};
        }
        const auto inspection = InspectEdlsSlot<TMaximumPayloadBytes>(
            storage,
            path,
            expectedSlot
        );
        if (inspection.Condition != EdlsSlotCondition::Clean) {
            return {EdlsAppendStatus::SegmentUnclean};
        }
        const auto extent = inspection.VerifiedPrefixBytes;
        if (extent > TMaximumSlotBytes ||
            framedBytes.size() > TMaximumSlotBytes - extent) {
            return {EdlsAppendStatus::SegmentCapacityExceeded};
        }
        // Check against an independently reported size before mutation,
        // while Bootstrap still holds the exclusive provider writer lock.
        const auto size = storage.GetFileSize(path);
        if (size.Status != FileSizeStatus::Succeeded ||
            size.Size.RawValue != extent) {
            return {EdlsAppendStatus::ChangedDuringPreflight};
        }
        const auto native = storage.AppendFile(
            path,
            SourceBufferView{framedBytes.data(), framedBytes.size()}
        );
        return {
            native == FileAppendStatus::Succeeded
                ? EdlsAppendStatus::ProviderAccepted
                : EdlsAppendStatus::ProviderRejected,
            native,
            framedBytes.size()
        };
    }

} // namespace ESPressio::Logging
