#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <ESPressio_System.hpp>
#include <ESPressio_Serialisation.hpp>
#include <ESPressio_BoundedTypes.hpp>

#include "LogRecordCoreFieldSerialisation.hpp"
#include "OwnedDiagnosticPositionalReencoding.hpp"

namespace ESPressio::Logging {

    /// Disposition of the source-to-generated wire projection.
    enum class GeneratedLogRecordProjectionStatus : std::uint8_t {
        /// All governed fields and positional arguments were projected.
        Succeeded = 0U,
        /// The source record failed its own structural validation.
        InvalidSource = 1U,
        /// The record does not identify the selected generated diagnostic.
        GeneratedMessageMismatch = 2U,
        /// The source fingerprint differs from the generated argument schema.
        DiagnosticFingerprintMismatch = 3U,
        /// The owned arena count differs from the generated argument list.
        ArgumentCountMismatch = 4U,
        /// Source reconstruction or owner conversion rejected an argument.
        ArgumentRejected = 5U
    };

    // Schema-bound wire projection of the EXISTING permanent LogRecord.
    // This is not a second semantic TypeIdentifier. Its Field 13 is the
    // generated message's exact ordered positional values, not private
    // source Kind/Offset/Size metadata.
    namespace Detail {

        /// Typed result of adapting a generated owner value for wire use.
        enum class GeneratedValueConversionResult : std::uint8_t {

            /// The canonical wire value is ready.
            Succeeded = 0U,

            /// The owning conversion adapter rejected its semantic input.
            OwnerRejected = 1U

        };

        /// Detects an owner-defined canonical representation.
        /// @tparam TValue Exact generated argument type.
        template <class TValue>
        concept HasOwnerCanonicalRepresentation = requires {
            typename Serialisation::CanonicalRepresentation<TValue>::Type;
        };

        /// Uses the original type when the owner defines no alternate representation.
        /// @tparam TValue Exact generated argument type.
        /// @tparam THasCanonical Owner representation availability, deduced by default.
        template <class TValue, bool THasCanonical = HasOwnerCanonicalRepresentation<TValue>>
        struct GeneratedPositionalWireType final {
            /// Wire value type used unchanged from the generated argument.
            using Type = TValue;
        };

        /// Selects the existing owner canonical representation.
        /// @tparam TValue Exact generated argument with an owner representation.
        template <class TValue>
        struct GeneratedPositionalWireType<TValue, true> final {
            /// Owner-defined wire value type for the generated argument.
            using Type = typename Serialisation::CanonicalRepresentation<TValue>::Type;
        };

        /// Resolves a generated argument to its selected wire value type.
        /// @tparam TValue Exact generated argument type.
        template <class TValue>
        using GeneratedPositionalWireTypeOf =
            typename GeneratedPositionalWireType<TValue>::Type;

        /// Converts an owner-qualified semantic value to a canonical wire value.
        /// @tparam TValue Source semantic value Type.
        /// @param semantic Caller-provided generated-schema semantic value.
        /// @param wire Existing owner wire output.
        /// @returns Succeeded or OwnerRejected; never a Boolean operational result.
        template <class TValue>
        [[nodiscard]] GeneratedValueConversionResult ConvertGeneratedValue(
            const TValue &semantic,
            GeneratedPositionalWireTypeOf<TValue> &wire) noexcept {
            if constexpr (HasOwnerCanonicalRepresentation<TValue>) {
                using Wire = GeneratedPositionalWireTypeOf<TValue>;
                const auto status =
                    Bounded::TypeConversionAdapter<TValue, Wire>::Convert(
                        semantic,
                        wire
                    );
                return Bounded::IsTypeConversionSuccessful<TValue, Wire>(status)
                           ? GeneratedValueConversionResult::Succeeded
                           : GeneratedValueConversionResult::OwnerRejected;
            } else {
                wire = semantic;
                return GeneratedValueConversionResult::Succeeded;
            }
        }

    } // namespace Detail

    /// Fixed wire view of the existing fifteen-field governed LogRecord.
    /// @tparam TWidth Selected bit width of the source attempt sequence.
    /// @tparam TParameters Exact ordered semantic types from the generated schema.
    template <AttemptSequenceWidth TWidth, class... TParameters>
    struct GeneratedLogRecordWireProjection final {
        /// Permanent System identity reused from the source record.
        inline static constexpr System::TypeIdentifier Identifier =
            BasicLogRecord<TWidth>::Identifier;

        /// Field0 generated diagnostic message identifier.
        LogMessageIdentifier Message{};
        /// Field1 severity selected at the logging site.
        Severity Level{Severity::Info};
        /// Field2 identity of the emitting device.
        System::Identity::DeviceIdentifier Device{};
        /// Field3 nonzero runtime incarnation of the emitter.
        System::Identity::RuntimeIncarnationId Incarnation{};
        /// Field4 attempt identity issued within that incarnation.
        AttemptSequence<TWidth> Attempt{};
        /// Field5 physical bit width of the attempt sequence.
        std::uint8_t SequenceWidth{static_cast<std::uint8_t>(TWidth)};
        /// Field6 source monotonic occurrence time.
        Clock::MonotonicTimestamp Monotonic{};
        /// Field7 optional synchronized coordinate from the same observation.
        std::optional<Clock::SynchronizedTimestamp> Synchronized{};
        /// Field8 synchronization state observed at capture.
        Clock::SynchronizationState SyncState{
            Clock::SynchronizationState::NeverSynchronized};
        /// Field9 clock era associated with the captured evidence.
        Clock::ClockEra Era{};
        /// Field10 owner-reported uncertainty for the synchronized coordinate.
        Clock::SynchronizationUncertainty Uncertainty{};
        /// Field11 provenance of the captured temporal evidence.
        LogTemporalProvenance Provenance{};
        /// Field12 exact generated diagnostic schema fingerprint.
        std::array<std::uint8_t, 16U> DiagnosticSchemaFingerprint{};
        /// Field13 ordered canonical owner values.
        std::tuple<Detail::GeneratedPositionalWireTypeOf<TParameters>...> Arguments{};
        /// Field14 stable record facts preserved from the source.
        LogRecordFacts Facts{};

        // The exact fifteen issued FieldIds, preserving the source
        // schema's numbering and root identity; only the Field13 source
        // storage changes to generated typed positional values.
        /// The exact ordered System FieldId bindings used by the owner codecs.
        using Fields = System::FieldSet<
            System::FieldBinding<&GeneratedLogRecordWireProjection::Message, 0U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Level, 1U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Device, 2U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Incarnation, 3U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Attempt, 4U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::SequenceWidth, 5U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Monotonic, 6U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Synchronized, 7U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::SyncState, 8U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Era, 9U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Uncertainty, 10U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Provenance, 11U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::DiagnosticSchemaFingerprint, 12U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Arguments, 13U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Facts, 14U>>;
    };

    /// Source-to-wire result and source rejection position where available.
    struct GeneratedLogRecordProjectionOutcome final {
        /// Overall projection disposition.
        GeneratedLogRecordProjectionStatus Status{
            GeneratedLogRecordProjectionStatus::InvalidSource};
        /// Source count on arity mismatch or zero-based index on argument refusal.
        std::size_t FirstFailedArgument{0U};
        /// Confirms that a complete governed projection is available.
        [[nodiscard]] bool IsSuccessful() const noexcept {
            return Status == GeneratedLogRecordProjectionStatus::Succeeded;
        }
    };

    /// Validate source evidence, reconstruct ordered values and fill the wire projection.
    /// @tparam TMessageSchema Generated descriptor governing identity and argument kinds.
    /// @tparam TWidth Selected bit width of the source attempt sequence.
    /// @tparam TParameters Exact ordered semantic types from the generated schema.
    /// @tparam N Maximum descriptors in the source arena.
    /// @tparam B Byte capacity of the source arena.
    /// @param source Record and owned argument bytes to validate.
    /// @param output Caller-owned projection replaced only on success.
    /// @param scratch Exclusive caller-owned positional workspace cleared on return.
    template <
        class TMessageSchema,
        AttemptSequenceWidth TWidth,
        class... TParameters,
        std::size_t N,
        std::size_t B>
    [[nodiscard]] GeneratedLogRecordProjectionOutcome
    ProjectGeneratedLogRecord(
        const BasicLogRecord<TWidth, N, B> &source,
        GeneratedLogRecordWireProjection<TWidth, TParameters...> &output,
        OwnedPositionalScratch<TParameters...> &scratch) noexcept {
        static_assert(
            IsDiagnosticSchemaMatching<TMessageSchema, TParameters...>(),
            "Projection must use exact owner-generated EDP-Localisation types"
        );
        using Projection = GeneratedLogRecordWireProjection<TWidth, TParameters...>;
        static_assert(
            System::SchemaType<Projection> &&
                System::IdentifiedType<Projection> &&
                Serialisation::SerialisableType<Projection>,
            "All fifteen canonical Fields must qualify under owner Serialisation"
        );

        const auto clear = [&]() noexcept {
            std::apply(
                [](auto &...item) noexcept {
                    (item.reset(), ...);
                },
                scratch
            );
        };
        clear();
        if (ValidateLogRecord(source) != LogRecordValidation::Valid)
            return {GeneratedLogRecordProjectionStatus::InvalidSource, 0U};
        if (source.Message.Domain != TMessageSchema::SourceDomain ||
            source.Message.SubDomain != TMessageSchema::SourceSubDomain ||
            source.Message.String != TMessageSchema::SourceString)
            return {GeneratedLogRecordProjectionStatus::GeneratedMessageMismatch, 0U};
        if (source.DiagnosticSchemaFingerprint !=
            TMessageSchema::OwningContract::DiagnosticSchemaFingerprint)
            return {GeneratedLogRecordProjectionStatus::DiagnosticFingerprintMismatch, 0U};
        if (source.Arguments.Count() != sizeof...(TParameters))
            return {
                GeneratedLogRecordProjectionStatus::ArgumentCountMismatch,
                source.Arguments.Count()
            };

        GeneratedLogRecordProjectionOutcome outcome{
            GeneratedLogRecordProjectionStatus::Succeeded,
            0U
        };
        [&]<std::size_t... I>(std::index_sequence<I...>) noexcept {
            (([&]() noexcept {
                 if (outcome.Status != GeneratedLogRecordProjectionStatus::Succeeded) return;
                 const auto status = Detail::LoadOwnedPosition<TParameters>(
                     source.Arguments,
                     I,
                     std::get<I>(scratch)
                 );
                 if (status != OwnedPositionalReencodingStatus::Succeeded) {
                     outcome = {
                         GeneratedLogRecordProjectionStatus::ArgumentRejected,
                         I
                     };
                 }
             }()),
             ...);
        }(std::index_sequence_for<TParameters...>{});
        if (!outcome.IsSuccessful()) {
            clear();
            return outcome;
        }

        Projection candidate{};
        [&]<std::size_t... I>(std::index_sequence<I...>) noexcept {
            (([&]() noexcept {
                 if (!outcome.IsSuccessful()) return;
                 if (Detail::ConvertGeneratedValue<TParameters>(
                     *std::get<I>(scratch),
                     std::get<I>(candidate.Arguments)
                 ) != Detail::GeneratedValueConversionResult::Succeeded) {
                     outcome = {
                         GeneratedLogRecordProjectionStatus::ArgumentRejected,
                         I
                     };
                 }
             }()),
             ...);
        }(std::index_sequence_for<TParameters...>{});
        clear();
        if (!outcome.IsSuccessful()) return outcome;

        candidate.Message = source.Message;
        candidate.Level = source.Level;
        candidate.Device = source.Device;
        candidate.Incarnation = source.Incarnation;
        candidate.Attempt = source.Attempt;
        candidate.SequenceWidth = source.SequenceWidth;
        candidate.Monotonic = source.Monotonic;
        candidate.Synchronized = source.Synchronized;
        candidate.SyncState = source.SyncState;
        candidate.Era = source.Era;
        candidate.Uncertainty = source.Uncertainty;
        candidate.Provenance = source.Provenance;
        candidate.DiagnosticSchemaFingerprint = source.DiagnosticSchemaFingerprint;
        candidate.Facts = source.Facts;
        output = std::move(candidate);
        return outcome;
    }

} // namespace ESPressio::Logging
