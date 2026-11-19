#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ESPressio::Logging {

    /// Portable, independently framed encoded LogRecord payload format.
    enum class EdlgCodec : std::uint8_t {
        /// The frame carries the canonical CBOR payload representation.
        Cbor = 1,
        /// The frame carries the canonical JSON payload representation.
        Json = 2
    };

    /// Result of constructing one complete framed record into caller-owned storage.
    enum class EdlgFrameWriteResult : std::uint8_t {
        /// The complete frame and written byte count were produced.
        Succeeded = 0,
        /// The supplied codec is outside the supported wire vocabulary.
        InvalidCodec = 1,
        /// Payload size, address or overlap violates the write contract.
        InvalidPayload = 2,
        /// Caller storage cannot hold the complete header, payload and trailer.
        InsufficientCapacity = 3
    };

    namespace Detail {

        /// Detects overlapping nonempty byte spans on the supported linear address model.
        [[nodiscard]] inline bool DoEdlgByteRangesOverlap(
            std::span<const std::byte> source,
            std::span<const std::byte> destination
        ) noexcept {
            if (source.empty() || destination.empty()) {
                return false;
            }
            const auto sourceAddress = reinterpret_cast<std::uintptr_t>(source.data());
            const auto destinationAddress = reinterpret_cast<std::uintptr_t>(destination.data());
            return sourceAddress >= destinationAddress
                ? sourceAddress - destinationAddress < destination.size()
                : destinationAddress - sourceAddress < source.size();
        }

        /// CRC32C (reflected Castagnoli) over a sequence of already encoded bytes.
        ///
        /// The initial parameter is the finalized result for the previous block, which
        /// permits header/payload/trailer accumulation without allocating a buffer.
        ///
        /// @param bytes Next wire-encoded block in checksum order.
        /// @param previous Finalized CRC32C from the previous block, or zero for the first.
        /// @return Finalized CRC32C through this block.
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

        /// Stores one unsigned value in the fixed 32-bit little-endian wire format.
        ///
        /// @param output Buffer with four writable bytes at offset.
        /// @param offset First byte to write.
        /// @param value Unsigned value to encode.
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

} // ESPressio::Logging
