#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <ESPressio_Serialisation.hpp>
#include "GeneratedLogRecordWireProjection.hpp"
#include "EdlsArchiveRotation.hpp"
#include "EdlgFrame.hpp"

namespace ESPressio::Logging {

enum class GeneratedEdlsPublishStatus : std::uint8_t {
        Published = 0U,
        SourceIdentityMismatch = 1U,
        ProjectionRejected = 2U,
        CborRejected = 3U,
        CborCapacityExceeded = 4U,
        FramingRejected = 5U,
        ArchiveRejected = 6U
    };

template<std::size_t N>
struct GeneratedEdlsPublishOutcome final {
    GeneratedEdlsPublishStatus Status{GeneratedEdlsPublishStatus::ProjectionRejected};
    GeneratedLogRecordProjectionOutcome Projection{};
    EdlsProtectedRollingOutcome<N> Archive{};
    std::size_t CborBytes{0U};
    std::size_t EdlgBytes{0U};
    [[nodiscard]] bool IsPublished() const noexcept {
        return Status==GeneratedEdlsPublishStatus::Published;
    }
};

/// Called only from an application-composed worker with exclusive archive
/// ownership. All scratch is caller-supplied and capacity-bounded. A result
/// of Published means complete EDLS/EDLG readback and predecessor checks,
/// not blanket atomicity of the underlying FileStorage device.
template<
    class TMessageSchema,
    std::size_t TProtectedSlots, std::size_t TMaximumPayload,
    std::size_t TMaximumSegment,
    AttemptSequenceWidth TWidth, class... TParameters,
    std::size_t TMaximumArguments, std::size_t TArgumentBytes,
    std::size_t TPhysicalSlots, class TStorage
>
[[nodiscard]] GeneratedEdlsPublishOutcome<TPhysicalSlots>
PublishGeneratedLogRecordToProtectedEdlsWithScratch(
    const BasicLogRecord<TWidth,TMaximumArguments,TArgumentBytes>& record,
    GeneratedLogRecordWireProjection<TWidth,TParameters...>& projection,
    OwnedPositionalScratch<TParameters...>& positionalScratch,
    std::span<std::byte> cborScratch,
    std::span<std::byte> edlgScratch,
    TStorage& storage,
    const std::array<
        ESPressio::Persistence::FilePathView,TPhysicalSlots
    >& paths,
    const std::array<std::byte,16U>& deviceIdentifier,
    std::uint32_t runtimeIncarnation,
    std::span<std::byte> archiveScratch
) noexcept {
    static_assert(TPhysicalSlots==TProtectedSlots+1U);
    static_assert(TMaximumPayload>=1U && TMaximumPayload<=4096U);
    static_assert(TMaximumSegment>=EdlsHeaderBytes+TMaximumPayload+20U);
    static_assert(System::TypeIdentifierOf<
        GeneratedLogRecordWireProjection<TWidth,TParameters...>
    > == System::TypeIdentifierOf<
        BasicLogRecord<TWidth,TMaximumArguments,TArgumentBytes>
    >);
    GeneratedEdlsPublishOutcome<TPhysicalSlots> result{};
    if(runtimeIncarnation==0U ||
       record.Incarnation.Value()!=runtimeIncarnation) {
        result.Status=GeneratedEdlsPublishStatus::SourceIdentityMismatch;
        return result;
    }
    for(std::size_t i=0U;i<deviceIdentifier.size();++i) {
        if(record.Device.Bytes()[i] !=
           std::to_integer<std::uint8_t>(deviceIdentifier[i])) {
            result.Status=GeneratedEdlsPublishStatus::SourceIdentityMismatch;
            return result;
        }
    }
    result.Projection=ProjectGeneratedLogRecord<
        TMessageSchema,TWidth,TParameters...
    >(record,projection,positionalScratch);
    if(!result.Projection.IsSuccessful()) {
        result.Status=GeneratedEdlsPublishStatus::ProjectionRejected;
        return result;
    }
    const auto measured=Serialisation::Measure<
        Serialisation::Cbor,Serialisation::RootProfile::TypedEnvelope
    >(projection);
    if(!measured.IsSuccessful()) {
        result.Status=GeneratedEdlsPublishStatus::CborRejected;
        return result;
    }
    if(measured.RequiredBytes>TMaximumPayload ||
       measured.RequiredBytes>cborScratch.size()) {
        result.Status=GeneratedEdlsPublishStatus::CborCapacityExceeded;
        return result;
    }
    const auto encoded=Serialisation::Serialise<
        Serialisation::Cbor,Serialisation::RootProfile::TypedEnvelope
    >(projection,cborScratch.first(measured.RequiredBytes));
    if(!encoded.IsSuccessful() ||
       encoded.BytesWritten!=measured.RequiredBytes) {
        result.Status=GeneratedEdlsPublishStatus::CborRejected;
        return result;
    }
    result.CborBytes=encoded.BytesWritten;
    std::size_t framed=0U;
    if(WriteEdlgFrame(
        EdlgCodec::Cbor,
        std::span<const std::byte>(cborScratch.data(),result.CborBytes),
        edlgScratch,framed,TMaximumPayload
    )!=EdlgFrameWriteResult::Succeeded) {
        result.Status=GeneratedEdlsPublishStatus::FramingRejected;
        return result;
    }
    result.EdlgBytes=framed;
    result.Archive=PublishEdlsProtectedRollingSuccessorWithScratch<
        TPhysicalSlots,TProtectedSlots,TMaximumPayload,TMaximumSegment
    >(storage,paths,deviceIdentifier,runtimeIncarnation,
      std::span<const std::byte>(edlgScratch.data(),framed),archiveScratch);
    result.Status=(
        result.Archive.Status==EdlsProtectedRollingStatus::PublishedWithinCapacity ||
        result.Archive.Status==EdlsProtectedRollingStatus::PublishedAndRetired)
        ? GeneratedEdlsPublishStatus::Published
        : GeneratedEdlsPublishStatus::ArchiveRejected;
    return result;
}

/// The old host API is retained, using stack scratch. ESP32 workers with
/// high payload bounds must instead call WithScratch and supply one
/// Bootstrap-owned scratch separate from their EDLG frame buffer. Once
/// framing completes, the former CBOR buffer may be reused as archive
/// scratch if it has the full EdlsHeaderBytes+maximumPayload+20 capacity.
template<
    class TMessageSchema,
    std::size_t TProtectedSlots,std::size_t TMaximumPayload,
    std::size_t TMaximumSegment,
    AttemptSequenceWidth TWidth,class... TParameters,
    std::size_t TMaximumArguments,std::size_t TArgumentBytes,
    std::size_t TPhysicalSlots,class TStorage
>
[[nodiscard]] GeneratedEdlsPublishOutcome<TPhysicalSlots>
PublishGeneratedLogRecordToProtectedEdls(
    const BasicLogRecord<TWidth,TMaximumArguments,TArgumentBytes>& record,
    GeneratedLogRecordWireProjection<TWidth,TParameters...>& projection,
    OwnedPositionalScratch<TParameters...>& positionalScratch,
    std::span<std::byte> cborScratch,
    std::span<std::byte> edlgScratch,
    TStorage& storage,
    const std::array<ESPressio::Persistence::FilePathView,
        TPhysicalSlots>& paths,
    const std::array<std::byte,16U>& deviceIdentifier,
    std::uint32_t runtimeIncarnation
) noexcept {
    std::array<std::byte,EdlsHeaderBytes+TMaximumPayload+20U> archiveScratch{};
    return PublishGeneratedLogRecordToProtectedEdlsWithScratch<
        TMessageSchema,TProtectedSlots,TMaximumPayload,TMaximumSegment
    >(record,projection,positionalScratch,cborScratch,edlgScratch,storage,
      paths,deviceIdentifier,runtimeIncarnation,archiveScratch);
}

} // namespace ESPressio::Logging
