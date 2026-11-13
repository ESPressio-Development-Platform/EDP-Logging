#pragma once

#include <limits>

#include <memory/ByteOperationsContract.hpp>

#include "EdlgFrame.hpp"

namespace ESPressio::Logging {

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
    /// @tparam TByteOperationsProvider Existing Memory ByteOperations provider.
    /// @param byteOperations Bootstrap-selected non-overlapping copy provider.
    template<class TByteOperationsProvider>
    [[nodiscard]] EdlgFrameWriteResult WriteEdlgFrame(
        EdlgCodec codec,
        std::span<const std::byte> payload,
        std::span<std::byte> output,
        std::size_t& written,
        std::size_t maximumPayloadBytes,
        const TByteOperationsProvider& byteOperations
    ) noexcept {
        using Traits = Memory::Detail::ByteOperationsProviderTraits<TByteOperationsProvider>;
        static_assert(sizeof(Traits) > 0U);

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
            // Avoid address arithmetic overflow while checking supported linear
            // pointer-address models. Nonlinear targets must still honour the
            // public no-alias precondition.
            const bool overlaps = payloadBegin >= outputBegin
                ? (payloadBegin - outputBegin) < output.size()
                : (outputBegin - payloadBegin) < payload.size();
            if (overlaps) {
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

        Detail::WriteEdlgLe32(
            output,
            8U,
            static_cast<std::uint32_t>(payload.size())
        );

        const std::uint32_t first = Detail::EdlgFrameCrc32C(output.first(12U));
        const std::uint32_t second = Detail::EdlgFrameCrc32C(
            payload,
            first
        );
        const std::uint32_t checksum = Detail::EdlgFrameCrc32C(
            std::span<const std::byte>(
                completion,
                4U
            ),
            second
        );
        Detail::WriteEdlgLe32(
            output,
            12U,
            checksum
        );

        if (!payload.empty()) {
            byteOperations.CopyBytes(
                output.data() + headerBytes,
                payload.data(),
                payload.size()
            );
        }
        for (std::size_t i = 0U; i < trailerBytes; ++i) {
            output[headerBytes + payload.size() + i] = completion[i];
        }

        written = overheadBytes + payload.size();
        return EdlgFrameWriteResult::Succeeded;
    }

} // ESPressio::Logging
