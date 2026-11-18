#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include <ESPressio_Persistence.hpp>
#include "EdlgFrameReader.hpp"

namespace ESPressio::Logging {

    /// Status from writing one complete checked EDLG frame to EDP-Persistence.
    enum class EdlgCheckpointWriteStatus : std::uint8_t {
        Committed = 0U,
        InvalidFrame = 1U,
        FrameTooLarge = 2U,
        ProviderRejected = 3U
    };

    /// Both semantic validity and underlying provider commit outcome.
    struct EdlgCheckpointWriteOutcome final {
        /// Semantic checkpoint write result, kept separate from raw provider status.
        EdlgCheckpointWriteStatus Status{EdlgCheckpointWriteStatus::ProviderRejected};
        /// Exact native KeyValueStorage provider result.
        ESPressio::Persistence::KeyValueStoreStatus Store{
            ESPressio::Persistence::KeyValueStoreStatus::NotReady
        };
    };

    /// Exact result of reading a single frame-shaped checkpoint.
    enum class EdlgCheckpointReadStatus : std::uint8_t {
        Recovered = 0U,
        NotFound = 1U,
        BufferTooSmall = 2U,
        CorruptFrame = 3U,
        ProviderFailure = 4U
    };

    /// Unambiguous result: frame proven valid only on Recovered.
    struct EdlgCheckpointReadOutcome final {
        /// Semantic recovery result; only Recovered publishes a borrowed frame view.
        EdlgCheckpointReadStatus Status{EdlgCheckpointReadStatus::ProviderFailure};
        /// Exact native KeyValueStorage provider read result.
        ESPressio::Persistence::KeyValueReadStatus Read{
            ESPressio::Persistence::KeyValueReadStatus::NotReady
        };
        /// Complete, checksum-verified outer frame size when recovered.
        std::size_t FrameBytes{0U};
    };

    /// Store one complete valid EDLG frame through an injected KeyValueStorage.
    /// This replaces the prior value. It is NOT an EDLS historical archive.
    /// Retention and interruption atomicity are exactly those of the selected
    /// EDP-Persistence provider. Bootstrap must serialize provider calls.
    /// @tparam TStorage Application-composed EDP-Persistence KeyValueStorage provider.
    template<class TStorage>
    [[nodiscard]] EdlgCheckpointWriteOutcome WriteEdlgCheckpoint(
        TStorage& storage,
        ESPressio::Persistence::KeyView key,
        std::span<const std::byte> frame,
        std::size_t maximumPayload
    ) noexcept {
        using namespace ESPressio::Persistence;
        if (maximumPayload > std::numeric_limits<std::size_t>::max() - 20U ||
            frame.size() > maximumPayload + 20U) {
            return {EdlgCheckpointWriteStatus::FrameTooLarge, KeyValueStoreStatus::NotReady};
        }
        EdlgFrameView validated{};
        if (ReadEdlgFrame(
            frame,
            validated,
            maximumPayload
        ) !=
                EdlgFrameReadResult::Succeeded ||
            validated.Consumed != frame.size()) {
            return {EdlgCheckpointWriteStatus::InvalidFrame, KeyValueStoreStatus::NotReady};
        }
        const auto native = storage.StoreValue(
            key,
            SourceBufferView{frame.data(), frame.size()}
        );
        return {
            native == KeyValueStoreStatus::Succeeded
                ? EdlgCheckpointWriteStatus::Committed
                : EdlgCheckpointWriteStatus::ProviderRejected,
            native
        };
    }

    /// Recover one exact EDLG frame into fixed caller-owned bytes, publishing
    /// output metadata only after complete CRC and completion-marker proof.
    /// Failure may change destination bytes; no incomplete view is published.
    /// @tparam TStorage Application-composed EDP-Persistence KeyValueStorage provider.
    template<class TStorage>
    [[nodiscard]] EdlgCheckpointReadOutcome ReadEdlgCheckpoint(
        const TStorage& storage,
        ESPressio::Persistence::KeyView key,
        std::span<std::byte> destination,
        std::size_t maximumPayload,
        EdlgFrameView& output
    ) noexcept {
        using namespace ESPressio::Persistence;
        const auto native = storage.ReadValue(
            key,
            DestinationBufferView{destination.data(), destination.size()}
        );
        if (native.Status == KeyValueReadStatus::NotFound) {
            return {EdlgCheckpointReadStatus::NotFound, native.Status, 0U};
        }
        if (native.Status != KeyValueReadStatus::Succeeded) {
            return {EdlgCheckpointReadStatus::ProviderFailure, native.Status, 0U};
        }
        // Without the explicit known-size fact the provider cannot prove that
        // an EDLG candidate is complete. A misreported transfer size must
        // never allow construction of an out-of-bounds borrowed span.
        if (!HasFact(
            native.Facts,
            ReadFact::AvailableDataSizeIsKnown
        ) ||
            native.BytesTransferred > destination.size()) {
            return {EdlgCheckpointReadStatus::ProviderFailure, native.Status, 0U};
        }
        if (native.AvailableDataSize.RawValue > destination.size() ||
            native.BytesTransferred != native.AvailableDataSize.RawValue) {
            return {EdlgCheckpointReadStatus::BufferTooSmall, native.Status, 0U};
        }
        EdlgFrameView verified{};
        if (ReadEdlgFrame(
            destination.first(native.BytesTransferred),
            verified,
            maximumPayload
        ) != EdlgFrameReadResult::Succeeded ||
            verified.Consumed != native.BytesTransferred) {
            return {EdlgCheckpointReadStatus::CorruptFrame, native.Status, 0U};
        }
        output = verified;
        return {EdlgCheckpointReadStatus::Recovered, native.Status, verified.Consumed};
    }

} // namespace ESPressio::Logging
