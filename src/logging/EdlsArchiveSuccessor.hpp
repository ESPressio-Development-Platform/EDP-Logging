#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <ESPressio_Persistence.hpp>

#include "EdlgFrameReader.hpp"
#include "EdlsArchiveInspection.hpp"

namespace ESPressio::Logging {

    /// Result of introducing a new EDLS generation into a PHYSICALLY EMPTY
    /// slot, never destroying prior history to make space.
    enum class EdlsSuccessorStatus : std::uint8_t {
        PublishedAndVerified,
        InvalidIdentityOrFrame,
        ArchiveAmbiguous,
        NoEmptySlot,
        SlotCapacityExceeded,
        ChangedDuringPreflight,
        ProviderRejected,
        ReadbackUnconfirmed
    };

    /// Exact provider mutation and independent readback observation.
    template<std::size_t TSlots>
    struct EdlsSuccessorOutcome final {
        /// Logical source-side publication state, not media durability.
        EdlsSuccessorStatus Status{EdlsSuccessorStatus::ArchiveAmbiguous};

        /// Native FileStorage ReplaceFile result, if invoked.
        ESPressio::Persistence::FileReplaceStatus Native{
            ESPressio::Persistence::FileReplaceStatus::NotReady
        };

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
    template<
        std::size_t TSlots,
        std::size_t TPayloadMaximum,
        std::size_t TSlotMaximum,
        class TStorage
    >
    [[nodiscard]] EdlsSuccessorOutcome<TSlots> PublishEdlsSuccessorIntoEmptySlotWithScratch(
        TStorage& storage,
        const std::array<ESPressio::Persistence::FilePathView,TSlots>& paths,
        const std::array<std::byte,16U>& deviceIdentifier,
        std::uint32_t runtimeIncarnation,
        std::span<const std::byte> completeEdlg,
        std::span<std::byte> callerScratch,
        const EdlsArchiveInspection<TSlots>* alreadyVerifiedInThisTransaction = nullptr
    ) noexcept {
        using namespace ESPressio::Persistence;
        static_assert(TSlots>=2U && TSlots<=64U);
        static_assert(TPayloadMaximum>=1U && TPayloadMaximum<=4096U);
        static_assert(TSlotMaximum>=EdlsHeaderBytes+20U);
        static_assert(ESPressio::Persistence::Detail::HasMandatoryFileStorageOperations<TStorage>);
        EdlsSuccessorOutcome<TSlots> outcome{};
        EdlsHeader header{};
        header.DeviceIdentifier=deviceIdentifier;
        header.RuntimeIncarnation=runtimeIncarnation;
        if (!Detail::ValidEdlsIdentity(EdlsHeader{
                0U, 1U, deviceIdentifier, runtimeIncarnation
            }) ||
            completeEdlg.size()>TPayloadMaximum+20U) {
            outcome.Status=EdlsSuccessorStatus::InvalidIdentityOrFrame;
            return outcome;
        }
        EdlgFrameView parsed{};
        if (ReadEdlgFrame(
                completeEdlg,parsed,TPayloadMaximum
            )!=EdlgFrameReadResult::Succeeded ||
            parsed.Consumed!=completeEdlg.size()) {
            outcome.Status=EdlsSuccessorStatus::InvalidIdentityOrFrame;
            return outcome;
        }
        if (completeEdlg.size()>TSlotMaximum-EdlsHeaderBytes) {
            outcome.Status=EdlsSuccessorStatus::SlotCapacityExceeded;
            return outcome;
        }
        if (callerScratch.size()<EdlsHeaderBytes+TPayloadMaximum+20U) {
            outcome.Status=EdlsSuccessorStatus::SlotCapacityExceeded;
            return outcome;
        }
        // An exclusive owner may reuse the archive's already CRC-qualified
        // immutable snapshot from an immediately preceding full physical
        // scan in THIS SAME transaction. No cross-publication cache is used;
        // physical vacancy is independently rechecked below, and the newly
        // written segment is fully CRC-read back before returning success.
        // Public default remains the original fresh whole-archive scan.
        const auto current=alreadyVerifiedInThisTransaction != nullptr
            ? *alreadyVerifiedInThisTransaction
            : InspectEdlsArchiveWithScratch<TSlots,TPayloadMaximum>(
                storage,paths,callerScratch);

        if (current.Condition!=EdlsArchiveCondition::Unambiguous ||
            current.NextGeneration==0U) {
            outcome.Status=EdlsSuccessorStatus::ArchiveAmbiguous;
            return outcome;
        }
        if (current.NewestSlot!=TSlots &&
            current.Slots[current.NewestSlot].Header.DeviceIdentifier!=
                deviceIdentifier) {
            outcome.Status=EdlsSuccessorStatus::InvalidIdentityOrFrame;
            return outcome;
        }
        // Prefer next physical slot after newest, but NEVER replace a
        // populated generation regardless of cyclic position.
        const auto first=current.NewestSlot==TSlots
            ? 0U : (current.NewestSlot+1U)%TSlots;
        for (std::size_t advance=0U;advance<TSlots;++advance) {
            const auto candidate=(first+advance)%TSlots;
            if (current.Slots[candidate].Condition==EdlsSlotCondition::Empty) {
                outcome.Slot=candidate;
                break;
            }
        }
        if (outcome.Slot==TSlots) {
            outcome.Status=EdlsSuccessorStatus::NoEmptySlot;
            return outcome;
        }
        outcome.Generation=current.NextGeneration;
        header.Slot=static_cast<std::uint16_t>(outcome.Slot);
        header.Generation=outcome.Generation;
        // A separate size check catches obvious mutation between the full
        // inspection and the ReplaceFile call. It does NOT substitute for the
        // application's exclusive writer lock.
        const auto latest=storage.GetFileSize(paths[outcome.Slot]);
        if (latest.Status!=FileSizeStatus::NotFound &&
            !(latest.Status==FileSizeStatus::Succeeded &&
              latest.Size.RawValue==0U)) {
            outcome.Status=EdlsSuccessorStatus::ChangedDuringPreflight;
            return outcome;
        }
        // The checked archive scan is complete before this scratch is
        // reused for immutable successor assembly; there is NO live borrow.
        auto buffer=callerScratch.first(EdlsHeaderBytes+TPayloadMaximum+20U);
        if (WriteEdlsHeader(header,buffer)!=EdlsHeaderStatus::Succeeded) {
            outcome.Status=EdlsSuccessorStatus::InvalidIdentityOrFrame;
            return outcome;
        }
        for (std::size_t i=0U;i<completeEdlg.size();++i) {
            buffer[EdlsHeaderBytes+i]=completeEdlg[i];
        }
        const auto used=EdlsHeaderBytes+completeEdlg.size();
        outcome.Native=storage.ReplaceFile(
            paths[outcome.Slot],SourceBufferView{buffer.data(),used}
        );
        if (outcome.Native!=FileReplaceStatus::Succeeded) {
            outcome.Status=EdlsSuccessorStatus::ProviderRejected;
            return outcome;
        }
        const auto verification=InspectEdlsSlotWithScratch<TPayloadMaximum>(
            storage,paths[outcome.Slot],header.Slot,callerScratch
        );
        const auto measured=storage.GetFileSize(paths[outcome.Slot]);
        if (verification.Condition!=EdlsSlotCondition::Clean ||
            !verification.HasHeader ||
            verification.Header.Generation!=header.Generation ||
            verification.Header.DeviceIdentifier!=header.DeviceIdentifier ||
            verification.Header.RuntimeIncarnation!=header.RuntimeIncarnation ||
            verification.VerifiedFrames!=1U ||
            verification.VerifiedPrefixBytes!=used ||
            measured.Status!=FileSizeStatus::Succeeded ||
            measured.Size.RawValue!=used) {
            outcome.Status=EdlsSuccessorStatus::ReadbackUnconfirmed;
            return outcome;
        }
        outcome.Status=EdlsSuccessorStatus::PublishedAndVerified;
        return outcome;
    }

    /// Backward-compatible local scratch path. High-payload native workers
    /// SHOULD provision stable Bootstrap-owned scratch via WithScratch to
    /// avoid nested >8KB stack frames under archive scan and publication.
    template<std::size_t TSlots,std::size_t TPayloadMaximum,
             std::size_t TSlotMaximum,class TStorage>
    [[nodiscard]] EdlsSuccessorOutcome<TSlots> PublishEdlsSuccessorIntoEmptySlot(
        TStorage& storage,
        const std::array<ESPressio::Persistence::FilePathView,TSlots>& paths,
        const std::array<std::byte,16U>& deviceIdentifier,
        std::uint32_t runtimeIncarnation,
        std::span<const std::byte> completeEdlg
    ) noexcept {
        static_assert(TPayloadMaximum>0U && TPayloadMaximum<=4096U);
        std::array<std::byte,EdlsHeaderBytes+TPayloadMaximum+20U> scratch{};
        return PublishEdlsSuccessorIntoEmptySlotWithScratch<
            TSlots,TPayloadMaximum,TSlotMaximum
        >(storage,paths,deviceIdentifier,runtimeIncarnation,completeEdlg,
          scratch);
    }

} // ESPressio::Logging
