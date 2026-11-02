#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "PowerLossEdlsSnapshots.hpp"

namespace ESPressio::Logging {

    /// Exact read-only eligibility and retained generation window of a
    /// PowerLoss-atomic key/value EDLS history.
    struct DurableEdlsHistoryObservation final {
        /// Complete classification. Only Clean or Empty permits appends.
        DurableEdlsStatus Status{DurableEdlsStatus::ProviderFailure};

        /// Oldest generation still protected by configured retention.
        std::uint64_t OldestRetained{0U};

        /// Newest committed generation, including any valid unacknowledged
        /// in-flight transaction that completed before interruption.
        std::uint64_t Newest{0U};

        /// Number of still-retained CRC-valid EDLG records.
        std::size_t RetainedCount{0U};
    };

    /// Bounded, N-record historical journal constructed from the established
    /// PowerLossEdlsSnapshots owner. N+1 independent atomic keys maintain
    /// one sacrificial slot during the next append.
    ///
    /// This adapts the snapshot owner's existing EDLS encoding, bounded key
    /// inspection, CRC parsing, immutable copy, generation and native
    /// StoreValue+readback flow. No redundant storage/CRC worker is introduced.
    ///
    /// A new completed native key commit advances the logical retention window,
    /// even if electrical interruption occurs before the application emits
    /// its acknowledgement. The oldest generation then becomes retired;
    /// recovery may legitimately return an unacknowledged newer generation.
    /// No promise is made to preserve records outside the N = TSlots-1
    /// most recent successfully committed generations.
    ///
    /// App Bootstrap exclusively owns/serializes all key invocations and must
    /// supply TSlots unique valid key names from a single PowerLoss-qualified
    /// EDP-Persistence::KeyValueStorage provider. TSlots in 2..8, one
    /// complete checked EDLG frame per <=512-byte NVS value.
    ///
    /// @tparam TSlots Number of physical NVS keys, one more than retained frames.
    /// @tparam TMaximumValueBytes Maximum bytes per EDLS key.
    /// @tparam TMaximumFramePayloadBytes Maximum EDLG payload bytes.
    /// @tparam TStorage Bootstrap-selected EDP-Persistence KeyValueStorage provider.
    template <
        std::size_t TSlots,
        std::size_t TMaximumValueBytes,
        std::size_t TMaximumFramePayloadBytes,
        class TStorage>
    class PowerLossEdlsJournal final {
      private:
        /// Reuse of the established independently checked snapshot owner.
        PowerLossEdlsSnapshots<
            TSlots,
            TMaximumValueBytes,
            TMaximumFramePayloadBytes,
            TStorage>
            snapshots_;

      public:
        /// Selects application-owned independent PowerLoss-atomic key slots.
        explicit PowerLossEdlsJournal(
            TStorage &storage,
            const std::array<ESPressio::Persistence::KeyView, TSlots> &keys) noexcept : snapshots_(storage, keys) {}

        /// Inspects the bounded retention window without mutating storage.
        /// Discontinuous generations or malformed physical ordering fail
        /// closed, rather than silently retiring a supposedly protected key.
        [[nodiscard]] DurableEdlsHistoryObservation Inspect() const noexcept {
            const auto physical = snapshots_.Inspect();
            DurableEdlsHistoryObservation result{};
            result.Status = physical.Status;

            if (physical.Status == DurableEdlsStatus::Empty) {
                return result;
            }
            // A damaged sacrificial key may be ignored for *read-only*
            // recovery of the independently verified retained window.
            // The degraded state still prevents any subsequent append.
            if (physical.Status != DurableEdlsStatus::Clean &&
                physical.Status != DurableEdlsStatus::Degraded) {
                return result;
            }
            if (physical.Generation == 0U || physical.NewestSlot >= TSlots) {
                result.Status = DurableEdlsStatus::Ambiguous;
                return result;
            }

            const auto capacity = TSlots - 1U;
            result.RetainedCount =
                physical.Generation < static_cast<std::uint64_t>(capacity)
                    ? static_cast<std::size_t>(physical.Generation)
                    : capacity;
            result.Newest = physical.Generation;
            result.OldestRetained =
                physical.Generation - static_cast<std::uint64_t>(result.RetainedCount) + 1U;

            // Every still-retained generation must have exactly one complete
            // EDLG/EDLS key and a matching canonical physical slot position.
            for (std::size_t age = 0U; age < result.RetainedCount; ++age) {
                const auto generation =
                    physical.Generation - static_cast<std::uint64_t>(age);
                const auto expectedSlot =
                    (physical.NewestSlot + TSlots - age) % TSlots;
                if (physical.SlotStatus[expectedSlot] != DurableEdlsStatus::Clean ||
                    physical.SlotGenerations[expectedSlot] != generation) {
                    result.Status = DurableEdlsStatus::Ambiguous;
                    result.RetainedCount = 0U;
                    result.OldestRetained = 0U;
                    return result;
                }
            }

            // With every physical slot populated, the slot immediately
            // following newest MUST be the single unretained sacrificial key.
            // In particular the snapshot engine may not overwrite a retained
            // key even if a pathological valid-CRC slot order is presented.
            bool allFull = true;
            for (std::size_t index = 0U; index < TSlots; ++index) {
                if (physical.SlotStatus[index] != DurableEdlsStatus::Clean) {
                    allFull = false;
                    break;
                }
            }
            if (allFull) {
                const auto sacrificial = (physical.NewestSlot + 1U) % TSlots;
                if (physical.SlotGenerations[sacrificial] >=
                    result.OldestRetained) {
                    result.Status = DurableEdlsStatus::Ambiguous;
                    result.RetainedCount = 0U;
                    result.OldestRetained = 0U;
                }
            }
            return result;
        }

        /// Appends one complete CRC-valid EDLG frame while retaining all
        /// existing protected generations throughout the inactive-key write.
        /// Only a fully completed StoreValue AND exact independent readback
        /// can return Committed; interruption may introduce a complete new
        /// generation before its caller observes acknowledgement.
        [[nodiscard]] DurableEdlsCommitOutcome<TSlots> Append(
            const std::array<std::byte, 16U> &identity,
            std::uint32_t incarnation,
            std::span<const std::byte> frame) noexcept {
            const auto before = Inspect();
            if (before.Status != DurableEdlsStatus::Clean &&
                before.Status != DurableEdlsStatus::Empty) {
                return {before.Status};
            }
            const auto outcome = snapshots_.Commit(
                identity,
                incarnation,
                frame);
            if (outcome.Status != DurableEdlsStatus::Committed) {
                return outcome;
            }
            const auto after = Inspect();
            if (after.Status != DurableEdlsStatus::Clean ||
                after.Newest != outcome.Generation ||
                (before.Status == DurableEdlsStatus::Clean &&
                 after.Newest != before.Newest + 1U)) {
                auto uncertain = outcome;
                uncertain.Status = DurableEdlsStatus::ReadbackUnconfirmed;
                return uncertain;
            }
            return outcome;
        }

        /// Replays a particular generation ONLY when it remains inside the
        /// protected retention window. Caller receives no frame bytes unless
        /// the underlying snapshot owner rechecks EDLS and EDLG checksums.
        [[nodiscard]] DurableEdlsFrameReadOutcome RecoverRetained(
            std::uint64_t generation,
            std::span<std::byte> destination) const noexcept {
            const auto history = Inspect();
            if (history.Status != DurableEdlsStatus::Clean &&
                history.Status != DurableEdlsStatus::Degraded) {
                return {history.Status, 0U};
            }
            if (generation < history.OldestRetained ||
                generation > history.Newest) {
                return {DurableEdlsStatus::InvalidRecord, 0U};
            }
            return snapshots_.RecoverGeneration(
                generation,
                destination);
        }
    };

} // namespace ESPressio::Logging
