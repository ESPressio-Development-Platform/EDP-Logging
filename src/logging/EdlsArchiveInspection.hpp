#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include <ESPressio_Persistence.hpp>

#include "EdlgFrameReader.hpp"
#include "EdlsSegment.hpp"

namespace ESPressio::Logging {

    /// Truthful result of inspecting one fixed EDLS archive slot in bounded memory.
    enum class EdlsSlotCondition : std::uint8_t {
        /// No bytes were present at this path.
        Empty = 0U,
        /// A valid header and complete contiguous frame prefix cover the observed file.
        Clean = 1U,
        /// A valid header or frame prefix precedes an invalid or truncated tail.
        DamagedTail = 2U,
        /// The observed header could not establish EDLS identity.
        InvalidHeader = 3U,
        /// The recorded physical slot differs from the expected slot.
        SlotMismatch = 4U,
        /// FileStorage could not supply the requested evidence.
        ProviderFailure = 5U
    };

    /// Conservative cross-slot generation/identity assessment. Never issues
    /// a generation when ANY slot is unreadable, inconsistent or exhausted.
    enum class EdlsArchiveCondition : std::uint8_t {
        /// A unique successor generation can be determined from the observations.
        Unambiguous = 0U,
        /// Slot conditions, duplicate generations or device identity prevent a successor plan.
        Ambiguous = 1U,
        /// The highest generation cannot be incremented without wrap.
        GenerationExhausted = 2U
    };

    /// Read-only facts of a single physical slot, never a write authorization.
    struct EdlsSlotInspection final {
        /// Exact classification; only Clean is eligible for future append planning.
        EdlsSlotCondition Condition{EdlsSlotCondition::ProviderFailure};

        /// Parsed EDLS identity when HasHeader is true.
        EdlsHeader Header{};

        /// Whether a complete checksum-valid EDLS header was observed.
        bool HasHeader{false};

        /// Number of contiguous, checksum-verified EDLG records.
        std::size_t VerifiedFrames{0U};

        /// First byte AFTER the last valid contiguous prefix.
        std::uint64_t VerifiedPrefixBytes{0U};
    };

    namespace Detail {

        /// Strongly typed result for bounded exact-file reads.
        enum class EdlsExactReadResult : std::uint8_t {
            /// The provider returned the full requested byte extent.
            Succeeded = 0U,
            /// FileStorage could not supply the requested evidence.
            ProviderFailure = 1U,
            /// The provider accepted the read and returned fewer bytes than requested.
            IncompleteRead = 2U
        };

        /// One exact bounded read, with no assumption of atomic file mutation.
        /// Report provider rejection separately from successful short transfer.
        /// @tparam TStorage FileStorage provider supplying a bounded positional read.
        template<class TStorage>
        [[nodiscard]] EdlsExactReadResult ReadExactEdlsBytes(
            const TStorage& storage,
            ESPressio::Persistence::FilePathView path,
            std::uint64_t offset,
            std::span<std::byte> destination
        ) noexcept {
            using namespace ESPressio::Persistence;
            const auto result = storage.ReadFileAt(
                path,
                StorageOffset{offset},
                DestinationBufferView{destination.data(), destination.size()}
            );
            if (result.Status != FileReadStatus::Succeeded) {
                return EdlsExactReadResult::ProviderFailure;
            }
            return result.BytesTransferred == destination.size()
                ? EdlsExactReadResult::Succeeded
                : EdlsExactReadResult::IncompleteRead;
        }

    } // ESPressio::Logging::Detail

    /// Parse one EDLS slot using only EDP-Persistence::FileStorage operations.
    /// Never repairs, seeks past corruption, mutates files or allocates memory.
    /// Scratch must fit TMaximumPayloadBytes+20. When the whole physical file
    /// fits that caller-owned span, one complete read supplies all validation;
    /// otherwise the existing bounded streaming reads remain authoritative.
    /// No file bytes or integrity evidence survive this invocation. Caller
    /// serializes archive mutation for a coherent observation. A failed bulk
    /// read preserves an independently readable header but never accepts frames.
    ///
    /// @tparam TMaximumPayloadBytes Maximum EDLG payload retained temporarily.
    /// @tparam TStorage Bootstrap-owned EDP-Persistence FileStorage provider.
    template<std::size_t TMaximumPayloadBytes, class TStorage>
    [[nodiscard]] EdlsSlotInspection InspectEdlsSlotWithScratch(
        const TStorage& storage,
        ESPressio::Persistence::FilePathView path,
        std::uint16_t expectedSlot,
        std::span<std::byte> frameScratch
    ) noexcept {
        using namespace ESPressio::Persistence;
        static_assert(TMaximumPayloadBytes > 0U &&
                      TMaximumPayloadBytes <= 4096U,
                      "Archive scan scratch must be bounded to 1..4096 payload bytes");
        const auto queried = storage.GetFileSize(path);
        if (queried.Status == FileSizeStatus::NotFound) {
            return {EdlsSlotCondition::Empty};
        }
        if (queried.Status != FileSizeStatus::Succeeded) {
            return {EdlsSlotCondition::ProviderFailure};
        }
        const auto size = queried.Size.RawValue;
        if (size == 0U) {
            return {EdlsSlotCondition::Empty};
        }
        if (size < EdlsHeaderBytes) {
            return {EdlsSlotCondition::InvalidHeader};
        }
        std::array<std::byte, EdlsHeaderBytes> headerBuffer{};
        std::span<const std::byte> headerBytes{headerBuffer};
        std::span<const std::byte> captured{};
        bool captureFailed = false;
        if (size <= frameScratch.size()) {
            const auto complete = frameScratch.first(static_cast<std::size_t>(size));
            if (Detail::ReadExactEdlsBytes(storage, path, 0U, complete) == Detail::EdlsExactReadResult::Succeeded) {
                captured = complete;
                headerBytes = captured.first(EdlsHeaderBytes);
            } else {
                captureFailed = true;
            }
        }
        if (captured.empty() && Detail::ReadExactEdlsBytes(
                storage, path, 0U, std::span<std::byte>(headerBuffer)
            ) != Detail::EdlsExactReadResult::Succeeded) {
            return {EdlsSlotCondition::ProviderFailure};
        }
        EdlsHeader header{};
        if (ReadEdlsHeader(
            headerBytes,
            header
        ) != EdlsHeaderStatus::Succeeded) {
            return {EdlsSlotCondition::InvalidHeader};
        }
        if (header.Slot != expectedSlot) {
            return {EdlsSlotCondition::SlotMismatch,header,true,0U,EdlsHeaderBytes};
        }
        EdlsSlotInspection result{
            EdlsSlotCondition::Clean,header,true,0U,EdlsHeaderBytes
        };
        // Failed whole-file I/O cannot be promoted into a verified prefix.
        // Retain valid identity/generation provenance for strict recovery:
        // a foreign header must not become an apparently unknown candidate.
        if (captureFailed) {
            result.Condition = EdlsSlotCondition::ProviderFailure;
            return result;
        }
        if (frameScratch.size() < TMaximumPayloadBytes+20U) {
            return {EdlsSlotCondition::ProviderFailure};
        }
        const bool sealed = header.Profile == EdlsContentProfile::SealedFrames;
        if (sealed && size < EdlsHeaderBytes + 20U + EdlsSealBytes) {
            result.Condition = EdlsSlotCondition::DamagedTail;
            return result;
        }
        const auto frameEnd = sealed ? size - EdlsSealBytes : size;
        auto segmentChecksum = Detail::EdlgFrameCrc32C(headerBytes);
        std::array<std::byte, 16U> frameHeader{};
        std::uint64_t position = EdlsHeaderBytes;
        while (position < frameEnd) {
            if (frameEnd - position < 20U) {
                result.Condition = EdlsSlotCondition::DamagedTail;
                return result;
            }
            std::span<const std::byte> frameHeaderBytes{frameHeader};
            if (!captured.empty()) {
                frameHeaderBytes = captured.subspan(
                    static_cast<std::size_t>(position), frameHeader.size()
                );
            } else if (Detail::ReadExactEdlsBytes(
                    storage, path, position, std::span<std::byte>(frameHeader)
                ) != Detail::EdlsExactReadResult::Succeeded) {
                result.Condition = EdlsSlotCondition::ProviderFailure;
                return result;
            }
            const std::uint32_t length =
                std::uint32_t(std::to_integer<std::uint8_t>(frameHeaderBytes[8U])) |
                (std::uint32_t(std::to_integer<std::uint8_t>(frameHeaderBytes[9U])) << 8U) |
                (std::uint32_t(std::to_integer<std::uint8_t>(frameHeaderBytes[10U])) << 16U) |
                (std::uint32_t(std::to_integer<std::uint8_t>(frameHeaderBytes[11U])) << 24U);
            if (length > TMaximumPayloadBytes) {
                result.Condition = EdlsSlotCondition::DamagedTail;
                return result;
            }
            const auto frameBytes = static_cast<std::size_t>(length) + 20U;
            if (frameBytes > frameEnd - position) {
                result.Condition = EdlsSlotCondition::DamagedTail;
                return result;
            }
            std::span<const std::byte> completeFrame{};
            if (!captured.empty()) {
                completeFrame = captured.subspan(
                    static_cast<std::size_t>(position), frameBytes
                );
            } else {
                if (Detail::ReadExactEdlsBytes(
                        storage, path, position, frameScratch.first(frameBytes)
                    ) != Detail::EdlsExactReadResult::Succeeded) {
                    result.Condition = EdlsSlotCondition::ProviderFailure;
                    return result;
                }
                completeFrame = frameScratch.first(frameBytes);
            }
            EdlgFrameView frame{};
            if (ReadEdlgFrame(
                completeFrame,
                frame,TMaximumPayloadBytes
            ) != EdlgFrameReadResult::Succeeded ||
                frame.Consumed != frameBytes) {
                result.Condition = EdlsSlotCondition::DamagedTail;
                return result;
            }
            if (sealed) {
                segmentChecksum = Detail::EdlgFrameCrc32C(
                    completeFrame,
                    segmentChecksum
                );
            }
            position += frameBytes;
            ++result.VerifiedFrames;
            result.VerifiedPrefixBytes = position;
        }
        if (sealed) {
            std::array<std::byte, EdlsSealBytes> seal{};
            std::span<const std::byte> sealBytes{seal};
            if (!captured.empty()) {
                sealBytes = captured.subspan(
                    static_cast<std::size_t>(frameEnd), EdlsSealBytes
                );
            } else if (Detail::ReadExactEdlsBytes(
                    storage, path, frameEnd, seal
                ) != Detail::EdlsExactReadResult::Succeeded) {
                result.Condition = EdlsSlotCondition::ProviderFailure;
                return result;
            }
            const auto expectedChecksum = Detail::EdlgFrameCrc32C(
                sealBytes.first(12U),
                segmentChecksum
            );
            if (sealBytes[0U] != std::byte{'E'} || sealBytes[1U] != std::byte{'D'} ||
                sealBytes[2U] != std::byte{'L'} || sealBytes[3U] != std::byte{'C'} ||
                result.VerifiedFrames == 0U ||
                Detail::ReadEdlsLe(sealBytes, 4U, 4U) != result.VerifiedFrames ||
                Detail::ReadEdlsLe(sealBytes, 8U, 4U) != frameEnd - EdlsHeaderBytes ||
                Detail::ReadEdlsLe(sealBytes, 12U, 4U) != expectedChecksum) {
                result.Condition = EdlsSlotCondition::DamagedTail;
                return result;
            }
            result.VerifiedPrefixBytes = size;
        }
        return result;
    }

    /// Scan one physical slot with a fixed stack frame buffer.
    /// High-payload native workers SHOULD select WithScratch explicitly and
    /// provision the buffer through their existing Bootstrap Memory owner.
    /// @tparam TMaximumPayloadBytes Maximum temporarily retained EDLG payload.
    /// @tparam TStorage Bootstrap-owned EDP-Persistence FileStorage provider.
    template<std::size_t TMaximumPayloadBytes, class TStorage>
    [[nodiscard]] EdlsSlotInspection InspectEdlsSlot(
        const TStorage& storage,
        ESPressio::Persistence::FilePathView path,
        std::uint16_t expectedSlot
    ) noexcept {
        static_assert(TMaximumPayloadBytes>0U && TMaximumPayloadBytes<=4096U);
        std::array<std::byte,TMaximumPayloadBytes+20U> ownedScratch{};
        return InspectEdlsSlotWithScratch<TMaximumPayloadBytes>(
            storage,path,expectedSlot,ownedScratch);
    }

    /// No segment is mutated or reserved; this is read-only planning evidence.
    /// @tparam TSlots Number of application-configured physical paths.
    template<std::size_t TSlots>
    struct EdlsArchiveInspection final {
        /// Overall no-wrap, identity and generation coherency status.
        EdlsArchiveCondition Condition{EdlsArchiveCondition::Ambiguous};

        /// Independently checked physical slot observations.
        std::array<EdlsSlotInspection,TSlots> Slots{};

        /// Newest candidate slot, or TSlots when archive has no valid header.
        std::size_t NewestSlot{TSlots};

        /// Next strictly increasing generation when unambiguous; zero otherwise.
        std::uint64_t NextGeneration{0U};
    };

    /// Read-only bounded assessment; rejects hidden generations, duplicate
    /// generations and mixed 16-byte DeviceIdentifier across fixed slots.
    /// A DamagedTail retains ONLY its verified prefix; no implicit append.
    ///
    /// @tparam TSlots Number of application-owned fixed storage slots (2..64).
    /// @tparam TMaximumPayloadBytes Maximum validated frame payload.
    /// @tparam TStorage Bootstrap-composed EDP-Persistence FileStorage provider.
    template<std::size_t TSlots, std::size_t TMaximumPayloadBytes, class TStorage>
    [[nodiscard]] EdlsArchiveInspection<TSlots> InspectEdlsArchiveWithScratch(
        const TStorage& storage,
        const std::array<ESPressio::Persistence::FilePathView,TSlots>& paths,
        std::span<std::byte> frameScratch
    ) noexcept {
        static_assert(TSlots >= 2U && TSlots <= 64U,
                      "Archive requires a bounded 2..64-slot topology");
        EdlsArchiveInspection<TSlots> result{};
        bool invalid = false;
        for (std::size_t slot = 0U; slot < TSlots; ++slot) {
            result.Slots[slot] = InspectEdlsSlotWithScratch<TMaximumPayloadBytes>(
                storage,paths[slot],static_cast<std::uint16_t>(slot),frameScratch
            );
            const auto& current = result.Slots[slot];
            if (current.Condition == EdlsSlotCondition::InvalidHeader ||
                current.Condition == EdlsSlotCondition::SlotMismatch ||
                current.Condition == EdlsSlotCondition::ProviderFailure) {
                invalid = true;
            }
            if (!current.HasHeader) {
                continue;
            }
            if (result.NewestSlot != TSlots) {
                const auto& preceding = result.Slots[result.NewestSlot];
                if (current.Header.DeviceIdentifier != preceding.Header.DeviceIdentifier ||
                    current.Header.Generation == preceding.Header.Generation) {
                    invalid = true;
                }
                if (current.Header.Generation > preceding.Header.Generation) {
                    result.NewestSlot = slot;
                }
            } else {
                result.NewestSlot = slot;
            }
            // Reject a duplicated generation in ANY prior observed slot.
            for (std::size_t previous = 0U; previous < slot; ++previous) {
                if (result.Slots[previous].HasHeader &&
                    result.Slots[previous].Header.Generation == current.Header.Generation) {
                    invalid = true;
                }
            }
        }
        if (invalid) {
            return result;
        }
        if (result.NewestSlot == TSlots) {
            result.Condition = EdlsArchiveCondition::Unambiguous;
            result.NextGeneration = 1U;
            return result;
        }
        const auto latest = result.Slots[result.NewestSlot].Header.Generation;
        if (latest == std::numeric_limits<std::uint64_t>::max()) {
            result.Condition = EdlsArchiveCondition::GenerationExhausted;
            return result;
        }
        result.Condition = EdlsArchiveCondition::Unambiguous;
        result.NextGeneration = latest + 1U;
        return result;
    }

    /// Inspect all configured paths with one fixed stack frame buffer.
    /// @tparam TSlots Number of application-configured physical paths.
    /// @tparam TMaximumPayloadBytes Maximum temporarily retained EDLG payload.
    /// @tparam TStorage Bootstrap-owned EDP-Persistence FileStorage provider.
    template<std::size_t TSlots, std::size_t TMaximumPayloadBytes, class TStorage>
    [[nodiscard]] EdlsArchiveInspection<TSlots> InspectEdlsArchive(
        const TStorage& storage,
        const std::array<ESPressio::Persistence::FilePathView,TSlots>& paths
    ) noexcept {
        static_assert(TMaximumPayloadBytes>0U && TMaximumPayloadBytes<=4096U);
        std::array<std::byte,TMaximumPayloadBytes+20U> ownedScratch{};
        return InspectEdlsArchiveWithScratch<TSlots,TMaximumPayloadBytes>(
            storage,paths,ownedScratch);
    }

} // namespace ESPressio::Logging
