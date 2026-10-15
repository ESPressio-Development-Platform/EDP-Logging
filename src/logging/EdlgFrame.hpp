#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace ESPressio::Logging {

    /// Portable, independently framed encoded LogRecord payload format.
    enum class EdlgCodec : std::uint8_t {
        Cbor = 1,
        Json = 2
    };

    /// Result of constructing one complete framed record into caller-owned storage.
    enum class EdlgFrameWriteResult : std::uint8_t {
        Succeeded = 0,
        InvalidCodec = 1,
        InvalidPayload = 2,
        InsufficientCapacity = 3
    };

    namespace Detail {

        /// CRC32C (reflected Castagnoli) over a sequence of already encoded bytes.
        ///
        /// The initial parameter is the finalized result for the previous block, which
        /// permits header/payload/trailer accumulation without allocating a buffer.
        /// This algorithm belongs only to Logging's EDLG framing until a common
        /// integrity provider is actually approved; no native object representation.
        [[nodiscard]] inline std::uint32_t EdlgFrameCrc32C(
            std::span<const std::byte> bytes,
            std::uint32_t previous = 0U
        ) noexcept {
            std::uint32_t crc = previous ^ 0xFFFFFFFFU;

            for (const std::byte value : bytes) {
                crc ^= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(value));

                for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
                    crc = (crc & 1U) != 0U
                        ? (crc >> 1U) ^ 0x82F63B78U
                        : crc >> 1U;
                }
            }

            return crc ^ 0xFFFFFFFFU;
        }

        /// Emits one 32-bit unsigned value in the fixed portable little-endian wire order.
        inline void WriteEdlgLe32(
            std::span<std::byte> output,
            std::size_t offset,
            std::uint32_t value
        ) noexcept {
            for (std::size_t index = 0U; index < 4U; ++index) {
                output[offset + index] = std::byte{
                    static_cast<std::uint8_t>(value & 0xFFU)
                };
                value >>= 8U;
            }
        }

    } // ESPressio::Logging::Detail

    /// Writes one canonical EDLG v1 record envelope without partial publication.
    ///
    /// Encoded payload is already validated by its owning EDP-Serialisation writer.
    /// Source and destination must not overlap, because the frame is emitted before
    /// source bytes are copied and its CRC is calculated over the source first.
    /// A null pointer with a non-zero length is invalid. No heap, worker or timer.
    ///
    /// @param codec Canonical CBOR or JSON payload representation.
    /// @param payload Borrowed encoded payload bytes, never a C++ object dump.
    /// @param output Mutable caller-owned exact frame storage.
    /// @param written Set only when the complete frame is emitted successfully.
    /// @param maximumPayloadBytes Bootstrap-specified profile bound.
    [[nodiscard]] inline EdlgFrameWriteResult WriteEdlgFrame(
        EdlgCodec codec,
        std::span<const std::byte> payload,
        std::span<std::byte> output,
        std::size_t& written,
        std::size_t maximumPayloadBytes
    ) noexcept {
        if (codec != EdlgCodec::Cbor && codec != EdlgCodec::Json) {
            return EdlgFrameWriteResult::InvalidCodec;
        }

        constexpr std::size_t headerBytes = 16U;
        constexpr std::size_t trailerBytes = 4U;
        constexpr std::size_t overheadBytes = headerBytes + trailerBytes;

        if (
            payload.size() > maximumPayloadBytes ||
            payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
            (payload.data() == nullptr && !payload.empty())
        ) {
            return EdlgFrameWriteResult::InvalidPayload;
        }

        if (output.size() < overheadBytes || payload.size() > output.size() - overheadBytes) {
            return EdlgFrameWriteResult::InsufficientCapacity;
        }

        const auto outputBegin = reinterpret_cast<std::uintptr_t>(output.data());
        const auto payloadBegin = reinterpret_cast<std::uintptr_t>(payload.data());
        if (!payload.empty() && !output.empty()) {
            // On platforms without a supported integral pointer-address model,
            // callers must enforce this documented no-alias precondition.
            // The straightforward address-range check is only an additional guard.
            if (
                payloadBegin <= outputBegin + output.size() - 1U &&
                outputBegin <= payloadBegin + payload.size() - 1U
            ) {
                return EdlgFrameWriteResult::InvalidPayload;
            }
        }

        constexpr std::byte magic[4U] = {
            std::byte{'E'}, std::byte{'D'}, std::byte{'L'}, std::byte{'G'}
        };
        constexpr std::byte completion[4U] = {
            std::byte{'G'}, std::byte{'L'}, std::byte{'D'}, std::byte{'E'}
        };

        for (std::size_t i = 0U; i < 4U; ++i) {
            output[i] = magic[i];
        }
        output[4U] = std::byte{1U};
        output[5U] = std::byte{static_cast<std::uint8_t>(codec)};
        output[6U] = std::byte{0U};
        output[7U] = std::byte{0U};

        Detail::WriteEdlgLe32(output, 8U, static_cast<std::uint32_t>(payload.size()));

        const std::uint32_t first = Detail::EdlgFrameCrc32C(output.first(12U));
        const std::uint32_t second = Detail::EdlgFrameCrc32C(payload, first);
        const std::uint32_t checksum = Detail::EdlgFrameCrc32C(
            std::span<const std::byte>(completion, 4U), second
        );
        Detail::WriteEdlgLe32(output, 12U, checksum);

        for (std::size_t i = 0U; i < payload.size(); ++i) {
            output[headerBytes + i] = payload[i];
        }
        for (std::size_t i = 0U; i < trailerBytes; ++i) {
            output[headerBytes + payload.size() + i] = completion[i];
        }

        written = overheadBytes + payload.size();
        return EdlgFrameWriteResult::Succeeded;
    }

} // ESPressio::Logging
