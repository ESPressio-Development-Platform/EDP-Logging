#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "EdlgFrame.hpp"

namespace ESPressio::Logging {

    /// Exact size of one version-1 EDLS segmented archive slot header.
    inline constexpr std::size_t EdlsHeaderBytes = 40U;

    /// Outcome from one bounded portable segment-header operation.
    enum class EdlsHeaderStatus : std::uint8_t {
        Succeeded = 0,
        InvalidArguments = 1,
        InsufficientCapacity = 2,
        MalformedHeader = 3,
        UnsupportedVersion = 4,
        ChecksumMismatch = 5
    };

    /// Value-only representation of a validated EDLS segment-header record.
    struct EdlsHeader final {
        std::uint16_t Slot = 0U;
        std::uint64_t Generation = 0U;
        std::array<std::byte, 16U> DeviceIdentifier{};
        std::uint32_t RuntimeIncarnation = 0U;
    };

    namespace Detail {

        /// Little-endian absolute-width store; never emit a native C++ structure.
        inline void WriteEdlsLe(
            std::span<std::byte> destination,
            std::size_t offset,
            std::uint64_t value,
            std::size_t width
        ) noexcept {
            for (std::size_t index = 0U; index < width; ++index) {
                destination[offset + index] = std::byte{
                    static_cast<std::uint8_t>(value & 0xFFU)
                };
                value >>= 8U;
            }
        }

        /// Little-endian absolute-width parse; caller establishes bounds first.
        [[nodiscard]] inline std::uint64_t ReadEdlsLe(
            std::span<const std::byte> source,
            std::size_t offset,
            std::size_t width
        ) noexcept {
            std::uint64_t result = 0U;
            for (std::size_t index = 0U; index < width; ++index) {
                result |= static_cast<std::uint64_t>(
                    std::to_integer<std::uint8_t>(source[offset + index])
                ) << (8U * index);
            }
            return result;
        }

        /// Reject uninitialized or externally corrupted archive identities.
        [[nodiscard]] inline bool ValidEdlsIdentity(
            const EdlsHeader& header
        ) noexcept {
            if (header.Generation == 0U || header.RuntimeIncarnation == 0U) {
                return false;
            }
            for (const auto value : header.DeviceIdentifier) {
                if (value != std::byte{0U}) {
                    return true;
                }
            }
            return false;
        }

    } // ESPressio::Logging::Detail

    /// Encode one complete 40-byte, checksum-protected EDLS v1 header.
    ///
    /// A header describes one physical slot and its pre-established, nonzero
    /// generation. It does NOT acquire generation, write to flash, issue a
    /// durability guarantee, or bypass the archive owner's no-wrap policy.
    /// Inputs are validated before any destination byte is changed.
    [[nodiscard]] inline EdlsHeaderStatus WriteEdlsHeader(
        const EdlsHeader& header,
        std::span<std::byte> destination
    ) noexcept {
        if (!Detail::ValidEdlsIdentity(header)) {
            return EdlsHeaderStatus::InvalidArguments;
        }
        if (destination.size() < EdlsHeaderBytes) {
            return EdlsHeaderStatus::InsufficientCapacity;
        }

        destination[0U] = std::byte{'E'};
        destination[1U] = std::byte{'D'};
        destination[2U] = std::byte{'L'};
        destination[3U] = std::byte{'S'};
        destination[4U] = std::byte{1U};
        destination[5U] = std::byte{0U};

        Detail::WriteEdlsLe(destination, 6U, header.Slot, 2U);
        Detail::WriteEdlsLe(destination, 8U, header.Generation, 8U);
        for (std::size_t index = 0U; index < 16U; ++index) {
            destination[16U + index] = header.DeviceIdentifier[index];
        }
        Detail::WriteEdlsLe(destination, 32U, header.RuntimeIncarnation, 4U);

        const auto checksum = Detail::EdlgFrameCrc32C(
            std::span<const std::byte>(destination.data(), 36U)
        );
        Detail::WriteEdlsLe(destination, 36U, checksum, 4U);
        return EdlsHeaderStatus::Succeeded;
    }

    /// Validate one exact EDLS header and publish decoded facts only on success.
    ///
    /// CRC32C is accidental-corruption detection, not authentication. The caller
    /// separately checks that the header's Slot matches the addressed storage
    /// path and that its Generation participates in the bounded archive plan.
    [[nodiscard]] inline EdlsHeaderStatus ReadEdlsHeader(
        std::span<const std::byte> source,
        EdlsHeader& destination
    ) noexcept {
        if (source.size() != EdlsHeaderBytes) {
            return EdlsHeaderStatus::MalformedHeader;
        }
        if (
            source[0U] != std::byte{'E'} ||
            source[1U] != std::byte{'D'} ||
            source[2U] != std::byte{'L'} ||
            source[3U] != std::byte{'S'}
        ) {
            return EdlsHeaderStatus::MalformedHeader;
        }
        if (source[4U] != std::byte{1U}) {
            return EdlsHeaderStatus::UnsupportedVersion;
        }
        if (source[5U] != std::byte{0U}) {
            return EdlsHeaderStatus::MalformedHeader;
        }
        const auto expected = Detail::EdlgFrameCrc32C(source.first(36U));
        const auto encoded = static_cast<std::uint32_t>(
            Detail::ReadEdlsLe(source, 36U, 4U)
        );
        if (encoded != expected) {
            return EdlsHeaderStatus::ChecksumMismatch;
        }

        EdlsHeader parsed{};
        parsed.Slot = static_cast<std::uint16_t>(Detail::ReadEdlsLe(source, 6U, 2U));
        parsed.Generation = Detail::ReadEdlsLe(source, 8U, 8U);
        for (std::size_t index = 0U; index < 16U; ++index) {
            parsed.DeviceIdentifier[index] = source[16U + index];
        }
        parsed.RuntimeIncarnation = static_cast<std::uint32_t>(
            Detail::ReadEdlsLe(source, 32U, 4U)
        );
        if (!Detail::ValidEdlsIdentity(parsed)) {
            return EdlsHeaderStatus::MalformedHeader;
        }
        destination = parsed;
        return EdlsHeaderStatus::Succeeded;
    }

} // ESPressio::Logging
