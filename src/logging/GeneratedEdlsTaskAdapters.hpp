#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <concepts>
#include <span>
#include <utility>

#include <ESPressio_Serialisation.hpp>
#include "GeneratedLogRecordEdlsArchive.hpp"
#include "EdlsPowerLossAcknowledgement.hpp"
#include "BoundedEdlsBatch.hpp"
#include "CoordinatorSinkService.hpp"
#include "RecordProduction.hpp"
#include "OwnedRepresentationFanout.hpp"

namespace ESPressio::Logging {

    namespace Detail {
        /// Existing bounded batch operations required by this ordinary delivery callback.
        template<class TTarget>
        concept BatchDeliveryTarget = requires(
            TTarget& target, std::span<const std::byte> frame
        ) {
            { target.Offer(frame) } noexcept -> std::same_as<EdlsBatchOutcome>;
            { target.Service() } noexcept -> std::same_as<EdlsBatchOutcome>;
            { target.Flush() } noexcept -> std::same_as<EdlsBatchOutcome>;
            { target.NextFlushDeadline() } noexcept ->
                std::same_as<std::optional<Clock::MonotonicTimestamp>>;
        };
    }


    /// Schema-qualified canonical-CBOR encoder, for the EXISTING
    /// EncodeOnceAndSubmitToSinks / EDP-Memory pooled representation owner.
    /// This adapter has no queue, worker, storage or SDK of its own.
    template<
        class TMessageSchema, AttemptSequenceWidth TWidth,
        std::size_t TMaximumArguments,std::size_t TArgumentBytes,
        class... TParameters
    >
    class GeneratedLogRecordCborEncoder final {
        using Source=BasicLogRecord<TWidth,TMaximumArguments,TArgumentBytes>;
        using Projection=GeneratedLogRecordWireProjection<TWidth,TParameters...>;
        Projection* _projection;
        OwnedPositionalScratch<TParameters...>* _scratch;
    public:
        explicit GeneratedLogRecordCborEncoder(
            Projection& projection,
            OwnedPositionalScratch<TParameters...>& scratch
        ) noexcept : _projection(&projection),_scratch(&scratch) {}

        [[nodiscard]] RepresentationEncodeResult operator()(
            const Source& record,std::span<std::byte> output
        ) noexcept {
            if(!ProjectGeneratedLogRecord<
                TMessageSchema,TWidth,TParameters...
            >(record,*_projection,*_scratch).IsSuccessful())
                return {RepresentationEncodeStatus::Rejected,0U};
            const auto measured=Serialisation::Measure<
                Serialisation::Cbor,Serialisation::RootProfile::TypedEnvelope
            >(*_projection);
            if(!measured.IsSuccessful() ||
               measured.RequiredBytes>output.size())
                return {RepresentationEncodeStatus::Rejected,0U};
            const auto written=Serialisation::Serialise<
                Serialisation::Cbor,Serialisation::RootProfile::TypedEnvelope
            >(*_projection,output.first(measured.RequiredBytes));
            if(!written.IsSuccessful() ||
               written.BytesWritten!=measured.RequiredBytes)
                return {RepresentationEncodeStatus::ProviderFailure,0U};
            return {RepresentationEncodeStatus::Succeeded,written.BytesWritten};
        }
    };

    enum class TypedEdlsTaskCompletion : std::uint8_t {
        NotInvoked, Archived, InvalidCodec, InvalidTypedRecord,
        SourceIdentityMismatch, FramingFailure, ArchiveFailure,
        DurablyAcknowledged, BufferedForBatch
    };

    /// Existing EDP-Threading Task-completion-side delivery callback.
    /// Takes already copied canonical CBOR bytes, validates all typed root
    /// fields and owner identities, then uses existing immutable EDLS owner.
    /// Bootstrap controls the SINGLE writer's lifetime and serialisation.
    /// An explicit BoundedEdlsBatch target copies framed RAM on Accepted,
    /// reports BufferedForBatch, and exposes optional ServiceCoordinator.
    /// Offer and service MUST use the same serialized ordinary owner. A Task
    /// terminal Accepted for this target is not a durable ACK. Only the
    /// service delta reports NVS-confirmed per-sink record completions.
    ///
    /// AsyncCopyingTaskSink's Accepted is only Task admission: durable
    /// archive status is available from this callback after Task terminal.
    template<
        class TMessageSchema,AttemptSequenceWidth TWidth,
        std::size_t TPhysicalSlots,std::size_t TProtectedSlots,
        std::size_t TMaximumPayload,std::size_t TMaximumSegment,
        class TStorage,class... TParameters
    >
    class TypedCborProtectedEdlsTaskDelivery final {
        using Projection=GeneratedLogRecordWireProjection<TWidth,TParameters...>;
        TStorage* _storage;
        Projection* _decoded;
        std::span<std::byte> _framingScratch;
        std::span<std::byte> _archiveScratch;
        std::array<ESPressio::Persistence::FilePathView,TPhysicalSlots> _paths;
        std::array<std::byte,16U> _device;
        std::uint32_t _incarnation;
        /// Confirmed completions not yet consumed by the existing coordinator's service poll.
        std::uint64_t _pendingDurableCompletions{0U};

        /// Records only an actual strict segment/NVS completion, never RAM acceptance.
        [[nodiscard]] bool ObserveBatchAcknowledgement(
            const EdlsBatchOutcome& outcome
        ) noexcept {
            if (outcome.Status != EdlsBatchStatus::DurablyAcknowledged &&
                outcome.Status != EdlsBatchStatus::DurablyAcknowledgedPendingRetirement) {
                LastCompletion = TypedEdlsTaskCompletion::ArchiveFailure;
                LastAcknowledgement = outcome.Archive;
                return false;
            }
            _pendingDurableCompletions += outcome.AcknowledgedRecords;
            LastAcknowledgement = outcome.Archive;
            LastCompletion = TypedEdlsTaskCompletion::DurablyAcknowledged;
            return true;
        }
    public:
        TypedEdlsTaskCompletion LastCompletion{
            TypedEdlsTaskCompletion::NotInvoked};
        EdlsProtectedRollingOutcome<TPhysicalSlots> LastArchive{};
        /// Populated only when TStorage is an opt-in strictly ACKed archive
        /// bound to the existing PowerLoss KeyValueStorage owner.
        EdlsAcknowledgedObservation LastAcknowledgement{};

        TypedCborProtectedEdlsTaskDelivery(
            TStorage& storage,Projection& decoded,
            std::span<std::byte> framingScratch,
            std::span<std::byte> archiveScratch,
            const std::array<
                ESPressio::Persistence::FilePathView,TPhysicalSlots>& paths,
            const std::array<std::byte,16U>& deviceIdentifier,
            std::uint32_t runtimeIncarnation
        ) noexcept : _storage(&storage),_decoded(&decoded),
             _framingScratch(framingScratch),_archiveScratch(archiveScratch),
             _paths(paths),
             _device(deviceIdentifier),_incarnation(runtimeIncarnation) {
            static_assert(TPhysicalSlots==TProtectedSlots+1U);
        }

        /// Legacy callbacks retain value semantics; a batch callback owns
        /// consumable completion deltas and must never fork that ownership.
        TypedCborProtectedEdlsTaskDelivery(const TypedCborProtectedEdlsTaskDelivery&) noexcept
            requires (!Detail::BatchDeliveryTarget<TStorage>) = default;
        TypedCborProtectedEdlsTaskDelivery(const TypedCborProtectedEdlsTaskDelivery&) noexcept
            requires Detail::BatchDeliveryTarget<TStorage> = delete;
        TypedCborProtectedEdlsTaskDelivery& operator=(
            const TypedCborProtectedEdlsTaskDelivery&) noexcept
            requires (!Detail::BatchDeliveryTarget<TStorage>) = default;
        TypedCborProtectedEdlsTaskDelivery& operator=(
            const TypedCborProtectedEdlsTaskDelivery&) noexcept
            requires Detail::BatchDeliveryTarget<TStorage> = delete;
        TypedCborProtectedEdlsTaskDelivery(TypedCborProtectedEdlsTaskDelivery&&) noexcept
            requires (!Detail::BatchDeliveryTarget<TStorage>) = default;
        TypedCborProtectedEdlsTaskDelivery(TypedCborProtectedEdlsTaskDelivery&&) noexcept
            requires Detail::BatchDeliveryTarget<TStorage> = delete;
        TypedCborProtectedEdlsTaskDelivery& operator=(
            TypedCborProtectedEdlsTaskDelivery&&) noexcept
            requires (!Detail::BatchDeliveryTarget<TStorage>) = default;
        TypedCborProtectedEdlsTaskDelivery& operator=(
            TypedCborProtectedEdlsTaskDelivery&&) noexcept
            requires Detail::BatchDeliveryTarget<TStorage> = delete;

        /// Services an opt-in batch from its SAME serialized ordinary owner.
        /// This does not create a worker/timer or retry uncertain publication.
        /// Returned per-sink completion deltas are consumed exactly once.
        [[nodiscard]] CoordinatorSinkServiceOutcome ServiceCoordinator() noexcept
            requires Detail::BatchDeliveryTarget<TStorage> {
            CoordinatorSinkServiceOutcome observation{};
            const auto serviced = _storage->Service();
            if (serviced.Status == EdlsBatchStatus::DurablyAcknowledged ||
                serviced.Status == EdlsBatchStatus::DurablyAcknowledgedPendingRetirement) {
                (void)ObserveBatchAcknowledgement(serviced);
            } else if (serviced.Status != EdlsBatchStatus::Waiting &&
                       serviced.Status != EdlsBatchStatus::Empty) {
                LastCompletion = TypedEdlsTaskCompletion::ArchiveFailure;
                LastAcknowledgement = serviced.Archive;
                observation.Status = CoordinatorSinkServiceStatus::RecoveryRequired;
            }
            observation.DurableSinkCompletions = _pendingDurableCompletions;
            _pendingDurableCompletions = 0U;
            if (observation.Status == CoordinatorSinkServiceStatus::Ready) {
                observation.NextDeadline = _storage->NextFlushDeadline();
            }
            return observation;
        }

        [[nodiscard]] RecordSinkSubmission operator()(
            EdlgCodec codec,std::span<const std::byte> encoded
        ) noexcept {
            LastArchive=EdlsProtectedRollingOutcome<TPhysicalSlots>{};
            LastAcknowledgement=EdlsAcknowledgedObservation{};
            LastCompletion=TypedEdlsTaskCompletion::InvalidCodec;
            if(codec!=EdlgCodec::Cbor || encoded.empty() ||
               encoded.size()>TMaximumPayload) {
                return RecordSinkSubmission::Rejected;
            }
            LastCompletion=TypedEdlsTaskCompletion::InvalidTypedRecord;
            const auto decoded=Serialisation::Deserialise<
                Serialisation::Cbor,Serialisation::RootProfile::TypedEnvelope
            >(encoded,*_decoded);
            if(!decoded.IsSuccessful() ||
               _decoded->Message.Domain!=TMessageSchema::SourceDomain ||
               _decoded->Message.SubDomain!=TMessageSchema::SourceSubDomain ||
               _decoded->Message.String!=TMessageSchema::SourceString ||
               _decoded->DiagnosticSchemaFingerprint!=
                    TMessageSchema::OwningContract::DiagnosticSchemaFingerprint ||
               _decoded->SequenceWidth!=static_cast<std::uint8_t>(TWidth) ||
               !_decoded->Attempt.IsValid()) {
                return RecordSinkSubmission::Rejected;
            }
            LastCompletion=TypedEdlsTaskCompletion::SourceIdentityMismatch;
            if(_incarnation==0U ||
                _decoded->Incarnation.Value()!=_incarnation)
                return RecordSinkSubmission::Rejected;
            for(std::size_t i=0U;i<16U;++i) {
                if(_decoded->Device.Bytes()[i] !=
                    std::to_integer<std::uint8_t>(_device[i])) {
                    return RecordSinkSubmission::Rejected;
                }
            }
            // Canonical-source qualification: reject trailing or alternate
            // encodings even if a permissive decoder reconstructed an equal
            // semantic value. Re-encode into the already caller-owned frame
            // scratch, then reuse it for the CRC frame; no new scratch owner.
            const auto measured=Serialisation::Measure<
                Serialisation::Cbor,Serialisation::RootProfile::TypedEnvelope
            >(*_decoded);
            if(!measured.IsSuccessful() || measured.RequiredBytes!=encoded.size() ||
               measured.RequiredBytes>_framingScratch.size()) {
                LastCompletion=TypedEdlsTaskCompletion::InvalidTypedRecord;
                return RecordSinkSubmission::Rejected;
            }
            const auto canonical=Serialisation::Serialise<
                Serialisation::Cbor,Serialisation::RootProfile::TypedEnvelope
            >(*_decoded,_framingScratch.first(measured.RequiredBytes));
            if(!canonical.IsSuccessful() ||
               canonical.BytesWritten!=encoded.size()) {
                LastCompletion=TypedEdlsTaskCompletion::InvalidTypedRecord;
                return RecordSinkSubmission::Rejected;
            }
            for(std::size_t i=0U;i<encoded.size();++i) {
                if(_framingScratch[i]!=encoded[i]) {
                    LastCompletion=TypedEdlsTaskCompletion::InvalidTypedRecord;
                    return RecordSinkSubmission::Rejected;
                }
            }
            std::size_t framed=0U;
            if(WriteEdlgFrame(EdlgCodec::Cbor,encoded,_framingScratch,
                framed,TMaximumPayload)!=EdlgFrameWriteResult::Succeeded) {
                LastCompletion=TypedEdlsTaskCompletion::FramingFailure;
                return RecordSinkSubmission::Rejected;
            }
            if constexpr (Detail::BatchDeliveryTarget<TStorage>) {
                const auto complete = std::span<const std::byte>(
                    _framingScratch.data(), framed
                );
                auto offered = _storage->Offer(complete);
                if (offered.Status == EdlsBatchStatus::FlushRequired) {
                    // The new input was NOT consumed. Complete the existing
                    // batch once, then reoffer only after a confirmed ACK.
                    if (!ObserveBatchAcknowledgement(_storage->Flush())) {
                        return RecordSinkSubmission::ProviderFailure;
                    }
                    offered = _storage->Offer(complete);
                }
                if (offered.Status == EdlsBatchStatus::Buffered) {
                    LastCompletion = TypedEdlsTaskCompletion::BufferedForBatch;
                    return RecordSinkSubmission::Accepted;
                }
                if (offered.Status == EdlsBatchStatus::InvalidFrame ||
                    offered.Status == EdlsBatchStatus::FrameTooLarge) {
                    LastCompletion = TypedEdlsTaskCompletion::FramingFailure;
                    return RecordSinkSubmission::Rejected;
                }
                LastCompletion = TypedEdlsTaskCompletion::ArchiveFailure;
                LastAcknowledgement = offered.Archive;
                return RecordSinkSubmission::ProviderFailure;
            } else {
            // Compile-time optional strict durability. The SAME canonical
            // full-LogRecord validation and existing EDLG frame owner above
            // are reused; only the injected archive publisher changes.
            if constexpr (requires(TStorage& archive,
                                   std::span<const std::byte> complete,
                                   std::span<std::byte> scratch) {
                {archive.Publish(complete,scratch)} noexcept ->
                    std::same_as<EdlsAcknowledgedObservation>;
            }) {
                LastAcknowledgement=_storage->Publish(
                    std::span<const std::byte>(
                        _framingScratch.data(),framed),_archiveScratch);
                if(LastAcknowledgement.Status==
                       EdlsAcknowledgedStatus::AcknowledgedWithinCapacity ||
                   LastAcknowledgement.Status==
                       EdlsAcknowledgedStatus::AcknowledgedAndRetired ||
                   LastAcknowledgement.Status==
                       EdlsAcknowledgedStatus::AcknowledgedPendingRetirement) {
                    LastCompletion=TypedEdlsTaskCompletion::DurablyAcknowledged;
                    return RecordSinkSubmission::Accepted;
                }
                LastCompletion=TypedEdlsTaskCompletion::ArchiveFailure;
                return RecordSinkSubmission::ProviderFailure;
            } else {
            LastArchive=PublishEdlsProtectedRollingSuccessorWithScratch<
                TPhysicalSlots,TProtectedSlots,TMaximumPayload,TMaximumSegment
            >(*_storage,_paths,_device,_incarnation,
                std::span<const std::byte>(_framingScratch.data(),framed),
                _archiveScratch);
            if(LastArchive.Status==
                EdlsProtectedRollingStatus::PublishedWithinCapacity ||
               LastArchive.Status==
                EdlsProtectedRollingStatus::PublishedAndRetired) {
                LastCompletion=TypedEdlsTaskCompletion::Archived;
                return RecordSinkSubmission::Accepted;
            }
            LastCompletion=TypedEdlsTaskCompletion::ArchiveFailure;
            return RecordSinkSubmission::ProviderFailure;
            }
            } // non-batching target
        }
    };

} // namespace ESPressio::Logging
