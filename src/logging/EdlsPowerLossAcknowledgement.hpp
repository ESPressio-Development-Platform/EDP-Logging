#pragma once

#include "EdlsArchiveRotation.hpp"
#include <ESPressio_Persistence.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace ESPressio::Logging {

    /// This API is an opt-in acknowledgement contract for an archive writer.
    /// An acknowledged EDLS generation is a complete verified segment AND
    /// a separately PowerLoss-atomic committed and reread watermark. The
    /// writer may not return success to a producer before BOTH facts hold.
    enum class EdlsAcknowledgedStatus : std::uint8_t {
        Ready = 0,
        Initialized = 1,
        MissingWatermark = 2,
        InvalidWatermark = 3,
        ArchiveAmbiguous = 4,
        AcknowledgedHistoryChanged = 5,
        UnacknowledgedCandidateRemoved = 6,
        CommittedOldestRetired = 7,
        NoPhysicalReserve = 8,
        GenerationExhausted = 9,
        ProviderFailure = 10,
        WatermarkStoreRejected = 11,
        PublishedNotAcknowledged = 12,
        AcknowledgedWithinCapacity = 13,
        AcknowledgedAndRetired = 14,
        AcknowledgedPendingRetirement = 15,
        InvalidByteCapacity = 16,
        ByteCapacityExceeded = 17,
    };

    /// Observed watermark generation and candidate location for one archive operation.
    struct EdlsAcknowledgedObservation final {
        /// Outcome of the read, recovery or publication attempt.
        EdlsAcknowledgedStatus Status{
            EdlsAcknowledgedStatus::InvalidWatermark};
        /// Last acknowledged segment generation when known. Zero represents genesis.
        std::uint64_t AcknowledgedGeneration{0U};
        /// Physical slot implicated by a candidate or retirement result.
        std::size_t CandidateSlot{0U};
        /// Runtime incarnation of the LAST acknowledged source, not this boot.
        std::uint32_t AcknowledgedIncarnation{0U};
    };

    /// Bootstrap provides one exclusive FileStorage owner and one independent
    /// PowerLoss-atomic KeyValueStorage owner. The EDLS v1 40-byte CRC header
    /// serves as the existing wire representation of a watermarked generation:
    /// key Header.Generation = latest acknowledged generation + 1, including
    /// genesis (=1, so last acknowledged=0). Header.Slot is reserved as 0.
    /// No private codec or second logging archive is introduced.
    ///
    /// The watermark's RuntimeIncarnation records the LAST ACKed segment's
    /// incarnation. On a new boot the caller-supplied runtime incarnation MAY
    /// and normally MUST advance; retained generations may span incarnations.
    /// Strict guarantees apply only to writers that use this exact ordering;
    /// this API MUST NOT be adopted to infer provenance for legacy EDLS files
    /// that were published/acknowledged without this companion watermark.
    /// @tparam TPhysicalSlots Total number of immutable segment paths, including one successor reserve.
    /// @tparam TProtectedSlots Maximum retained acknowledged generations in the steady state.
    /// @tparam TPayloadMaximum Maximum encoded payload bytes per EDLG frame.
    /// @tparam TSegmentMaximum Upper bound on bytes per immutable EDLS segment.
    /// @tparam TFileStorage Exclusive caller-owned FileStorage provider for segment paths.
    /// @tparam TKeyValueStorage Independent PowerLoss-atomic read/write provider for the watermark.
    template <
        std::size_t TPhysicalSlots,
        std::size_t TProtectedSlots,
        std::size_t TPayloadMaximum,
        std::size_t TSegmentMaximum,
        class TFileStorage,
        class TKeyValueStorage>
    class PowerLossAcknowledgedEdls final {

        // Composition contract for the independent acknowledgement provider.
        /// KeyValueStorage capability selected by Bootstrap.
        using Capability = ESPressio::Persistence::KeyValueStorage;
        /// Declared retention, interruption atomicity, access mode and size limits.
        using Properties = typename TKeyValueStorage::CompositionOffers ::template PropertiesFor<Capability>;
        static_assert(TPhysicalSlots == TProtectedSlots + 1U);
        static_assert(TProtectedSlots >= 2U && TPhysicalSlots <= 64U);
        static_assert(TPayloadMaximum >= 1U && TPayloadMaximum <= 4096U);
        static_assert(TSegmentMaximum >= EdlsHeaderBytes + TPayloadMaximum + 20U);
        static_assert(ESPressio::Persistence::KeyValueStorageProvider<
                      TKeyValueStorage>);
        static_assert(ESPressio::Persistence::Detail::
                          HasMandatoryFileStorageOperations<TFileStorage>);
        static_assert(Properties::template Value<
                          ESPressio::Persistence::KeyValueRetention> ==
                      ESPressio::Persistence::RetentionLevel::PowerLoss);
        static_assert(Properties::template Value<
                          ESPressio::Persistence::KeyValueInterruptionAtomicity> ==
                      ESPressio::Persistence::InterruptionAtomicity::PowerLoss);
        static_assert(Properties::template Value<
                          ESPressio::Persistence::KeyValueAccessMode> ==
                      ESPressio::Persistence::AccessMode::ReadWrite);
        static_assert(Properties::template Value<
                          ESPressio::Persistence::MaximumKeyValueSize> >=
                      ESPressio::Persistence::StorageSize{EdlsHeaderBytes});

        // Immutable application storage budget.
        /// Logical bytes owned by this sink, including the 40-byte ACK value.
        /// Filesystem allocation overhead and provider-internal metadata are
        /// outside this interface; Bootstrap budgets their physical reserve.
        const std::uint64_t byteCapacity_;

        /// Equal segment reservations guarantee room for a successor without
        /// retiring protected records first, even with variable-sized batches.
        [[nodiscard]] std::uint64_t SegmentByteCapacity() const noexcept {
            if (byteCapacity_ < EdlsHeaderBytes)
                return 0U;
            const auto share = (byteCapacity_ - EdlsHeaderBytes) / TPhysicalSlots;
            return share < TSegmentMaximum ? share : TSegmentMaximum;
        }

        /// A single nonempty EDLG frame needs 20 framing bytes plus one payload byte.
        [[nodiscard]] bool IsByteCapacityValid() const noexcept {
            return SegmentByteCapacity() >= EdlsHeaderBytes + 21U;
        }

        // Borrowed storage providers and names.
        /// Exclusive provider used to publish, read and retire EDLS segment files.
        TFileStorage *files_;
        /// Independent provider used to commit and read back the acknowledgement.
        TKeyValueStorage *watermark_;
        /// Copied path views whose backing storage must outlive this owner.
        std::array<ESPressio::Persistence::FilePathView, TPhysicalSlots> paths_;
        /// Copied key view whose backing storage must outlive this owner.
        ESPressio::Persistence::KeyView watermarkKey_;

        // Source identity persisted in new segment and watermark headers.
        /// Stable device identity used to validate the protected history.
        std::array<std::byte, 16U> identity_;
        /// Runtime incarnation assigned by Bootstrap for this writer.
        std::uint32_t incarnation_;
        // A volatile fast-path hint, NEVER a durability acknowledgement.
        // Valid ONLY after this exclusive writer completed all physical
        // successor verification, NVS commit/readback and retirement.
        // Boot, explicit recovery or ANY failed publication invalidates it.
        /// Whether the last transaction completed every required verification step.
        bool exclusiveWriterCompleted_{false};
        /// Acknowledged generation paired with the valid in-memory writer hint.
        std::uint64_t exclusiveWriterAck_{0U};

        /// Stores and rereads the watermark, confirming generation and incarnation.
        [[nodiscard]] EdlsAcknowledgedObservation StoreWatermark(
            std::uint64_t latest) noexcept {
            namespace P = ESPressio::Persistence;
            if (latest == std::numeric_limits<std::uint64_t>::max() ||
                incarnation_ == 0U)
                return {EdlsAcknowledgedStatus::GenerationExhausted};
            EdlsHeader encoded{};
            encoded.Slot = 0U;
            encoded.Generation = latest + 1U;
            encoded.DeviceIdentifier = identity_;
            encoded.RuntimeIncarnation = incarnation_;
            std::array<std::byte, EdlsHeaderBytes> bytes{};
            if (WriteEdlsHeader(
                    encoded,
                    bytes) != EdlsHeaderStatus::Succeeded)
                return {EdlsAcknowledgedStatus::InvalidWatermark};
            if (watermark_->StoreValue(
                    watermarkKey_,
                    P::SourceBufferView{bytes.data(), bytes.size()}) !=
                P::KeyValueStoreStatus::Succeeded)
                return {EdlsAcknowledgedStatus::WatermarkStoreRejected};
            const auto checked = Read();
            return checked.Status == EdlsAcknowledgedStatus::Ready &&
                           checked.AcknowledgedGeneration == latest &&
                           checked.AcknowledgedIncarnation == incarnation_
                       ? checked
                       : EdlsAcknowledgedObservation{
                             EdlsAcknowledgedStatus::WatermarkStoreRejected};
        }

        /// Strict two-provider commit order:
        ///  1. reserve previously physically ABSENT EDLS path;
        ///  2. publish and CRC-readback new immutable EDLS successor;
        ///  3. prove every still ACKed predecessor unchanged;
        ///  4. commit+readback PowerLoss-atomic ACK watermark in KeyValue;
        ///  5. ONLY THEN retire the oldest physically acknowledged segment.
        ///
        /// Never report accepted before step 4. Failed watermark persistence
        /// leaves the old protected window intact and a safely unacknowledged
        /// candidate that Recover can resolve after a restart.
        /// @tparam TMaximumFrames Maximum records in this immutable segment.
        template <std::size_t TMaximumFrames>
        [[nodiscard]] EdlsAcknowledgedObservation PublishFrameSequence(
            std::span<const std::byte> completeEdlg,
            std::span<std::byte> scratch) noexcept {
            // Refuse before recovery or any provider mutation. This bound
            // includes EDLS framing/seal and reserves every physical slot,
            // including the unacknowledged successor, plus the ACK value.
            if (!IsByteCapacityValid())
                return {EdlsAcknowledgedStatus::InvalidByteCapacity};
            constexpr std::uint64_t overhead =
                EdlsHeaderBytes + (TMaximumFrames > 1U ? EdlsSealBytes : 0U);
            const auto slotCapacity = SegmentByteCapacity();
            if (slotCapacity < overhead || completeEdlg.size() > slotCapacity - overhead) {
                exclusiveWriterCompleted_ = false;
                return {EdlsAcknowledgedStatus::ByteCapacityExceeded};
            }
            // The full initial Recover remains authoritative after boot,
            // interrupted/failed publish, explicit recovery or uncertainty.
            // Once this same exclusive writer completed a fully verified
            // transaction, a fresh independent NVS watermark read combined
            // with the mandatory full CRC-clean physical preflight below
            // is sufficient; repeating Recover's previous scan is redundant.
            // Never cache a physical file inspection across publications.
            const bool previouslyComplete = exclusiveWriterCompleted_;
            const auto previousAck = exclusiveWriterAck_;
            exclusiveWriterCompleted_ = false;
            const auto recovered = previouslyComplete ? Read() : Recover(scratch);
            if (previouslyComplete && (recovered.Status != EdlsAcknowledgedStatus::Ready ||
                                       recovered.AcknowledgedGeneration != previousAck ||
                                       recovered.AcknowledgedIncarnation != incarnation_))
                return {EdlsAcknowledgedStatus::InvalidWatermark};
            if (recovered.Status != EdlsAcknowledgedStatus::Ready &&
                recovered.Status !=
                    EdlsAcknowledgedStatus::UnacknowledgedCandidateRemoved &&
                recovered.Status !=
                    EdlsAcknowledgedStatus::CommittedOldestRetired)
                return recovered;
            const auto ack = recovered.AcknowledgedGeneration;
            if (ack >= std::numeric_limits<std::uint64_t>::max() - 1U)
                return {EdlsAcknowledgedStatus::GenerationExhausted, ack};

            const auto prior = Detail::InspectProtectedRollingPlanWithScratch<
                TPhysicalSlots,
                TProtectedSlots,
                TPayloadMaximum>(
                *files_,
                paths_,
                scratch);
            const auto expected = ack < TProtectedSlots
                                      ? static_cast<std::size_t>(ack)
                                      : TProtectedSlots;
            if (prior.Status != EdlsProtectedRollingStatus::NoPendingRetirement ||
                prior.Occupied != expected ||
                prior.NewestGeneration != ack)
                return {EdlsAcknowledgedStatus::ArchiveAmbiguous, ack};
            if (prior.Occupied >= TPhysicalSlots)
                return {EdlsAcknowledgedStatus::NoPhysicalReserve, ack};
            // The exclusive-writer fast path must also reject externally
            // enlarged protected segments; never trust cached byte counts.
            for (const auto &slot : prior.Inspected.Slots) {
                if (slot.Condition == EdlsSlotCondition::Clean &&
                    slot.VerifiedPrefixBytes > slotCapacity)
                    return {EdlsAcknowledgedStatus::ByteCapacityExceeded, ack};
            }
            const auto posted = PublishEdlsSuccessorIntoEmptySlotWithScratch<
                TPhysicalSlots,
                TPayloadMaximum,
                TSegmentMaximum,
                TFileStorage,
                TMaximumFrames>(
                *files_,
                paths_,
                identity_,
                incarnation_,
                completeEdlg,
                scratch,
                &prior.Inspected);
            if (posted.Status != EdlsSuccessorStatus::PublishedAndVerified ||
                posted.Generation != ack + 1U ||
                posted.Slot >= TPhysicalSlots ||
                prior.Inspected.Slots[posted.Slot].Condition !=
                    EdlsSlotCondition::Empty)
                return {EdlsAcknowledgedStatus::PublishedNotAcknowledged,
                        ack,
                        posted.Slot};
            if (Detail::VerifyEdlsProtectedPredecessorsWithScratch<
                    TPhysicalSlots,
                    TPayloadMaximum>(
                    *files_,
                    paths_,
                    prior.Inspected,
                    scratch) != Detail::EdlsPredecessorVerificationResult::Verified)
                return {EdlsAcknowledgedStatus::AcknowledgedHistoryChanged,
                        ack,
                        posted.Slot};
            const auto committed = StoreWatermark(ack + 1U);
            if (committed.Status != EdlsAcknowledgedStatus::Ready)
                return {EdlsAcknowledgedStatus::WatermarkStoreRejected,
                        ack,
                        posted.Slot};
            if (prior.Occupied < TProtectedSlots) {
                exclusiveWriterAck_ = ack + 1U;
                exclusiveWriterCompleted_ = true;
                return {EdlsAcknowledgedStatus::AcknowledgedWithinCapacity,
                        ack + 1U,
                        posted.Slot};
            }
            const auto retired =
                FinishEdlsProtectedRollingRetirementWithScratch<
                    TPhysicalSlots,
                    TProtectedSlots,
                    TPayloadMaximum>(
                    *files_,
                    paths_,
                    scratch);
            if (retired.Status ==
                EdlsProtectedRollingStatus::RecoveredPendingRetirement) {
                exclusiveWriterAck_ = ack + 1U;
                exclusiveWriterCompleted_ = true;
            }
            return {
                retired.Status ==
                        EdlsProtectedRollingStatus::RecoveredPendingRetirement
                    ? EdlsAcknowledgedStatus::AcknowledgedAndRetired
                    : EdlsAcknowledgedStatus::AcknowledgedPendingRetirement,
                ack + 1U,
                posted.Slot};
        }

      public:
        /// Bind caller-owned, non-aliased providers and borrow the path and key storage.
        /// Bootstrap keeps both providers and the backing names alive for the archive lifetime.
        explicit PowerLossAcknowledgedEdls(
            TFileStorage &files,
            TKeyValueStorage &watermark,
            const std::array<
                ESPressio::Persistence::FilePathView,
                TPhysicalSlots> &paths,
            ESPressio::Persistence::KeyView watermarkKey,
            const std::array<std::byte, 16U> &identity,
            std::uint32_t runtimeIncarnation,
            std::uint64_t byteCapacity =
                EdlsHeaderBytes + static_cast<std::uint64_t>(TPhysicalSlots) * TSegmentMaximum) noexcept
            : byteCapacity_(byteCapacity), files_(&files), watermark_(&watermark), paths_(paths),
              watermarkKey_(watermarkKey), identity_(identity),
              incarnation_(runtimeIncarnation) {}

        /// Read only the PowerLoss-atomic independent acknowledgement fact.
        /// Neither a FileStorage success nor a parseable archive is an ACK.
        [[nodiscard]] EdlsAcknowledgedObservation Read() const noexcept {
            namespace P = ESPressio::Persistence;
            if (!IsByteCapacityValid())
                return {EdlsAcknowledgedStatus::InvalidByteCapacity};
            if (!files_->IsFileStorageReady() ||
                !watermark_->IsKeyValueStorageReady() ||
                watermarkKey_.Data() == nullptr ||
                watermarkKey_.Size() == 0U)
                return {EdlsAcknowledgedStatus::ProviderFailure};
            std::array<std::byte, EdlsHeaderBytes> bytes{};
            const auto native = watermark_->ReadValue(
                watermarkKey_,
                P::DestinationBufferView{
                    bytes.data(),
                    bytes.size()});
            if (native.Status == P::KeyValueReadStatus::NotFound)
                return {EdlsAcknowledgedStatus::MissingWatermark};
            if (native.Status != P::KeyValueReadStatus::Succeeded ||
                !P::HasFact(
                    native.Facts,
                    P::ReadFact::AvailableDataSizeIsKnown))
                return {EdlsAcknowledgedStatus::ProviderFailure};
            if (native.BytesTransferred != EdlsHeaderBytes ||
                native.AvailableDataSize.RawValue != EdlsHeaderBytes)
                return {EdlsAcknowledgedStatus::InvalidWatermark};
            EdlsHeader decoded{};
            if (ReadEdlsHeader(
                    bytes,
                    decoded) != EdlsHeaderStatus::Succeeded ||
                decoded.Profile != EdlsContentProfile::Unsealed ||
                decoded.Slot != 0U ||
                decoded.DeviceIdentifier != identity_)
                return {EdlsAcknowledgedStatus::InvalidWatermark};
            return {EdlsAcknowledgedStatus::Ready,
                    decoded.Generation - 1U,
                    TPhysicalSlots,
                    decoded.RuntimeIncarnation};
        }

        /// Only an empty, physically absent archive can be freshly bound to
        /// genesis ACK=0. Never silently install an ACK for legacy history.
        [[nodiscard]] EdlsAcknowledgedObservation InitializeEmpty() noexcept {
            exclusiveWriterCompleted_ = false;
            namespace P = ESPressio::Persistence;
            auto known = Read();
            if (known.Status != EdlsAcknowledgedStatus::MissingWatermark)
                return known;
            for (const auto path : paths_) {
                if (files_->GetFileSize(path).Status !=
                    P::FileSizeStatus::NotFound)
                    return {EdlsAcknowledgedStatus::ArchiveAmbiguous};
            }
            const auto written = StoreWatermark(0U);
            if (written.Status != EdlsAcknowledgedStatus::Ready)
                return written;
            return {EdlsAcknowledgedStatus::Initialized, 0U, TPhysicalSlots};
        }

        /// Reconcile only the exact ACKed latest N CRC-clean generations.
        ///
        /// A single other physical file is removable ONLY if the independent
        /// PowerLoss ledger proves its generation is not acknowledged. This
        /// permits zero-byte/torn candidate recovery without ever retiring
        /// a still-protected predecessor, while rejecting unknown provenance.
        [[nodiscard]] EdlsAcknowledgedObservation Recover(
            std::span<std::byte> scratch) noexcept {
            exclusiveWriterCompleted_ = false;
            namespace P = ESPressio::Persistence;
            const auto evidence = Read();
            if (evidence.Status != EdlsAcknowledgedStatus::Ready)
                return evidence;
            const auto ack = evidence.AcknowledgedGeneration;
            const auto countExpected = ack < TProtectedSlots
                                           ? static_cast<std::size_t>(ack)
                                           : TProtectedSlots;
            const auto first = ack >= TProtectedSlots
                                   ? ack - TProtectedSlots + 1U
                                   : 1U;
            std::array<bool, TProtectedSlots> observed{};
            std::array<EdlsSlotInspection, TPhysicalSlots> before{};
            std::size_t candidate = TPhysicalSlots;
            bool olderCommitted = false;
            std::uint32_t observedAckIncarnation = 0U;

            for (std::size_t slot = 0U; slot < TPhysicalSlots; ++slot) {
                const auto size = files_->GetFileSize(paths_[slot]);
                if (size.Status == P::FileSizeStatus::NotFound)
                    continue;
                if (size.Status != P::FileSizeStatus::Succeeded)
                    return {EdlsAcknowledgedStatus::ProviderFailure};
                // Reject incompatible pre-existing media without deleting it.
                if (size.Size.RawValue > SegmentByteCapacity())
                    return {EdlsAcknowledgedStatus::ByteCapacityExceeded, ack, slot};
                before[slot] = InspectEdlsSlotWithScratch<TPayloadMaximum>(
                    *files_,
                    paths_[slot],
                    static_cast<std::uint16_t>(slot),
                    scratch);
                const auto &found = before[slot];
                if (found.Condition == EdlsSlotCondition::Clean &&
                    found.VerifiedFrames > 0U &&
                    found.HasHeader &&
                    found.Header.DeviceIdentifier == identity_ &&
                    found.Header.Generation >= first &&
                    found.Header.Generation <= ack) {
                    const auto offset = static_cast<std::size_t>(
                        found.Header.Generation - first);
                    if (offset >= countExpected || observed[offset])
                        return {EdlsAcknowledgedStatus::ArchiveAmbiguous};
                    observed[offset] = true;
                    if (found.Header.Generation == ack)
                        observedAckIncarnation = found.Header.RuntimeIncarnation;
                    continue;
                }
                if (candidate != TPhysicalSlots)
                    return {EdlsAcknowledgedStatus::ArchiveAmbiguous};
                if (found.HasHeader && (found.Header.DeviceIdentifier != identity_ ||
                                        (found.Header.Generation != ack + 1U &&
                                         !(ack >= TProtectedSlots &&
                                           found.Header.Generation == first - 1U)))) {
                    return {EdlsAcknowledgedStatus::ArchiveAmbiguous};
                }
                candidate = slot;
                olderCommitted = found.Condition == EdlsSlotCondition::Clean &&
                                 found.VerifiedFrames > 0U &&
                                 found.HasHeader &&
                                 ack >= TProtectedSlots &&
                                 found.Header.Generation == first - 1U;
            }
            for (std::size_t i = 0U; i < countExpected; ++i)
                if (!observed[i])
                    return {EdlsAcknowledgedStatus::ArchiveAmbiguous};
            // The ledger's incarnation binds to the LAST ACKed segment;
            // previous generations can legitimately belong to older boots.
            if (ack > 0U &&
                observedAckIncarnation != evidence.AcknowledgedIncarnation)
                return {EdlsAcknowledgedStatus::InvalidWatermark};
            const auto same = Read();
            if (same.Status != EdlsAcknowledgedStatus::Ready ||
                same.AcknowledgedGeneration != ack ||
                same.AcknowledgedIncarnation !=
                    evidence.AcknowledgedIncarnation)
                return {EdlsAcknowledgedStatus::InvalidWatermark};
            if (candidate == TPhysicalSlots)
                return {EdlsAcknowledgedStatus::Ready, ack, candidate};
            for (std::size_t slot = 0U; slot < TPhysicalSlots; ++slot) {
                const auto &prior = before[slot];
                if (prior.Condition != EdlsSlotCondition::Clean ||
                    prior.Header.Generation < first ||
                    prior.Header.Generation > ack)
                    continue;
                if (Detail::VerifyEdlsPredecessorWithScratch<
                        TPayloadMaximum>(
                        *files_,
                        paths_[slot],
                        slot,
                        prior,
                        scratch) != Detail::EdlsPredecessorVerificationResult::Verified)
                    return {EdlsAcknowledgedStatus::
                                AcknowledgedHistoryChanged,
                            ack,
                            candidate};
            }
            if (olderCommitted) {
                const auto completed =
                    FinishEdlsProtectedRollingRetirementWithScratch<
                        TPhysicalSlots,
                        TProtectedSlots,
                        TPayloadMaximum>(
                        *files_,
                        paths_,
                        scratch);
                if (completed.Status !=
                    EdlsProtectedRollingStatus::RecoveredPendingRetirement)
                    return {EdlsAcknowledgedStatus::ProviderFailure,
                            ack,
                            candidate};
            } else {
                if (files_->RemoveFile(paths_[candidate]) !=
                    P::FileRemoveStatus::Succeeded)
                    return {EdlsAcknowledgedStatus::ProviderFailure,
                            ack,
                            candidate};
            }
            if (files_->GetFileSize(paths_[candidate]).Status !=
                P::FileSizeStatus::NotFound)
                return {EdlsAcknowledgedStatus::ProviderFailure,
                        ack,
                        candidate};
            for (std::size_t slot = 0U; slot < TPhysicalSlots; ++slot) {
                const auto &prior = before[slot];
                if (prior.Condition != EdlsSlotCondition::Clean ||
                    prior.Header.Generation < first ||
                    prior.Header.Generation > ack)
                    continue;
                if (Detail::VerifyEdlsPredecessorWithScratch<
                        TPayloadMaximum>(
                        *files_,
                        paths_[slot],
                        slot,
                        prior,
                        scratch) != Detail::EdlsPredecessorVerificationResult::Verified)
                    return {EdlsAcknowledgedStatus::
                                AcknowledgedHistoryChanged,
                            ack,
                            candidate};
            }
            const auto next = Read();
            if (next.Status != EdlsAcknowledgedStatus::Ready ||
                next.AcknowledgedGeneration != ack ||
                next.AcknowledgedIncarnation !=
                    evidence.AcknowledgedIncarnation)
                return {EdlsAcknowledgedStatus::InvalidWatermark,
                        ack,
                        candidate};
            return {
                olderCommitted
                    ? EdlsAcknowledgedStatus::CommittedOldestRetired
                    : EdlsAcknowledgedStatus::UnacknowledgedCandidateRemoved,
                ack,
                candidate};
        }

        /// Immutable sink-owned logical byte budget selected before initialisation.
        [[nodiscard]] std::uint64_t ByteCapacity() const noexcept {
            return byteCapacity_;
        }

        /// Per-segment share after reserving the ACK value and all physical slots.
        /// A batch must fit this bound INCLUDING its header and optional seal.
        [[nodiscard]] std::uint64_t ConfiguredSegmentByteCapacity() const noexcept {
            return SegmentByteCapacity();
        }

        /// Maximum canonical payload bytes in each individual EDLG frame.
        [[nodiscard]] static constexpr std::size_t MaximumFramePayloadBytes() noexcept {
            return TPayloadMaximum;
        }

        /// Maximum total immutable segment size, including its EDLS header.
        [[nodiscard]] static constexpr std::size_t MaximumSegmentBytes() noexcept {
            return TSegmentMaximum;
        }

        /// Publish exactly one complete EDLG frame using the existing transaction.
        [[nodiscard]] EdlsAcknowledgedObservation Publish(
            std::span<const std::byte> completeEdlg,
            std::span<std::byte> scratch) noexcept {
            return PublishFrameSequence<1U>(
                completeEdlg,
                scratch);
        }

        /// Commit one explicit bounded sequence of complete canonical frames.
        /// AcknowledgedGeneration names the SEGMENT, never a record count.
        /// All frames in this generation share the same durable ACK boundary.
        /// Scratch must not alias frames and must fit the complete segment.
        /// @tparam TMaximumFrames Compile-time frame-count bound, 1..64.
        template <std::size_t TMaximumFrames>
        [[nodiscard]] EdlsAcknowledgedObservation PublishBatch(
            std::span<const std::byte> completeFrames,
            std::span<std::byte> scratch) noexcept {
            return PublishFrameSequence<TMaximumFrames>(
                completeFrames,
                scratch);
        }
    };

} // namespace ESPressio::Logging
