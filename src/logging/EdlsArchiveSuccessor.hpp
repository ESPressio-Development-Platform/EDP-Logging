#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <limits>

#include <ESPressio_Persistence.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>
#include <memory/ByteOperationsContract.hpp>

#include "EdlgFrameReader.hpp"
#include "EdlsArchiveInspection.hpp"

namespace ESPressio::Logging {

    /// Result of introducing a new EDLS generation into a PHYSICALLY EMPTY
    /// slot, never destroying prior history to make space.
    enum class EdlsSuccessorStatus : std::uint8_t {
        /// Independent readback confirmed the new generation and frame.
        PublishedAndVerified = 0U,
        /// The proposed identity or complete frame input was rejected.
        InvalidIdentityOrFrame = 1U,
        /// Inspection could not establish an unambiguous successor generation.
        ArchiveAmbiguous = 2U,
        /// No configured physical path was available for exclusive creation.
        NoEmptySlot = 3U,
        /// The complete proposed segment exceeds its configured byte bound.
        SlotCapacityExceeded = 4U,
        /// The selected physical path no longer satisfies preflight.
        ChangedDuringPreflight = 5U,
        /// The FileStorage owner declined replacement.
        ProviderRejected = 6U,
        /// The proposed generation or frame could not be confirmed independently.
        ReadbackUnconfirmed = 7U
    };

    /// Exact provider mutation and independent readback observation.
    /// @tparam TSlots Number of physical paths and the unselected-slot sentinel.
    template <std::size_t TSlots>
    struct EdlsSuccessorOutcome final {
        /// Logical source-side publication state, not media durability.
        EdlsSuccessorStatus Status{EdlsSuccessorStatus::ArchiveAmbiguous};

        /// Native FileStorage ReplaceFile result, if invoked.
        ESPressio::Persistence::FileReplaceStatus Native{
            ESPressio::Persistence::FileReplaceStatus::NotReady};

        /// Physical slot chosen, or TSlots when no slot selected.
        std::size_t Slot{TSlots};

        /// Proposed generation, or zero when none proposed.
        std::uint64_t Generation{0U};
    };

    /// Publish one generation in an EXISTINGLY EMPTY physical EDLS slot.
    ///
    /// This is a no-destructive-overwrite, bounded archive continuation
    /// primitive. It uses existing FileStorage ReplaceFile, the canonical
    /// EDLS writer and existing EDLG reader. It will not discard older
    /// generations; when every slot is occupied it returns NoEmptySlot
    /// instead of pretending a safe circular rotation was certified.
    ///
    /// Bootstrap holds exclusive access to every file in the complete
    /// 2..64-slot topology across scan, mutation and independent readback.
    /// No filesystem atomicity follows merely from successful ReplaceFile:
    /// if the provider exposes MayModify/None interruption semantics, an
    /// interrupted write may leave the empty target damaged; prior clean
    /// slots remain untouched. Future recovery must then fail closed.
    ///
    /// @tparam TSlots Bounded number of application-owned EDLS file paths.
    /// @tparam TPayloadMaximum Maximum EDLG payload scratch (1..4096).
    /// @tparam TSlotMaximum Hard upper byte count for one physical slot.
    /// @tparam TStorage Application-composed abstract FileStorage provider.
    /// @tparam TMaximumFrames Explicit opt-in maximum frames per segment; default one.
    /// Caller scratch must be disjoint from the input frames and hold the complete segment.
    /// @tparam TByteOperationsProvider Stateless Memory byte operations owner selected at Bootstrap.
    template <
        std::size_t TSlots,
        std::size_t TPayloadMaximum,
        std::size_t TSlotMaximum,
        class TStorage,
        std::size_t TMaximumFrames = 1U,
        class TByteOperationsProvider = ESPressio::Platform::Portable::Memory::ByteOperationsProvider>
    [[nodiscard]] EdlsSuccessorOutcome<TSlots> PublishEdlsSuccessorIntoEmptySlotWithScratch(
        TStorage &storage,
        const std::array<ESPressio::Persistence::FilePathView, TSlots> &paths,
        const std::array<std::byte, 16U> &deviceIdentifier,
        std::uint32_t runtimeIncarnation,
        std::span<const std::byte> completeEdlg,
        std::span<std::byte> callerScratch,
        const EdlsArchiveInspection<TSlots> *alreadyVerifiedInThisTransaction = nullptr) noexcept {
        using namespace ESPressio::Persistence;
        using ByteOperations = ESPressio::Memory::Detail::ByteOperationsProviderTraits<TByteOperationsProvider>;
        static_assert(sizeof(ByteOperations) > 0U);
        static_assert(std::is_nothrow_default_constructible_v<TByteOperationsProvider>);
        static_assert(TSlots >= 2U && TSlots <= 64U);
        static_assert(TMaximumFrames >= 1U && TMaximumFrames <= 64U);
        static_assert(TPayloadMaximum >= 1U && TPayloadMaximum <= 4096U);
        static_assert(TSlotMaximum >= EdlsHeaderBytes + 20U);
        static_assert(ESPressio::Persistence::Detail::HasMandatoryFileStorageOperations<TStorage>);
        EdlsSuccessorOutcome<TSlots> outcome{};
        EdlsHeader header{};
        constexpr std::size_t sealBytes = TMaximumFrames > 1U ? EdlsSealBytes : 0U;
        if constexpr (TMaximumFrames > 1U) {
            header.Profile = EdlsContentProfile::SealedFrames;
        }
        header.DeviceIdentifier = deviceIdentifier;
        header.RuntimeIncarnation = runtimeIncarnation;
        if (!Detail::IsValidEdlsIdentity(EdlsHeader{
                0U, 1U, deviceIdentifier, runtimeIncarnation}) ||
            completeEdlg.empty() ||
            (TMaximumFrames == 1U && completeEdlg.size() > TPayloadMaximum + 20U)) {
            outcome.Status = EdlsSuccessorStatus::InvalidIdentityOrFrame;
            return outcome;
        }
        // Archive inspection reuses caller scratch before successor assembly.
        // A frame that overlaps any part of that workspace cannot be preserved.
        if (Detail::DoEdlgByteRangesOverlap(
            completeEdlg,
            callerScratch
        )) {
            outcome.Status = EdlsSuccessorStatus::InvalidIdentityOrFrame;
            return outcome;
        }

        // The SAME canonical reader validates every record before any file
        // mutation. Default callers still require exactly one complete frame.
        std::size_t frameCount = 0U;
        std::size_t position = 0U;
        while (position < completeEdlg.size()) {
            if (frameCount == TMaximumFrames) {
                outcome.Status = EdlsSuccessorStatus::InvalidIdentityOrFrame;
                return outcome;
            }
            EdlgFrameView parsed{};
            if (ReadEdlgFrame(
                    completeEdlg.subspan(position),
                    parsed,
                    TPayloadMaximum) != EdlgFrameReadResult::Succeeded) {
                outcome.Status = EdlsSuccessorStatus::InvalidIdentityOrFrame;
                return outcome;
            }
            position += parsed.Consumed;
            ++frameCount;
        }
        if (TSlotMaximum < EdlsHeaderBytes + sealBytes ||
            completeEdlg.size() > TSlotMaximum - EdlsHeaderBytes - sealBytes ||
            completeEdlg.size() > std::numeric_limits<std::uint32_t>::max()) {
            outcome.Status = EdlsSuccessorStatus::SlotCapacityExceeded;
            return outcome;
        }
        if (callerScratch.size() < EdlsHeaderBytes + TPayloadMaximum + 20U ||
            callerScratch.size() < EdlsHeaderBytes + completeEdlg.size() + sealBytes) {
            outcome.Status = EdlsSuccessorStatus::SlotCapacityExceeded;
            return outcome;
        }
        // An exclusive owner may reuse the archive's already CRC-qualified
        // immutable snapshot from an immediately preceding full physical
        // scan in THIS SAME transaction. No cross-publication cache is used;
        // physical vacancy is independently rechecked below, and the newly
        // written segment is fully CRC-read back before returning success.
        // Public default remains the original fresh whole-archive scan.
        const auto current = alreadyVerifiedInThisTransaction != nullptr
                                 ? *alreadyVerifiedInThisTransaction
                                 : InspectEdlsArchiveWithScratch<TSlots, TPayloadMaximum>(
                                       storage,
                                           paths,
                                           callerScratch);

        if (current.Condition != EdlsArchiveCondition::Unambiguous ||
            current.NextGeneration == 0U) {
            outcome.Status = EdlsSuccessorStatus::ArchiveAmbiguous;
            return outcome;
        }
        if (current.NewestSlot != TSlots &&
            current.Slots[current.NewestSlot].Header.DeviceIdentifier !=
                deviceIdentifier) {
            outcome.Status = EdlsSuccessorStatus::InvalidIdentityOrFrame;
            return outcome;
        }
        // Prefer next physical slot after newest, but NEVER replace a
        // populated generation regardless of cyclic position.
        const auto first = current.NewestSlot == TSlots
                               ? 0U
                               : (current.NewestSlot + 1U) % TSlots;
        for (std::size_t advance = 0U; advance < TSlots; ++advance) {
            const auto candidate = (first + advance) % TSlots;
            if (current.Slots[candidate].Condition == EdlsSlotCondition::Empty) {
                outcome.Slot = candidate;
                break;
            }
        }
        if (outcome.Slot == TSlots) {
            outcome.Status = EdlsSuccessorStatus::NoEmptySlot;
            return outcome;
        }
        outcome.Generation = current.NextGeneration;
        header.Slot = static_cast<std::uint16_t>(outcome.Slot);
        header.Generation = outcome.Generation;
        // A separate size check catches obvious mutation between the full
        // inspection and the ReplaceFile call. It does NOT substitute for the
        // application's exclusive writer lock.
        const auto latest = storage.GetFileSize(paths[outcome.Slot]);
        if (latest.Status != FileSizeStatus::NotFound &&
            !(latest.Status == FileSizeStatus::Succeeded &&
              latest.Size.RawValue == 0U)) {
            outcome.Status = EdlsSuccessorStatus::ChangedDuringPreflight;
            return outcome;
        }
        // The checked archive scan is complete before this scratch is
        // reused for immutable successor assembly; there is NO live borrow.
        auto buffer = callerScratch.first(EdlsHeaderBytes + completeEdlg.size() + sealBytes);
        if (WriteEdlsHeader(
            header,
            buffer
        ) != EdlsHeaderStatus::Succeeded) {
            outcome.Status = EdlsSuccessorStatus::InvalidIdentityOrFrame;
            return outcome;
        }
        TByteOperationsProvider{}.CopyBytes(
            buffer.data() + EdlsHeaderBytes,
            completeEdlg.data(),
            completeEdlg.size()
        );
        const auto sealOffset = EdlsHeaderBytes + completeEdlg.size();
        if constexpr (TMaximumFrames > 1U) {
            buffer[sealOffset] = std::byte{'E'};
            buffer[sealOffset + 1U] = std::byte{'D'};
            buffer[sealOffset + 2U] = std::byte{'L'};
            buffer[sealOffset + 3U] = std::byte{'C'};
            Detail::WriteEdlsLe(
                buffer,
                sealOffset + 4U,
                frameCount,
                4U
            );
            Detail::WriteEdlsLe(
                buffer,
                sealOffset + 8U,
                completeEdlg.size(),
                4U
            );
            const auto checksum = Detail::EdlgFrameCrc32C(buffer.first(sealOffset + 12U));
            Detail::WriteEdlsLe(
                buffer,
                sealOffset + 12U,
                checksum,
                4U
            );
        }
        const auto used = sealOffset + sealBytes;
        outcome.Native = storage.ReplaceFile(
            paths[outcome.Slot],
                SourceBufferView{buffer.data(), used});
        if (outcome.Native != FileReplaceStatus::Succeeded) {
            outcome.Status = EdlsSuccessorStatus::ProviderRejected;
            return outcome;
        }
        const auto verification = InspectEdlsSlotWithScratch<TPayloadMaximum>(
            storage,
                paths[outcome.Slot],
                header.Slot,
                callerScratch);
        const auto measured = storage.GetFileSize(paths[outcome.Slot]);
        if (verification.Condition != EdlsSlotCondition::Clean ||
            !verification.HasHeader ||
            verification.Header.Generation != header.Generation ||
            verification.Header.DeviceIdentifier != header.DeviceIdentifier ||
            verification.Header.RuntimeIncarnation != header.RuntimeIncarnation ||
            verification.Header.Profile != header.Profile ||
            verification.VerifiedFrames != frameCount ||
            verification.VerifiedPrefixBytes != used ||
            measured.Status != FileSizeStatus::Succeeded ||
            measured.Size.RawValue != used) {
            outcome.Status = EdlsSuccessorStatus::ReadbackUnconfirmed;
            return outcome;
        }
        // The exact size was verified and fits in caller scratch. The clean
        // inspection captured the complete successor, including any seal.
        if (TByteOperationsProvider{}.CompareBytes(
            callerScratch.data() + EdlsHeaderBytes,
            completeEdlg.data(),
            completeEdlg.size()
        ) != ESPressio::Memory::ByteComparison::Equal) {
            outcome.Status = EdlsSuccessorStatus::ReadbackUnconfirmed;
            return outcome;
        }
        outcome.Status = EdlsSuccessorStatus::PublishedAndVerified;
        return outcome;
    }

    /// Publish one complete frame with fixed stack scratch.
    /// High-payload workers can provision Bootstrap-owned scratch via WithScratch.
    /// @tparam TSlots Number of application-owned physical EDLS paths.
    /// @tparam TPayloadMaximum Maximum EDLG payload temporarily retained.
    /// @tparam TSlotMaximum Maximum bytes permitted in one physical slot.
    /// @tparam TStorage Application-composed FileStorage owner.
    /// @tparam TByteOperationsProvider Stateless Memory byte operations owner selected at Bootstrap.
    template <std::size_t TSlots, std::size_t TPayloadMaximum, std::size_t TSlotMaximum,
              class TStorage, class TByteOperationsProvider = ESPressio::Platform::Portable::Memory::ByteOperationsProvider>
    [[nodiscard]] EdlsSuccessorOutcome<TSlots> PublishEdlsSuccessorIntoEmptySlot(
        TStorage &storage,
        const std::array<ESPressio::Persistence::FilePathView, TSlots> &paths,
        const std::array<std::byte, 16U> &deviceIdentifier,
        std::uint32_t runtimeIncarnation,
        std::span<const std::byte> completeEdlg) noexcept {
        static_assert(TPayloadMaximum > 0U && TPayloadMaximum <= 4096U);
        std::array<std::byte, EdlsHeaderBytes + TPayloadMaximum + 20U> scratch{};
        return PublishEdlsSuccessorIntoEmptySlotWithScratch<
            TSlots,
            TPayloadMaximum,
            TSlotMaximum,
            TStorage,
            1U,
            TByteOperationsProvider>(
                storage,
                paths,
                deviceIdentifier,
                runtimeIncarnation,
                completeEdlg,
                scratch
            );
    }

} // namespace ESPressio::Logging
