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
        Empty,
        Clean,
        DamagedTail,
        InvalidHeader,
        SlotMismatch,
        ProviderFailure
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

        /// One exact bounded read, with no assumption of atomic file mutation.
        template<class TStorage>
        [[nodiscard]] bool ReadExactEdlsBytes(
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
            return result.Status == FileReadStatus::Succeeded &&
                   result.BytesTransferred == destination.size();
        }

    } // ESPressio::Logging::Detail

    /// Parse one EDLS slot using only EDP-Persistence::FileStorage operations.
    /// Never repairs, seeks past corruption, mutates files or allocates memory.
    /// The complete frame scratch is bounded by TMaximumPayloadBytes+20.
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
        if (!Detail::ReadExactEdlsBytes(
            storage, path, 0U, std::span<std::byte>(headerBuffer)
        )) {
            return {EdlsSlotCondition::ProviderFailure};
        }
        EdlsHeader header{};
        if (ReadEdlsHeader(headerBuffer, header) != EdlsHeaderStatus::Succeeded) {
            return {EdlsSlotCondition::InvalidHeader};
        }
        if (header.Slot != expectedSlot) {
            return {EdlsSlotCondition::SlotMismatch,header,true,0U,EdlsHeaderBytes};
        }
        EdlsSlotInspection result{
            EdlsSlotCondition::Clean,header,true,0U,EdlsHeaderBytes
        };
        if (frameScratch.size() < TMaximumPayloadBytes+20U) {
            return {EdlsSlotCondition::ProviderFailure};
        }
        std::array<std::byte, 16U> frameHeader{};
        std::uint64_t position = EdlsHeaderBytes;
        while (position < size) {
            if (size - position < 20U) {
                result.Condition = EdlsSlotCondition::DamagedTail;
                return result;
            }
            if (!Detail::ReadExactEdlsBytes(
                storage, path, position, std::span<std::byte>(frameHeader)
            )) {
                result.Condition = EdlsSlotCondition::ProviderFailure;
                return result;
            }
            const std::uint32_t length =
                std::uint32_t(std::to_integer<std::uint8_t>(frameHeader[8U])) |
                (std::uint32_t(std::to_integer<std::uint8_t>(frameHeader[9U])) << 8U) |
                (std::uint32_t(std::to_integer<std::uint8_t>(frameHeader[10U])) << 16U) |
                (std::uint32_t(std::to_integer<std::uint8_t>(frameHeader[11U])) << 24U);
            if (length > TMaximumPayloadBytes) {
                result.Condition = EdlsSlotCondition::DamagedTail;
                return result;
            }
            const auto frameBytes = static_cast<std::size_t>(length) + 20U;
            if (frameBytes > size - position) {
                result.Condition = EdlsSlotCondition::DamagedTail;
                return result;
            }
            if (!Detail::ReadExactEdlsBytes(
                storage, path, position,
                frameScratch.first(frameBytes)
            )) {
                result.Condition = EdlsSlotCondition::ProviderFailure;
                return result;
            }
            EdlgFrameView frame{};
            if (ReadEdlgFrame(
                std::span<const std::byte>(frameScratch.data(),frameBytes),
                frame,TMaximumPayloadBytes
            ) != EdlgFrameReadResult::Succeeded ||
                frame.Consumed != frameBytes) {
                result.Condition = EdlsSlotCondition::DamagedTail;
                return result;
            }
            position += frameBytes;
            ++result.VerifiedFrames;
            result.VerifiedPrefixBytes = position;
        }
        return result;
    }

    /// Backward-compatible bounded single-slot scan, using stack scratch.
    /// High-payload native workers SHOULD select WithScratch explicitly and
    /// provision the buffer through their existing Bootstrap Memory owner.
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

    /// Conservative cross-slot generation/identity assessment. Never issues
    /// a generation when ANY slot is unreadable, inconsistent or exhausted.
    enum class EdlsArchiveCondition : std::uint8_t {
        Unambiguous,
        Ambiguous,
        GenerationExhausted
    };

    /// No segment is mutated or reserved; this is read-only planning evidence.
    /// TSlots is the number of exact application-configured paths.
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

    /// Existing stack-backed API remains stable. Worker-side high-payload
    /// call sites can use InspectEdlsArchiveWithScratch for predictable
    /// low stack usage without creating any duplicate filesystem owner.
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
