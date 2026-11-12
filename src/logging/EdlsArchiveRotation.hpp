#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <ESPressio_Persistence.hpp>

#include "EdlsArchiveSuccessor.hpp"

namespace ESPressio::Logging {

    /// A no-cross-slot-mutation best-effort EDLS rotation decision. This is
    /// provider/readback truth, never an acknowledgement of power-cut retention.
    enum class EdlsRollingStatus : std::uint8_t {
        PublishedInEmptySlot = 0U,
        PublishedAfterOldestReclaim = 1U,
        InvalidIdentityOrFrame = 2U,
        ArchiveAmbiguous = 3U,
        SlotCapacityExceeded = 4U,
        ChangedDuringPreflight = 5U,
        ProviderRejected = 6U,
        ReadbackUnconfirmed = 7U,
        PriorNewestUnconfirmed = 8U,
        NoReclaimableSlot = 9U
    };

    /// Separates the native FileStorage mutation from surviving generation
    /// evidence and the independent readback of a newly published frame.
    template <std::size_t TSlots>
    struct EdlsRollingOutcome final {

        /// Precisely qualified outcome; never implies atomicity/durability.
        EdlsRollingStatus Status{EdlsRollingStatus::ArchiveAmbiguous};

        /// Native EDP-Persistence ReplaceFile outcome, if called.
        ESPressio::Persistence::FileReplaceStatus Native{
            ESPressio::Persistence::FileReplaceStatus::NotReady};

        /// Selected physical slot, TSlots when no slot was selected.
        std::size_t Slot{TSlots};

        /// Old newest surviving generation, or TSlots for an empty archive.
        std::size_t ProtectedNewestSlot{TSlots};

        /// Proposed successor generation, or zero before preflight.
        std::uint64_t Generation{0U};

        /// True only after a separate clean readback of prior newest slot.
        bool PriorNewestVerified{false};
    };

    namespace Detail {

        /// Outcomes of independently re-reading a previously clean EDLS slot.
        enum class EdlsPredecessorVerificationResult : std::uint8_t {
            /// Header identity and CRC-validated frame count and prefix length agree.
            Verified = 0U,

            /// The original inspection did not establish a clean header.
            InvalidBaseline = 1U,

            /// FileStorage could not complete the reinspection.
            ProviderFailure = 2U,

            /// The slot no longer contains a complete, clean EDLS segment.
            ReadbackNotClean = 3U,

            /// The clean segment's generation, device, incarnation or profile changed.
            HeaderChanged = 4U,

            /// The clean segment's verified frame count or prefix length changed.
            VerifiedPrefixChanged = 5U
        };

        /// Reinspect one physical slot using caller-owned frame scratch and
        /// compare its header identity and CRC-verified prefix with the baseline.
        template <
            std::size_t TPayloadMaximum,
            class TStorage>
        [[nodiscard]] EdlsPredecessorVerificationResult
        VerifyEdlsPredecessorWithScratch(
            const TStorage &storage,
            ESPressio::Persistence::FilePathView path,
            std::size_t physicalSlot,
            const EdlsSlotInspection &before,
            std::span<std::byte> scratch) noexcept {

            using Result = EdlsPredecessorVerificationResult;
            if (before.Condition != EdlsSlotCondition::Clean ||
                !before.HasHeader) {
                return Result::InvalidBaseline;
            }

            const auto after = InspectEdlsSlotWithScratch<TPayloadMaximum>(
                storage,
                path,
                static_cast<std::uint16_t>(physicalSlot),
                scratch);
            if (after.Condition == EdlsSlotCondition::ProviderFailure) {
                return Result::ProviderFailure;
            }

            if (after.Condition != EdlsSlotCondition::Clean || !after.HasHeader) {
                return Result::ReadbackNotClean;
            }

            if (after.Header.Generation != before.Header.Generation ||
                after.Header.DeviceIdentifier != before.Header.DeviceIdentifier ||
                after.Header.RuntimeIncarnation != before.Header.RuntimeIncarnation ||
                after.Header.Profile != before.Header.Profile) {
                return Result::HeaderChanged;
            }

            if (after.VerifiedFrames != before.VerifiedFrames ||
                after.VerifiedPrefixBytes != before.VerifiedPrefixBytes) {
                return Result::VerifiedPrefixChanged;
            }

            return Result::Verified;
        }

        /// Reinspect one physical slot using fixed frame scratch on the stack.
        template <
            std::size_t TPayloadMaximum,
            class TStorage>
        [[nodiscard]] EdlsPredecessorVerificationResult VerifyEdlsPredecessor(
            const TStorage &storage,
            ESPressio::Persistence::FilePathView path,
            std::size_t physicalSlot,
            const EdlsSlotInspection &before) noexcept {

            std::array<std::byte, TPayloadMaximum + 20U> scratch{};
            return VerifyEdlsPredecessorWithScratch<TPayloadMaximum>(
                storage,
                path,
                physicalSlot,
                before,
                scratch);
        }

    } // namespace Detail

    /// Advance a finite EDLS topology, retaining the previous newest slot
    /// WITHOUT touching it even when all physical slots are occupied.
    ///
    /// When empty slots exist, delegates to the existing strictly no-overwrite
    /// successor primitive. When all slots are CLEAN, chooses the physically
    /// oldest verified generation, never the newest, and replaces only that
    /// slot. A native failure may damage precisely the selected old slot.
    ///
    /// Bootstrap MUST hold one exclusive application/provider writer claim
    /// across the complete inspect/preflight/Replace/readback sequence.
    /// FileStorage providers with InterruptionAtomicity::None or
    /// FailurePreservation::MayModify cannot certify durable bytes across
    /// external power loss even after this returns PublishedAfterOldestReclaim.
    /// This operation only verifies normal provider readback and the logical
    /// continuation of the pre-existing newest segment.
    ///
    /// Full-archive corruption, damaged tails and generation conflicts fail
    /// closed; no "repair" based on a guessed current era is attempted.
    /// Later recovery explicitly decides whether a damaged retired slot may
    /// be overwritten again. This routine makes no such destructive decision.
    ///
    /// @tparam TSlots Physical fixed slot count, 2..64.
    /// @tparam TPayloadMaximum Finite EDLG payload maximum, 1..4096 bytes.
    /// @tparam TSlotMaximum Hard physical segment byte capacity.
    /// @tparam TStorage Bootstrap-selected EDP-Persistence FileStorage owner.
    template <
        std::size_t TSlots,
        std::size_t TPayloadMaximum,
        std::size_t TSlotMaximum,
        class TStorage>
    [[nodiscard]] EdlsRollingOutcome<TSlots> PublishEdlsOldestEligibleSuccessor(
        TStorage &storage,
        const std::array<ESPressio::Persistence::FilePathView, TSlots> &paths,
        const std::array<std::byte, 16U> &deviceIdentifier,
        std::uint32_t runtimeIncarnation,
        std::span<const std::byte> completeEdlg) noexcept {
        using namespace ESPressio::Persistence;

        static_assert(TSlots >= 2U && TSlots <= 64U);
        static_assert(TPayloadMaximum >= 1U && TPayloadMaximum <= 4096U);
        static_assert(TSlotMaximum >= EdlsHeaderBytes + 20U);
        static_assert(ESPressio::Persistence::Detail::HasMandatoryFileStorageOperations<TStorage>);

        EdlsRollingOutcome<TSlots> result{};
        const auto current = InspectEdlsArchive<TSlots, TPayloadMaximum>(
            storage, paths);
        if (current.Condition != EdlsArchiveCondition::Unambiguous ||
            current.NextGeneration == 0U) {
            return result;
        }
        if (current.NewestSlot != TSlots &&
            current.Slots[current.NewestSlot].Header.DeviceIdentifier !=
                deviceIdentifier) {
            result.Status = EdlsRollingStatus::InvalidIdentityOrFrame;
            return result;
        }
        if (!Detail::IsValidEdlsIdentity(EdlsHeader{
                0U, 1U, deviceIdentifier, runtimeIncarnation}) ||
            completeEdlg.size() > TPayloadMaximum + 20U) {
            result.Status = EdlsRollingStatus::InvalidIdentityOrFrame;
            return result;
        }
        EdlgFrameView frame{};
        if (ReadEdlgFrame(completeEdlg, frame, TPayloadMaximum) !=
                EdlgFrameReadResult::Succeeded ||
            frame.Consumed != completeEdlg.size()) {
            result.Status = EdlsRollingStatus::InvalidIdentityOrFrame;
            return result;
        }
        if (completeEdlg.size() > TSlotMaximum - EdlsHeaderBytes) {
            result.Status = EdlsRollingStatus::SlotCapacityExceeded;
            return result;
        }

        result.ProtectedNewestSlot = current.NewestSlot;
        result.Generation = current.NextGeneration;

        // Reuse the already-qualified no-overwrite path for available slots.
        for (std::size_t i = 0U; i < TSlots; ++i) {
            if (current.Slots[i].Condition == EdlsSlotCondition::Empty) {
                const auto appended = PublishEdlsSuccessorIntoEmptySlot<
                    TSlots,
                    TPayloadMaximum,
                    TSlotMaximum>(storage, paths, deviceIdentifier, runtimeIncarnation, completeEdlg);
                result.Native = appended.Native;
                result.Slot = appended.Slot;
                result.Generation = appended.Generation;
                switch (appended.Status) {
                case EdlsSuccessorStatus::PublishedAndVerified:
                    result.Status = EdlsRollingStatus::PublishedInEmptySlot;
                    break;
                case EdlsSuccessorStatus::InvalidIdentityOrFrame:
                    result.Status = EdlsRollingStatus::InvalidIdentityOrFrame;
                    break;
                case EdlsSuccessorStatus::ArchiveAmbiguous:
                case EdlsSuccessorStatus::NoEmptySlot:
                    result.Status = EdlsRollingStatus::ArchiveAmbiguous;
                    break;
                case EdlsSuccessorStatus::SlotCapacityExceeded:
                    result.Status = EdlsRollingStatus::SlotCapacityExceeded;
                    break;
                case EdlsSuccessorStatus::ChangedDuringPreflight:
                    result.Status = EdlsRollingStatus::ChangedDuringPreflight;
                    break;
                case EdlsSuccessorStatus::ProviderRejected:
                    result.Status = EdlsRollingStatus::ProviderRejected;
                    break;
                case EdlsSuccessorStatus::ReadbackUnconfirmed:
                    result.Status = EdlsRollingStatus::ReadbackUnconfirmed;
                    break;
                }
                if (result.ProtectedNewestSlot != TSlots) {
                    result.PriorNewestVerified =
                        Detail::VerifyEdlsPredecessor<TPayloadMaximum>(
                            storage,
                            paths[result.ProtectedNewestSlot],
                            result.ProtectedNewestSlot,
                            current.Slots[result.ProtectedNewestSlot]) ==
                        Detail::EdlsPredecessorVerificationResult::Verified;
                    if (!result.PriorNewestVerified &&
                        (result.Status == EdlsRollingStatus::PublishedInEmptySlot ||
                         result.Status == EdlsRollingStatus::ReadbackUnconfirmed ||
                         result.Status == EdlsRollingStatus::ProviderRejected)) {
                        result.Status = EdlsRollingStatus::PriorNewestUnconfirmed;
                    }
                }
                return result;
            }
        }

        // Reclamation is authorised only for a COMPLETE clean topology.
        // No invalid tail/unknown FileStorage state is treated as an empty slot.
        std::size_t oldest = TSlots;
        for (std::size_t i = 0U; i < TSlots; ++i) {
            const auto &candidate = current.Slots[i];
            if (candidate.Condition != EdlsSlotCondition::Clean ||
                !candidate.HasHeader) {
                result.Status = EdlsRollingStatus::ArchiveAmbiguous;
                return result;
            }
            if (i == result.ProtectedNewestSlot) {
                continue;
            }
            if (oldest == TSlots ||
                candidate.Header.Generation <
                    current.Slots[oldest].Header.Generation) {
                oldest = i;
            }
        }
        if (oldest == TSlots) {
            result.Status = EdlsRollingStatus::NoReclaimableSlot;
            return result;
        }
        result.Slot = oldest;
        const auto measuredBefore = storage.GetFileSize(paths[oldest]);
        if (measuredBefore.Status != FileSizeStatus::Succeeded ||
            measuredBefore.Size.RawValue !=
                current.Slots[oldest].VerifiedPrefixBytes) {
            result.Status = EdlsRollingStatus::ChangedDuringPreflight;
            return result;
        }

        // The buffer is caller-frame-size bounded; no dynamic allocation,
        // no direct platform SDK and no source record lifetime is retained.
        std::array<std::byte, EdlsHeaderBytes + TPayloadMaximum + 20U> expected{};
        EdlsHeader header{};
        header.Slot = static_cast<std::uint16_t>(oldest);
        header.Generation = result.Generation;
        header.DeviceIdentifier = deviceIdentifier;
        header.RuntimeIncarnation = runtimeIncarnation;
        if (WriteEdlsHeader(header, expected) != EdlsHeaderStatus::Succeeded) {
            result.Status = EdlsRollingStatus::InvalidIdentityOrFrame;
            return result;
        }
        for (std::size_t i = 0U; i < completeEdlg.size(); ++i) {
            expected[EdlsHeaderBytes + i] = completeEdlg[i];
        }
        const auto used = EdlsHeaderBytes + completeEdlg.size();
        result.Native = storage.ReplaceFile(
            paths[oldest], SourceBufferView{expected.data(), used});

        // Independently re-read the protected prior newest even when native
        // replacement failed, so a single failure cannot masquerade as
        // successful cross-slot preservation evidence.
        result.PriorNewestVerified =
            Detail::VerifyEdlsPredecessor<TPayloadMaximum>(
                storage,
                paths[result.ProtectedNewestSlot],
                result.ProtectedNewestSlot,
                current.Slots[result.ProtectedNewestSlot]) ==
            Detail::EdlsPredecessorVerificationResult::Verified;
        if (!result.PriorNewestVerified) {
            result.Status = EdlsRollingStatus::PriorNewestUnconfirmed;
            return result;
        }
        if (result.Native != FileReplaceStatus::Succeeded) {
            result.Status = EdlsRollingStatus::ProviderRejected;
            return result;
        }

        const auto verification = InspectEdlsSlot<TPayloadMaximum>(
            storage, paths[oldest], static_cast<std::uint16_t>(oldest));
        const auto measuredAfter = storage.GetFileSize(paths[oldest]);
        if (verification.Condition != EdlsSlotCondition::Clean ||
            !verification.HasHeader ||
            verification.Header.Generation != result.Generation ||
            verification.Header.DeviceIdentifier != deviceIdentifier ||
            verification.Header.RuntimeIncarnation != runtimeIncarnation ||
            verification.VerifiedFrames != 1U ||
            verification.VerifiedPrefixBytes != used ||
            measuredAfter.Status != FileSizeStatus::Succeeded ||
            measuredAfter.Size.RawValue != used) {
            result.Status = EdlsRollingStatus::ReadbackUnconfirmed;
            return result;
        }

        // Byte-for-byte bounded readback catches an unexpectedly substituted
        // but internally checksum-valid representation, not just bad CRC.
        std::array<std::byte, 64U> observed{};
        for (std::size_t offset = 0U; offset < used;) {
            const auto chunk = used - offset > observed.size()
                                   ? observed.size()
                                   : used - offset;
            if (Detail::ReadExactEdlsBytes(
                    storage, paths[oldest], offset, std::span<std::byte>(observed.data(), chunk)) != Detail::EdlsExactReadResult::Succeeded) {
                result.Status = EdlsRollingStatus::ReadbackUnconfirmed;
                return result;
            }
            for (std::size_t i = 0U; i < chunk; ++i) {
                if (observed[i] != expected[offset + i]) {
                    result.Status = EdlsRollingStatus::ReadbackUnconfirmed;
                    return result;
                }
            }
            offset += chunk;
        }
        result.Status = EdlsRollingStatus::PublishedAfterOldestReclaim;
        return result;
    }

    /// Explicit result of immutable bounded rolling over TProtected+1 paths.
    /// No status grants generic FileStorage interruption atomicity.
    enum class EdlsProtectedRollingStatus : std::uint8_t {
        PublishedAndRetired = 0U,
        PublishedWithinCapacity = 1U,
        RecoveredPendingRetirement = 2U,
        NoPendingRetirement = 3U,
        RecoveryRequired = 4U,
        ArchiveAmbiguous = 5U,
        NoncontiguousHistory = 6U,
        IdentityMismatch = 7U,
        InvalidFrame = 8U,
        GenerationExhausted = 9U,
        NoPhysicalReserve = 10U,
        ProviderRejected = 11U,
        ReadbackUnconfirmed = 12U,
        ProtectedHistoryChanged = 13U,
        RetirementRejected = 14U,
        RetirementUnconfirmed = 15U
    };

    /// Full operation evidence, with native write/removal outcomes preserved.
    /// A partially completed operation may have committed a complete successor;
    /// the caller must NOT mistake a non-Published result for an empty slot.
    template <std::size_t TPhysicalSlots>
    struct EdlsProtectedRollingOutcome final {

        /// Result of this specific step (not blanket durability evidence).
        EdlsProtectedRollingStatus Status{
            EdlsProtectedRollingStatus::ArchiveAmbiguous};

        /// Existing FileStorage owner result, if a write was invoked.
        ESPressio::Persistence::FileReplaceStatus NativeWrite{
            ESPressio::Persistence::FileReplaceStatus::NotReady};

        /// Existing FileStorage owner result, if retirement was invoked.
        ESPressio::Persistence::FileRemoveStatus NativeRemove{
            ESPressio::Persistence::FileRemoveStatus::NotReady};

        /// Complete successor's physical slot or sentinel if none was selected.
        std::size_t PublishedSlot{TPhysicalSlots};

        /// Oldest eligible physical slot or sentinel if none was selected.
        std::size_t RetiredSlot{TPhysicalSlots};

        /// Successor generation when known, zero when no proposal exists.
        std::uint64_t Generation{0U};
    };

    namespace Detail {

        /// Strict read-only assessment of a finite protected EDLS history.
        /// One spare physical slot remains necessary for each new successor.
        /// Unknown/damaged/zero-byte existing files are NOT retired by guess.
        template <
            std::size_t TPhysicalSlots,
            std::size_t TProtectedSlots,
            std::size_t TPayloadMaximum,
            class TStorage>
        struct EdlsProtectedRollingPlan final {

            EdlsProtectedRollingStatus Status{
                EdlsProtectedRollingStatus::ArchiveAmbiguous};

            EdlsArchiveInspection<TPhysicalSlots> Inspected{};

            std::size_t Occupied{0U};

            std::size_t OldestSlot{TPhysicalSlots};

            std::uint64_t OldestGeneration{0U};

            std::uint64_t NewestGeneration{0U};
        };

        /// Verify unique contiguous histories, exact physical occupancy and
        /// stable cross-generation device identity before ANY mutation.
        template <
            std::size_t TPhysicalSlots,
            std::size_t TProtectedSlots,
            std::size_t TPayloadMaximum,
            class TStorage>
        [[nodiscard]] EdlsProtectedRollingPlan<
            TPhysicalSlots,
            TProtectedSlots,
            TPayloadMaximum,
            TStorage>
        InspectProtectedRollingPlanWithScratch(
            const TStorage &storage,
            const std::array<
                ESPressio::Persistence::FilePathView,
                TPhysicalSlots> &paths,
            std::span<std::byte> scratch) noexcept {
            static_assert(TProtectedSlots >= 2U);
            static_assert(TPhysicalSlots == TProtectedSlots + 1U);
            static_assert(TPhysicalSlots <= 64U);

            EdlsProtectedRollingPlan<
                TPhysicalSlots,
                TProtectedSlots,
                TPayloadMaximum,
                TStorage>
                result{};
            result.Inspected = InspectEdlsArchiveWithScratch<
                TPhysicalSlots,
                TPayloadMaximum>(storage, paths, scratch);

            if (result.Inspected.Condition ==
                EdlsArchiveCondition::GenerationExhausted) {
                result.Status = EdlsProtectedRollingStatus::GenerationExhausted;
                return result;
            }
            if (result.Inspected.Condition !=
                EdlsArchiveCondition::Unambiguous) {
                return result;
            }

            // Count only independently CRC-clean complete EDLS segments.
            // A damaged or unidentified candidate remains an explicit blocker.
            for (std::size_t slot = 0U; slot < TPhysicalSlots; ++slot) {
                const auto &current = result.Inspected.Slots[slot];
                if (current.Condition == EdlsSlotCondition::Empty) {
                    // The historical reader also marks zero-length files as
                    // Empty. Those are NOT proven never published; require
                    // physical absence for a reusable free name.
                    const auto measured = storage.GetFileSize(paths[slot]);
                    if (measured.Status !=
                        ESPressio::Persistence::FileSizeStatus::NotFound) {
                        return result;
                    }
                    continue;
                }
                if (current.Condition != EdlsSlotCondition::Clean ||
                    !current.HasHeader || current.VerifiedFrames == 0U) {
                    return result;
                }
                const auto generation = current.Header.Generation;
                if (result.Occupied == 0U ||
                    generation < result.OldestGeneration) {
                    result.OldestSlot = slot;
                    result.OldestGeneration = generation;
                }
                if (generation > result.NewestGeneration) {
                    result.NewestGeneration = generation;
                }
                ++result.Occupied;
            }
            if (result.Occupied > 0U &&
                result.NewestGeneration - result.OldestGeneration !=
                    result.Occupied - 1U) {
                result.Status = EdlsProtectedRollingStatus::NoncontiguousHistory;
                return result;
            }

            result.Status = EdlsProtectedRollingStatus::NoPendingRetirement;
            return result;
        }

        /// Backward-compatible local scratch path for the strict plan.
        template <
            std::size_t TPhysicalSlots,
            std::size_t TProtectedSlots,
            std::size_t TPayloadMaximum,
            class TStorage>
        [[nodiscard]] EdlsProtectedRollingPlan<
            TPhysicalSlots,
            TProtectedSlots,
            TPayloadMaximum,
            TStorage>
        InspectProtectedRollingPlan(
            const TStorage &storage,
            const std::array<
                ESPressio::Persistence::FilePathView,
                TPhysicalSlots> &paths) noexcept {
            std::array<std::byte, TPayloadMaximum + 20U> scratch{};
            return InspectProtectedRollingPlanWithScratch<
                TPhysicalSlots,
                TProtectedSlots,
                TPayloadMaximum>(storage, paths, scratch);
        }

        /// Reinspect each previously clean slot using shared caller scratch.
        /// Return the first verification failure in physical slot order.
        template <
            std::size_t TPhysicalSlots,
            std::size_t TPayloadMaximum,
            class TStorage>
        [[nodiscard]] EdlsPredecessorVerificationResult
        VerifyEdlsProtectedPredecessorsWithScratch(
            const TStorage &storage,
            const std::array<
                ESPressio::Persistence::FilePathView,
                TPhysicalSlots> &paths,
            const EdlsArchiveInspection<TPhysicalSlots> &before,
            std::span<std::byte> scratch) noexcept {

            for (std::size_t slot = 0U; slot < TPhysicalSlots; ++slot) {
                const auto &preceding = before.Slots[slot];
                if (preceding.Condition != EdlsSlotCondition::Clean) {
                    continue;
                }

                const auto result = VerifyEdlsPredecessorWithScratch<TPayloadMaximum>(
                    storage,
                    paths[slot],
                    slot,
                    preceding,
                    scratch);
                if (result != EdlsPredecessorVerificationResult::Verified) {
                    return result;
                }
            }

            return EdlsPredecessorVerificationResult::Verified;
        }

        /// Reinspect the protected set using one fixed frame buffer on the stack.
        template <
            std::size_t TPhysicalSlots,
            std::size_t TPayloadMaximum,
            class TStorage>
        [[nodiscard]] EdlsPredecessorVerificationResult
        VerifyEdlsProtectedPredecessors(
            const TStorage &storage,
            const std::array<
                ESPressio::Persistence::FilePathView,
                TPhysicalSlots> &paths,
            const EdlsArchiveInspection<TPhysicalSlots> &before) noexcept {

            std::array<std::byte, TPayloadMaximum + 20U> scratch{};
            return VerifyEdlsProtectedPredecessorsWithScratch<
                TPhysicalSlots,
                TPayloadMaximum>(
                storage,
                paths,
                before,
                scratch);
        }

    } // namespace Detail

    /// Finish retirement only after an independently complete N+1 archive
    /// can be re-read, leaving exactly the latest N acknowledged candidates.
    ///
    /// This operation does not need a new frame or an underlying SDK import;
    /// it is safe to repeat after a crash at the retirement boundary when
    /// all N+1 EDLS segment headers/frames remain complete and consistent.
    /// It NEVER removes invalid/unidentifiable/torn candidate files.
    ///
    /// Bootstrap owns exclusive FileStorage access during scan-to-retire.
    template <
        std::size_t TPhysicalSlots,
        std::size_t TProtectedSlots,
        std::size_t TPayloadMaximum,
        class TStorage>
    [[nodiscard]] EdlsProtectedRollingOutcome<TPhysicalSlots>
    FinishEdlsProtectedRollingRetirementWithScratch(
        TStorage &storage,
        const std::array<
            ESPressio::Persistence::FilePathView,
            TPhysicalSlots> &paths,
        std::span<std::byte> scratch) noexcept {
        using namespace ESPressio::Persistence;

        EdlsProtectedRollingOutcome<TPhysicalSlots> result{};
        const auto before = Detail::InspectProtectedRollingPlanWithScratch<
            TPhysicalSlots,
            TProtectedSlots,
            TPayloadMaximum>(storage, paths, scratch);
        result.Status = before.Status;
        if (before.Status !=
            EdlsProtectedRollingStatus::NoPendingRetirement) {
            return result;
        }
        if (before.Occupied <= TProtectedSlots) {
            return result;
        }
        // With exactly TProtected+1 CRC-clean contiguous generations,
        // the oldest is logically retired by the committed successor.
        // The immediately preceding full physical plan has JUST inspected
        // and CRC-qualified every protected newest file, under Bootstrap's
        // exclusive FileStorage writer claim. No FileStorage mutation occurs
        // between that inspection and the oldest-generation removal.
        // Re-reading the same newest N files here adds no independent
        // post-mutation evidence and needlessly multiplies Flash I/O.
        //
        // Preserve the mandatory complete post-removal physical CRC scan:
        // it independently validates every protected successor after the
        // provider's actual filesystem mutation and catches any damage.
        result.RetiredSlot = before.OldestSlot;
        result.Generation = before.NewestGeneration;
        result.NativeRemove = storage.RemoveFile(paths[before.OldestSlot]);
        if (result.NativeRemove != FileRemoveStatus::Succeeded) {
            result.Status = EdlsProtectedRollingStatus::RetirementRejected;
            return result;
        }
        // This complete post-delete inspection already independently reads
        // every retained segment and validates its EDLS header and every
        // EDLG frame CRC. Repeating the same full read with
        // VerifyEdlsProtectedPredecessors immediately afterwards cannot
        // strengthen a caller-serialized result: there is no intervening
        // storage mutation. Keep all pre-retirement CRC verification and
        // perform this *post*-retirement validation exactly once.
        const auto after = Detail::InspectProtectedRollingPlanWithScratch<
            TPhysicalSlots,
            TProtectedSlots,
            TPayloadMaximum>(storage, paths, scratch);
        if (after.Status != EdlsProtectedRollingStatus::NoPendingRetirement ||
            after.Occupied != TProtectedSlots ||
            after.NewestGeneration != before.NewestGeneration ||
            after.OldestGeneration != before.OldestGeneration + 1U) {
            result.Status = EdlsProtectedRollingStatus::RetirementUnconfirmed;
            return result;
        }
        result.Status = EdlsProtectedRollingStatus::RecoveredPendingRetirement;
        return result;
    }

    /// Legacy stack-backed entrypoint delegates to the SAME single actual
    /// retirement state machine; new high-payload workers choose WithScratch.
    template <std::size_t TPhysicalSlots, std::size_t TProtectedSlots, std::size_t TPayloadMaximum, class TStorage>
    [[nodiscard]] EdlsProtectedRollingOutcome<TPhysicalSlots>
    FinishEdlsProtectedRollingRetirement(
        TStorage &storage,
        const std::array<ESPressio::Persistence::FilePathView,
                         TPhysicalSlots> &paths) noexcept {
        std::array<std::byte, EdlsHeaderBytes + TPayloadMaximum + 20U> scratch{};
        return FinishEdlsProtectedRollingRetirementWithScratch<
            TPhysicalSlots,
            TProtectedSlots,
            TPayloadMaximum>(storage, paths, scratch);
    }

    /// Publish a real EDLS/EDLG successor into an UNUSED physical path,
    /// then retire only the oldest eligible path after independently verifying
    /// the prior protected history remains intact.
    ///
    /// TPhysicalSlots=TProtectedSlots+1 is deliberate: after every successful
    /// finite roll exactly one named physical successor reserve is empty.
    /// If the *underlying flash* lacks space for a new file, refuse without
    /// deleting the still-retained oldest segment; capacity must be budgeted
    /// separately by the application and its underlying provider.
    ///
    /// If interrupted after the successor commit but before retirement,
    /// FinishEdlsProtectedRollingRetirement can finish safely on a fresh boot
    /// if ALL N+1 files are independently clean/contiguous. A torn candidate
    /// remains ambiguous and MUST NOT be auto-erased by these routines.
    /// No claim of generic FileStorage PowerLoss atomicity is introduced.
    template <
        std::size_t TPhysicalSlots,
        std::size_t TProtectedSlots,
        std::size_t TPayloadMaximum,
        std::size_t TSlotMaximum,
        class TStorage>
    [[nodiscard]] EdlsProtectedRollingOutcome<TPhysicalSlots>
    PublishEdlsProtectedRollingSuccessorWithScratch(
        TStorage &storage,
        const std::array<
            ESPressio::Persistence::FilePathView,
            TPhysicalSlots> &paths,
        const std::array<std::byte, 16U> &deviceIdentifier,
        std::uint32_t runtimeIncarnation,
        std::span<const std::byte> completeEdlg,
        std::span<std::byte> scratch) noexcept {
        using namespace ESPressio::Persistence;
        static_assert(TPhysicalSlots == TProtectedSlots + 1U);
        static_assert(TProtectedSlots >= 2U && TPhysicalSlots <= 64U);
        static_assert(TPayloadMaximum >= 1U && TPayloadMaximum <= 4096U);
        static_assert(TSlotMaximum >= EdlsHeaderBytes + TPayloadMaximum + 20U);
        static_assert(ESPressio::Persistence::Detail::HasMandatoryFileStorageOperations<TStorage>);

        EdlsProtectedRollingOutcome<TPhysicalSlots> result{};
        const auto before = Detail::InspectProtectedRollingPlanWithScratch<
            TPhysicalSlots,
            TProtectedSlots,
            TPayloadMaximum>(storage, paths, scratch);
        result.Status = before.Status;
        if (before.Status !=
            EdlsProtectedRollingStatus::NoPendingRetirement) {
            return result;
        }
        if (before.Occupied == TPhysicalSlots) {
            result.Status = EdlsProtectedRollingStatus::RecoveryRequired;
            return result;
        }
        if (before.Occupied > 0U &&
            before.Inspected.Slots[before.Inspected.NewestSlot]
                    .Header.DeviceIdentifier != deviceIdentifier) {
            result.Status = EdlsProtectedRollingStatus::IdentityMismatch;
            return result;
        }

        // The existing successor owner handles exact EDLG framing, EDLS
        // header identity, new immutable filename and independent readback.
        const auto posted = PublishEdlsSuccessorIntoEmptySlotWithScratch<
            TPhysicalSlots,
            TPayloadMaximum,
            TSlotMaximum>(storage, paths, deviceIdentifier, runtimeIncarnation, completeEdlg, scratch);
        result.NativeWrite = posted.Native;
        result.PublishedSlot = posted.Slot;
        result.Generation = posted.Generation;
        switch (posted.Status) {
        case EdlsSuccessorStatus::PublishedAndVerified:
            break;
        case EdlsSuccessorStatus::InvalidIdentityOrFrame:
            result.Status = EdlsProtectedRollingStatus::InvalidFrame;
            return result;
        case EdlsSuccessorStatus::ArchiveAmbiguous:
        case EdlsSuccessorStatus::ChangedDuringPreflight:
            result.Status = EdlsProtectedRollingStatus::ArchiveAmbiguous;
            return result;
        case EdlsSuccessorStatus::NoEmptySlot:
            result.Status = EdlsProtectedRollingStatus::NoPhysicalReserve;
            return result;
        case EdlsSuccessorStatus::SlotCapacityExceeded:
            result.Status = EdlsProtectedRollingStatus::InvalidFrame;
            return result;
        case EdlsSuccessorStatus::ProviderRejected:
            result.Status = EdlsProtectedRollingStatus::ProviderRejected;
            return result;
        case EdlsSuccessorStatus::ReadbackUnconfirmed:
            result.Status = EdlsProtectedRollingStatus::ReadbackUnconfirmed;
            return result;
        }
        // Before retirement, recheck protected header identity, frame CRCs,
        // frame count and verified prefix length even after a clean successor.
        if (Detail::VerifyEdlsProtectedPredecessorsWithScratch<
                TPhysicalSlots,
                TPayloadMaximum>(
                storage,
                paths,
                before.Inspected,
                scratch) != Detail::EdlsPredecessorVerificationResult::Verified) {
            result.Status = EdlsProtectedRollingStatus::ProtectedHistoryChanged;
            return result;
        }
        if (before.Occupied < TProtectedSlots) {
            result.Status = EdlsProtectedRollingStatus::PublishedWithinCapacity;
            return result;
        }

        const auto retired = FinishEdlsProtectedRollingRetirementWithScratch<
            TPhysicalSlots,
            TProtectedSlots,
            TPayloadMaximum>(storage, paths, scratch);
        result.RetiredSlot = retired.RetiredSlot;
        result.NativeRemove = retired.NativeRemove;
        result.Status = retired.Status ==
                                EdlsProtectedRollingStatus::RecoveredPendingRetirement
                            ? EdlsProtectedRollingStatus::PublishedAndRetired
                            : retired.Status;
        return result;
    }

    /// Legacy API intentionally remains callable and regression-compatible.
    /// This stack-backed path is not recommended at TPayloadMaximum=4096 on
    /// the ESP32 main task; use the caller-owned WithScratch overload.
    template <std::size_t TPhysicalSlots, std::size_t TProtectedSlots, std::size_t TPayloadMaximum, std::size_t TSlotMaximum, class TStorage>
    [[nodiscard]] EdlsProtectedRollingOutcome<TPhysicalSlots>
    PublishEdlsProtectedRollingSuccessor(
        TStorage &storage,
        const std::array<ESPressio::Persistence::FilePathView,
                         TPhysicalSlots> &paths,
        const std::array<std::byte, 16U> &deviceIdentifier,
        std::uint32_t runtimeIncarnation,
        std::span<const std::byte> completeEdlg) noexcept {
        std::array<std::byte, EdlsHeaderBytes + TPayloadMaximum + 20U> scratch{};
        return PublishEdlsProtectedRollingSuccessorWithScratch<
            TPhysicalSlots,
            TProtectedSlots,
            TPayloadMaximum,
            TSlotMaximum>(storage, paths, deviceIdentifier, runtimeIncarnation, completeEdlg, scratch);
    }

} // namespace ESPressio::Logging
