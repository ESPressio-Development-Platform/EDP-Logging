#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <ESPressio_Serialisation.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>

#include "GeneratedLogRecordWireProjection.hpp"
#include "EdlsArchiveRotation.hpp"
#include "EdlgFrameWriter.hpp"

namespace ESPressio::Logging {

    /// First refusing stage or confirmed EDLS readback of a generated record.
    enum class GeneratedEdlsPublishStatus : std::uint8_t {
        /// The protected rolling publisher confirmed the successor.
        Published = 0U,
        /// The record device or incarnation disagrees with the archive inputs.
        SourceIdentityMismatch = 1U,
        /// Source validation or generated wire projection declined the record.
        ProjectionRejected = 2U,
        /// EDP-Serialisation declined the canonical encoding.
        CborRejected = 3U,
        /// The measured root exceeds the frame payload bound or caller CBOR scratch.
        CborCapacityExceeded = 4U,
        /// EDLG framing declined the encoded root.
        FramingRejected = 5U,
        /// The protected rolling publisher did not confirm successful publication.
        ArchiveRejected = 6U
    };

    /// Publication disposition with stage-specific evidence for the caller.
    /// @tparam N Physical path count used by the archive outcome.
    template <std::size_t N>
    struct GeneratedEdlsPublishOutcome final {
        /// Overall stage disposition.
        GeneratedEdlsPublishStatus Status{GeneratedEdlsPublishStatus::ProjectionRejected};
        /// Source projection evidence after the projection is invoked.
        GeneratedLogRecordProjectionOutcome Projection{};
        /// Protected rolling evidence after the archive is invoked.
        EdlsProtectedRollingOutcome<N> Archive{};
        /// Size of the successfully produced canonical root, or zero.
        std::size_t CborBytes{0U};
        /// Size of the complete EDLG frame once framing succeeds.
        std::size_t EdlgBytes{0U};
        /// Tests for confirmed publication under the selected archive contract.
        [[nodiscard]] bool IsPublished() const noexcept {
            return Status == GeneratedEdlsPublishStatus::Published;
        }
    };

    /// Called only from an application-composed worker with exclusive archive
    /// ownership. All scratch is caller-supplied and capacity-bounded. A result
    /// of Published means complete EDLS/EDLG readback and predecessor checks,
    /// not blanket atomicity of the underlying FileStorage device.
    /// @tparam TMessageSchema Generated descriptor governing identity and argument kinds.
    /// @tparam TProtectedSlots Number of complete retained segments.
    /// @tparam TMaximumPayload Maximum bytes in one EDLG frame payload.
    /// @tparam TMaximumSegment Maximum bytes in one complete EDLS segment.
    /// @tparam TWidth Selected bit width of the source attempt sequence.
    /// @tparam TParameters Exact ordered generated argument types.
    /// @tparam TMaximumArguments Descriptor capacity of the source arena.
    /// @tparam TArgumentBytes Byte capacity of the source arena.
    /// @tparam TPhysicalSlots Configured physical paths including the successor reserve.
    /// @tparam TStorage Bootstrap-selected FileStorage owner.
    /// @tparam TByteOperationsProvider Stateless Memory copy owner for frame and segment assembly.
    template <
        class TMessageSchema,
        std::size_t TProtectedSlots,
        std::size_t TMaximumPayload,
        std::size_t TMaximumSegment,
        AttemptSequenceWidth TWidth,
        class... TParameters,
        std::size_t TMaximumArguments,
        std::size_t TArgumentBytes,
        std::size_t TPhysicalSlots,
        class TStorage,
        class TByteOperationsProvider = ESPressio::Platform::Portable::Memory::ByteOperationsProvider>
    [[nodiscard]] GeneratedEdlsPublishOutcome<TPhysicalSlots>
    PublishGeneratedLogRecordToProtectedEdlsWithScratch(
        const BasicLogRecord<TWidth, TMaximumArguments, TArgumentBytes> &record,
        GeneratedLogRecordWireProjection<TWidth, TParameters...> &projection,
        OwnedPositionalScratch<TParameters...> &positionalScratch,
        std::span<std::byte> cborScratch,
        std::span<std::byte> edlgScratch,
        TStorage &storage,
        const std::array<
            ESPressio::Persistence::FilePathView,
            TPhysicalSlots> &paths,
        const std::array<std::byte, 16U> &deviceIdentifier,
        std::uint32_t runtimeIncarnation,
        std::span<std::byte> archiveScratch,
        const TByteOperationsProvider& byteOperations = {}) noexcept {
        static_assert(TPhysicalSlots == TProtectedSlots + 1U);
        static_assert(TMaximumPayload >= 1U && TMaximumPayload <= 4096U);
        static_assert(TMaximumSegment >= EdlsHeaderBytes + TMaximumPayload + 20U);
        static_assert(System::TypeIdentifierOf<
                          GeneratedLogRecordWireProjection<TWidth, TParameters...>> == System::TypeIdentifierOf<BasicLogRecord<TWidth, TMaximumArguments, TArgumentBytes>>);
        GeneratedEdlsPublishOutcome<TPhysicalSlots> result{};
        if (runtimeIncarnation == 0U ||
            record.Incarnation.Value() != runtimeIncarnation) {
            result.Status = GeneratedEdlsPublishStatus::SourceIdentityMismatch;
            return result;
        }
        for (std::size_t i = 0U; i < deviceIdentifier.size(); ++i) {
            if (record.Device.Bytes()[i] !=
                std::to_integer<std::uint8_t>(deviceIdentifier[i])) {
                result.Status = GeneratedEdlsPublishStatus::SourceIdentityMismatch;
                return result;
            }
        }
        result.Projection = ProjectGeneratedLogRecord<
            TMessageSchema,
            TWidth,
            TParameters...>(record, projection, positionalScratch);
        if (!result.Projection.IsSuccessful()) {
            result.Status = GeneratedEdlsPublishStatus::ProjectionRejected;
            return result;
        }
        const auto measured = Serialisation::Measure<
            Serialisation::Cbor,
            Serialisation::RootProfile::TypedEnvelope>(projection);
        if (!measured.IsSuccessful()) {
            result.Status = GeneratedEdlsPublishStatus::CborRejected;
            return result;
        }
        if (measured.RequiredBytes > TMaximumPayload ||
            measured.RequiredBytes > cborScratch.size()) {
            result.Status = GeneratedEdlsPublishStatus::CborCapacityExceeded;
            return result;
        }
        const auto encoded = Serialisation::Serialise<
            Serialisation::Cbor,
            Serialisation::RootProfile::TypedEnvelope>(projection, cborScratch.first(measured.RequiredBytes));
        if (!encoded.IsSuccessful() ||
            encoded.BytesWritten != measured.RequiredBytes) {
            result.Status = GeneratedEdlsPublishStatus::CborRejected;
            return result;
        }
        result.CborBytes = encoded.BytesWritten;
        std::size_t framed = 0U;
        if (WriteEdlgFrame(
                EdlgCodec::Cbor,
                std::span<const std::byte>(cborScratch.data(), result.CborBytes),
                edlgScratch,
                framed,
                TMaximumPayload,
                byteOperations
            ) != EdlgFrameWriteResult::Succeeded) {
            result.Status = GeneratedEdlsPublishStatus::FramingRejected;
            return result;
        }
        result.EdlgBytes = framed;
        result.Archive = PublishEdlsProtectedRollingSuccessorWithScratch<
            TPhysicalSlots,
            TProtectedSlots,
            TMaximumPayload,
            TMaximumSegment, TStorage, TByteOperationsProvider>(storage, paths, deviceIdentifier, runtimeIncarnation, std::span<const std::byte>(edlgScratch.data(), framed), archiveScratch);
        result.Status = (result.Archive.Status == EdlsProtectedRollingStatus::PublishedWithinCapacity ||
                         result.Archive.Status == EdlsProtectedRollingStatus::PublishedAndRetired)
                            ? GeneratedEdlsPublishStatus::Published
                            : GeneratedEdlsPublishStatus::ArchiveRejected;
        return result;
    }

    /// Publish with a fixed stack frame buffer. ESP32 workers with
    /// high payload bounds can use WithScratch with one
    /// Bootstrap-owned scratch separate from their EDLG frame buffer. Once
    /// framing completes, the former CBOR buffer may be reused as archive
    /// scratch if it has the full EdlsHeaderBytes+maximumPayload+20 capacity.
    /// @tparam TMessageSchema Generated descriptor governing identity and argument kinds.
    /// @tparam TProtectedSlots Number of complete retained segments.
    /// @tparam TMaximumPayload Maximum bytes in one EDLG frame payload.
    /// @tparam TMaximumSegment Maximum bytes in one complete EDLS segment.
    /// @tparam TWidth Selected bit width of the source attempt sequence.
    /// @tparam TParameters Exact ordered generated argument types.
    /// @tparam TMaximumArguments Descriptor capacity of the source arena.
    /// @tparam TArgumentBytes Byte capacity of the source arena.
    /// @tparam TPhysicalSlots Configured physical paths including the successor reserve.
    /// @tparam TStorage Bootstrap-selected FileStorage owner.
    /// @tparam TByteOperationsProvider Stateless Memory copy owner for frame and segment assembly.
    template <
        class TMessageSchema,
        std::size_t TProtectedSlots,
        std::size_t TMaximumPayload,
        std::size_t TMaximumSegment,
        AttemptSequenceWidth TWidth,
        class... TParameters,
        std::size_t TMaximumArguments,
        std::size_t TArgumentBytes,
        std::size_t TPhysicalSlots,
        class TStorage,
        class TByteOperationsProvider = ESPressio::Platform::Portable::Memory::ByteOperationsProvider>
    [[nodiscard]] GeneratedEdlsPublishOutcome<TPhysicalSlots>
    PublishGeneratedLogRecordToProtectedEdls(
        const BasicLogRecord<TWidth, TMaximumArguments, TArgumentBytes> &record,
        GeneratedLogRecordWireProjection<TWidth, TParameters...> &projection,
        OwnedPositionalScratch<TParameters...> &positionalScratch,
        std::span<std::byte> cborScratch,
        std::span<std::byte> edlgScratch,
        TStorage &storage,
        const std::array<ESPressio::Persistence::FilePathView,
                         TPhysicalSlots> &paths,
        const std::array<std::byte, 16U> &deviceIdentifier,
        std::uint32_t runtimeIncarnation) noexcept {
        std::array<std::byte, EdlsHeaderBytes + TMaximumPayload + 20U> archiveScratch{};
        return PublishGeneratedLogRecordToProtectedEdlsWithScratch<
            TMessageSchema,
            TProtectedSlots,
            TMaximumPayload,
            TMaximumSegment>(record, projection, positionalScratch, cborScratch, edlgScratch, storage, paths, deviceIdentifier, runtimeIncarnation, archiveScratch, TByteOperationsProvider{});
    }

} // namespace ESPressio::Logging
