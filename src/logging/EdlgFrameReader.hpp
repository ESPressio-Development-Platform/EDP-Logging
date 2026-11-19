#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "EdlgFrame.hpp"

namespace ESPressio::Logging {

    /// Strict, non-resynchronising read-only inspection of one EDLG v1 frame.
    enum class EdlgFrameReadResult : std::uint8_t {
        /// Header, payload, completion marker and CRC all validate.
        Succeeded = 0,
        /// The source ends before the required header or full frame.
        IncompleteFrame = 1,
        /// The source does not start with the EDLG magic bytes.
        InvalidMagic = 2,
        /// The frame version is outside the implemented v1 contract.
        UnsupportedVersion = 3,
        /// The wire codec is neither CBOR nor JSON.
        InvalidCodec = 4,
        /// Reserved frame flags contain unsupported nonzero bits.
        InvalidFlags = 5,
        /// The declared payload exceeds the caller bound or addressable frame size.
        PayloadTooLarge = 6,
        /// The complete frame lacks the required GLDE trailer.
        InvalidCompletion = 7,
        /// The stored CRC32C differs from the complete protected bytes.
        ChecksumMismatch = 8
    };

    /// Borrowed verified frame; source must outlive all consumers of the view.
    struct EdlgFrameView final {

        // Verified framing metadata and borrowed payload.

        /// Wire codec declared in the verified frame header.
        EdlgCodec Codec{EdlgCodec::Cbor};

        /// Encoded bytes borrowed from the input buffer supplied to ReadEdlgFrame.
        std::span<const std::byte> Payload{};

        /// Bytes occupied by the verified header, payload and completion marker.
        std::size_t Consumed = 0U;

        /// Stored CRC32C after comparison with the recomputed frame checksum.
        std::uint32_t Checksum = 0U;

    };

    /// Inspect one complete frame at offset zero in caller-owned bytes.
    ///
    /// A valid frame consumes exactly header+payload+completion, permitting a
    /// caller to iterate a contiguous prefix. Extra source bytes are untouched.
    /// No attempt is made to salvage records after corruption; archive rotation
    /// and forensic resync are separate explicit responsibilities.
    ///
    /// Until this returns Succeeded, output is unchanged. After it succeeds,
    /// output.Payload borrows storage from source.
    ///
    /// @param source Input bytes starting at the candidate frame header.
    /// @param output View assigned after all framing checks pass.
    /// @param maximumPayloadBytes Largest encoded payload accepted by the caller.
    /// @return Succeeded or the first failed framing check.
    [[nodiscard]] inline EdlgFrameReadResult ReadEdlgFrame(
        std::span<const std::byte> source,
        EdlgFrameView& output,
        std::size_t maximumPayloadBytes
    ) noexcept {
        constexpr std::size_t headerBytes = 16U;
        constexpr std::size_t trailerBytes = 4U;
        constexpr std::size_t overheadBytes = headerBytes + trailerBytes;

        if (source.size() < headerBytes) {
            return EdlgFrameReadResult::IncompleteFrame;
        }
        if (
            source[0U] != std::byte{'E'} ||
            source[1U] != std::byte{'D'} ||
            source[2U] != std::byte{'L'} ||
            source[3U] != std::byte{'G'}
        ) {
            return EdlgFrameReadResult::InvalidMagic;
        }
        if (source[4U] != std::byte{1U}) {
            return EdlgFrameReadResult::UnsupportedVersion;
        }

        EdlgCodec codec{};
        switch (std::to_integer<std::uint8_t>(source[5U])) {
            case 1U:
                codec = EdlgCodec::Cbor;
                break;
            case 2U:
                codec = EdlgCodec::Json;
                break;
            default:
                return EdlgFrameReadResult::InvalidCodec;
        }
        if (source[6U] != std::byte{0U} || source[7U] != std::byte{0U}) {
            return EdlgFrameReadResult::InvalidFlags;
        }

        const auto payloadLength =
            static_cast<std::uint32_t>(
                std::to_integer<std::uint8_t>(source[8U]) |
                (std::uint32_t(std::to_integer<std::uint8_t>(source[9U])) << 8U) |
                (std::uint32_t(std::to_integer<std::uint8_t>(source[10U])) << 16U) |
                (std::uint32_t(std::to_integer<std::uint8_t>(source[11U])) << 24U)
            );

        if (
            payloadLength > maximumPayloadBytes ||
            static_cast<std::uint64_t>(payloadLength) >
                static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() - overheadBytes)
        ) {
            return EdlgFrameReadResult::PayloadTooLarge;
        }
        const auto completeLength = overheadBytes + static_cast<std::size_t>(payloadLength);
        if (source.size() < completeLength) {
            return EdlgFrameReadResult::IncompleteFrame;
        }
        const auto tailOffset = headerBytes + static_cast<std::size_t>(payloadLength);
        if (
            source[tailOffset] != std::byte{'G'} ||
            source[tailOffset + 1U] != std::byte{'L'} ||
            source[tailOffset + 2U] != std::byte{'D'} ||
            source[tailOffset + 3U] != std::byte{'E'}
        ) {
            return EdlgFrameReadResult::InvalidCompletion;
        }

        const auto payload = source.subspan(
            headerBytes,
            payloadLength
        );
        const auto one = Detail::EdlgFrameCrc32C(source.first(12U));
        const auto two = Detail::EdlgFrameCrc32C(
            payload,
            one
        );
        const auto expected = Detail::EdlgFrameCrc32C(
            source.subspan(
                tailOffset,
                trailerBytes
            ),
            two
        );
        const std::uint32_t persisted =
            static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(source[12U])) |
            (std::uint32_t(std::to_integer<std::uint8_t>(source[13U])) << 8U) |
            (std::uint32_t(std::to_integer<std::uint8_t>(source[14U])) << 16U) |
            (std::uint32_t(std::to_integer<std::uint8_t>(source[15U])) << 24U);
        if (expected != persisted) {
            return EdlgFrameReadResult::ChecksumMismatch;
        }

        output = EdlgFrameView{codec, payload, completeLength, persisted};
        return EdlgFrameReadResult::Succeeded;
    }

} // ESPressio::Logging
