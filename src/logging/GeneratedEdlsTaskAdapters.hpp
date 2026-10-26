#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <ESPressio_Serialisation.hpp>
#include "GeneratedLogRecordEdlsArchive.hpp"
#include "RecordProduction.hpp"
#include "OwnedRepresentationFanout.hpp"

namespace ESPressio::Logging {

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
        SourceIdentityMismatch, FramingFailure, ArchiveFailure
    };

    /// Existing EDP-Threading Task-completion-side delivery callback.
    /// Takes already copied canonical CBOR bytes, validates all typed root
    /// fields and owner identities, then uses existing immutable EDLS owner.
    /// Bootstrap controls the SINGLE writer's lifetime and serialisation.
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
    public:
        TypedEdlsTaskCompletion LastCompletion{
            TypedEdlsTaskCompletion::NotInvoked};
        EdlsProtectedRollingOutcome<TPhysicalSlots> LastArchive{};

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

        [[nodiscard]] RecordSinkSubmission operator()(
            EdlgCodec codec,std::span<const std::byte> encoded
        ) noexcept {
            LastArchive=EdlsProtectedRollingOutcome<TPhysicalSlots>{};
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
    };

} // namespace ESPressio::Logging
