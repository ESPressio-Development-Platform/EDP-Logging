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
    template<std::size_t TSlots>
    struct EdlsRollingOutcome final {

        /// Precisely qualified outcome; never implies atomicity/durability.
        EdlsRollingStatus Status{EdlsRollingStatus::ArchiveAmbiguous};

        /// Native EDP-Persistence ReplaceFile outcome, if called.
        ESPressio::Persistence::FileReplaceStatus Native{
            ESPressio::Persistence::FileReplaceStatus::NotReady
        };

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

        /// Revisit the preexisting newest clean generation after replacement,
        /// including its CRC-validated full prefix; no mutation is performed.
        template<std::size_t TPayloadMaximum, class TStorage>
        [[nodiscard]] bool PriorNewestEdlsStillClean(
            const TStorage& storage,
            ESPressio::Persistence::FilePathView path,
            std::size_t physicalSlot,
            const EdlsSlotInspection& before
        ) noexcept {
            if (before.Condition != EdlsSlotCondition::Clean ||
                !before.HasHeader) {
                return false;
            }
            const auto after = InspectEdlsSlot<TPayloadMaximum>(
                storage, path, static_cast<std::uint16_t>(physicalSlot)
            );
            return after.Condition == EdlsSlotCondition::Clean &&
                   after.HasHeader &&
                   after.Header.Generation == before.Header.Generation &&
                   after.Header.DeviceIdentifier ==
                       before.Header.DeviceIdentifier &&
                   after.Header.RuntimeIncarnation ==
                       before.Header.RuntimeIncarnation &&
                   after.VerifiedFrames == before.VerifiedFrames &&
                   after.VerifiedPrefixBytes == before.VerifiedPrefixBytes;
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
    template<
        std::size_t TSlots,
        std::size_t TPayloadMaximum,
        std::size_t TSlotMaximum,
        class TStorage
    >
    [[nodiscard]] EdlsRollingOutcome<TSlots> PublishEdlsOldestEligibleSuccessor(
        TStorage& storage,
        const std::array<ESPressio::Persistence::FilePathView,TSlots>& paths,
        const std::array<std::byte,16U>& deviceIdentifier,
        std::uint32_t runtimeIncarnation,
        std::span<const std::byte> completeEdlg
    ) noexcept {
        using namespace ESPressio::Persistence;

        static_assert(TSlots >= 2U && TSlots <= 64U);
        static_assert(TPayloadMaximum >= 1U && TPayloadMaximum <= 4096U);
        static_assert(TSlotMaximum >= EdlsHeaderBytes + 20U);
        static_assert(ESPressio::Persistence::Detail::HasMandatoryFileStorageOperations<TStorage>);

        EdlsRollingOutcome<TSlots> result{};
        const auto current = InspectEdlsArchive<TSlots,TPayloadMaximum>(
            storage, paths
        );
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
        if (!Detail::ValidEdlsIdentity(EdlsHeader{
                0U, 1U, deviceIdentifier, runtimeIncarnation
            }) ||
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
                    TSlots,TPayloadMaximum,TSlotMaximum
                >(storage,paths,deviceIdentifier,runtimeIncarnation,completeEdlg);
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
                        Detail::PriorNewestEdlsStillClean<TPayloadMaximum>(
                            storage,
                            paths[result.ProtectedNewestSlot],
                            result.ProtectedNewestSlot,
                            current.Slots[result.ProtectedNewestSlot]
                        );
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
            const auto& candidate = current.Slots[i];
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
        std::array<std::byte,EdlsHeaderBytes+TPayloadMaximum+20U> expected{};
        EdlsHeader header{};
        header.Slot = static_cast<std::uint16_t>(oldest);
        header.Generation = result.Generation;
        header.DeviceIdentifier = deviceIdentifier;
        header.RuntimeIncarnation = runtimeIncarnation;
        if (WriteEdlsHeader(header,expected) != EdlsHeaderStatus::Succeeded) {
            result.Status = EdlsRollingStatus::InvalidIdentityOrFrame;
            return result;
        }
        for (std::size_t i = 0U; i < completeEdlg.size(); ++i) {
            expected[EdlsHeaderBytes+i] = completeEdlg[i];
        }
        const auto used = EdlsHeaderBytes + completeEdlg.size();
        result.Native = storage.ReplaceFile(
            paths[oldest], SourceBufferView{expected.data(),used}
        );

        // Independently re-read the protected prior newest even when native
        // replacement failed, so a single failure cannot masquerade as
        // successful cross-slot preservation evidence.
        result.PriorNewestVerified =
            Detail::PriorNewestEdlsStillClean<TPayloadMaximum>(
                storage,
                paths[result.ProtectedNewestSlot],
                result.ProtectedNewestSlot,
                current.Slots[result.ProtectedNewestSlot]
            );
        if (!result.PriorNewestVerified) {
            result.Status = EdlsRollingStatus::PriorNewestUnconfirmed;
            return result;
        }
        if (result.Native != FileReplaceStatus::Succeeded) {
            result.Status = EdlsRollingStatus::ProviderRejected;
            return result;
        }

        const auto verification = InspectEdlsSlot<TPayloadMaximum>(
            storage,paths[oldest],static_cast<std::uint16_t>(oldest)
        );
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
        std::array<std::byte,64U> observed{};
        for (std::size_t offset = 0U; offset < used;) {
            const auto chunk = used - offset > observed.size()
                ? observed.size() : used - offset;
            if (!Detail::ReadExactEdlsBytes(
                storage, paths[oldest], offset,
                std::span<std::byte>(observed.data(),chunk)
            )) {
                result.Status = EdlsRollingStatus::ReadbackUnconfirmed;
                return result;
            }
            for (std::size_t i = 0U; i < chunk; ++i) {
                if (observed[i] != expected[offset+i]) {
                    result.Status = EdlsRollingStatus::ReadbackUnconfirmed;
                    return result;
                }
            }
            offset += chunk;
        }
        result.Status = EdlsRollingStatus::PublishedAfterOldestReclaim;
        return result;
    }

} // namespace ESPressio::Logging
