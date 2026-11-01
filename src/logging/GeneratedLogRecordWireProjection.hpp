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

        template<class TValue>
        concept HasOwnerCanonicalRepresentation = requires {
            typename Serialisation::CanonicalRepresentation<TValue>::Type;
        };

        template<class TValue, bool THasCanonical =
            HasOwnerCanonicalRepresentation<TValue>>
        struct GeneratedPositionalWireType final {
            using Type=TValue;
        };
        template<class TValue>
        struct GeneratedPositionalWireType<TValue,true> final {
            using Type=typename Serialisation::CanonicalRepresentation<TValue>::Type;
        };
        template<class TValue>
        using GeneratedPositionalWireTypeOf =
            typename GeneratedPositionalWireType<TValue>::Type;

        /// Converts an owner-qualified semantic value to a canonical wire value.
        /// @tparam TValue Source semantic value Type.
        /// @param semantic Caller-provided generated-schema semantic value.
        /// @param wire Existing owner wire output.
        /// @returns Succeeded or OwnerRejected; never a Boolean operational result.
        template<class TValue>
        [[nodiscard]] GeneratedValueConversionResult ConvertGeneratedValue(
            const TValue& semantic,
            GeneratedPositionalWireTypeOf<TValue>& wire
        ) noexcept {
            if constexpr (HasOwnerCanonicalRepresentation<TValue>) {
                using Wire=GeneratedPositionalWireTypeOf<TValue>;
                const auto status=
                    Bounded::TypeConversionAdapter<TValue,Wire>::Convert(semantic,wire);
                return Bounded::IsTypeConversionSuccessful<TValue,Wire>(status)
                    ? GeneratedValueConversionResult::Succeeded
                    : GeneratedValueConversionResult::OwnerRejected;
            } else {
                wire=semantic;
                return GeneratedValueConversionResult::Succeeded;
            }
        }
    }

    template<AttemptSequenceWidth TWidth, class... TParameters>
    struct GeneratedLogRecordWireProjection final {
        inline static constexpr System::TypeIdentifier Identifier =
            BasicLogRecord<TWidth>::Identifier;

        LogMessageIdentifier Message{};
        Severity Level{Severity::Info};
        System::Identity::DeviceIdentifier Device{};
        System::Identity::RuntimeIncarnationId Incarnation{};
        AttemptSequence<TWidth> Attempt{};
        std::uint8_t SequenceWidth{static_cast<std::uint8_t>(TWidth)};
        Clock::MonotonicTimestamp Monotonic{};
        std::optional<Clock::SynchronizedTimestamp> Synchronized{};
        Clock::SynchronizationState SyncState{
            Clock::SynchronizationState::NeverSynchronized};
        Clock::ClockEra Era{};
        Clock::SynchronizationUncertainty Uncertainty{};
        LogTemporalProvenance Provenance{};
        std::array<std::uint8_t,16U> DiagnosticSchemaFingerprint{};
        std::tuple<Detail::GeneratedPositionalWireTypeOf<TParameters>...> Arguments{};
        LogRecordFacts Facts{};

        // The exact fifteen issued FieldIds, preserving the source
        // schema's numbering and root identity; only the Field13 source
        // storage changes to generated typed positional values.
        using Fields=System::FieldSet<
            System::FieldBinding<&GeneratedLogRecordWireProjection::Message,0U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Level,1U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Device,2U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Incarnation,3U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Attempt,4U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::SequenceWidth,5U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Monotonic,6U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Synchronized,7U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::SyncState,8U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Era,9U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Uncertainty,10U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Provenance,11U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::DiagnosticSchemaFingerprint,12U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Arguments,13U>,
            System::FieldBinding<&GeneratedLogRecordWireProjection::Facts,14U>
        >;
    };

    enum class GeneratedLogRecordProjectionStatus : std::uint8_t {
        Succeeded = 0U,
        InvalidSource = 1U,
        GeneratedMessageMismatch = 2U,
        DiagnosticFingerprintMismatch = 3U,
        ArgumentCountMismatch = 4U,
        ArgumentRejected = 5U
    };

    struct GeneratedLogRecordProjectionOutcome final {
        GeneratedLogRecordProjectionStatus Status{
            GeneratedLogRecordProjectionStatus::InvalidSource};
        std::size_t FirstFailedArgument{0U};
        [[nodiscard]] bool IsSuccessful() const noexcept {
            return Status==GeneratedLogRecordProjectionStatus::Succeeded;
        }
    };

    template<
        class TMessageSchema, AttemptSequenceWidth TWidth,
        class... TParameters, std::size_t N, std::size_t B
    >
    [[nodiscard]] GeneratedLogRecordProjectionOutcome
    ProjectGeneratedLogRecord(
        const BasicLogRecord<TWidth,N,B>& source,
        GeneratedLogRecordWireProjection<TWidth,TParameters...>& output,
        OwnedPositionalScratch<TParameters...>& scratch
    ) noexcept {
        static_assert(IsDiagnosticSchemaMatching<TMessageSchema,TParameters...>(),
            "Projection must use exact owner-generated EDP-Localisation types");
        using Projection=GeneratedLogRecordWireProjection<TWidth,TParameters...>;
        static_assert(System::SchemaType<Projection> &&
                      System::IdentifiedType<Projection> &&
                      Serialisation::SerialisableType<Projection>,
            "All fifteen canonical Fields must qualify under owner Serialisation");

        const auto clear=[&]() noexcept {
            std::apply([](auto&... item) noexcept { (item.reset(),...); },scratch);
        };
        clear();
        if(ValidateLogRecord(source)!=LogRecordValidation::Valid)
            return {GeneratedLogRecordProjectionStatus::InvalidSource,0U};
        if(source.Message.Domain!=TMessageSchema::SourceDomain ||
           source.Message.SubDomain!=TMessageSchema::SourceSubDomain ||
           source.Message.String!=TMessageSchema::SourceString)
            return {GeneratedLogRecordProjectionStatus::GeneratedMessageMismatch,0U};
        if(source.DiagnosticSchemaFingerprint !=
           TMessageSchema::OwningContract::DiagnosticSchemaFingerprint)
            return {GeneratedLogRecordProjectionStatus::DiagnosticFingerprintMismatch,0U};
        if(source.Arguments.Count()!=sizeof...(TParameters))
            return {GeneratedLogRecordProjectionStatus::ArgumentCountMismatch,source.Arguments.Count()};

        GeneratedLogRecordProjectionOutcome outcome{
            GeneratedLogRecordProjectionStatus::Succeeded,0U};
        [&]<std::size_t... I>(std::index_sequence<I...>) noexcept {
            (([&]() noexcept {
                if(outcome.Status!=GeneratedLogRecordProjectionStatus::Succeeded) return;
                const auto status=Detail::LoadOwnedPosition<TParameters>(
                    source.Arguments,I,std::get<I>(scratch));
                if(status!=OwnedPositionalReencodingStatus::Succeeded) {
                    outcome={GeneratedLogRecordProjectionStatus::ArgumentRejected,I};
                }
            }()),...);
        }(std::index_sequence_for<TParameters...>{});
        if(!outcome.IsSuccessful()) { clear(); return outcome; }

        Projection candidate{};
        [&]<std::size_t... I>(std::index_sequence<I...>) noexcept {
            (([&]() noexcept {
                if(!outcome.IsSuccessful()) return;
                if (Detail::ConvertGeneratedValue<TParameters>(
                    *std::get<I>(scratch),std::get<I>(candidate.Arguments)
                ) != Detail::GeneratedValueConversionResult::Succeeded) {
                    outcome={GeneratedLogRecordProjectionStatus::ArgumentRejected,I};
                }
            }()),...);
        }(std::index_sequence_for<TParameters...>{});
        clear();
        if(!outcome.IsSuccessful()) return outcome;

        candidate.Message=source.Message;
        candidate.Level=source.Level;
        candidate.Device=source.Device;
        candidate.Incarnation=source.Incarnation;
        candidate.Attempt=source.Attempt;
        candidate.SequenceWidth=source.SequenceWidth;
        candidate.Monotonic=source.Monotonic;
        candidate.Synchronized=source.Synchronized;
        candidate.SyncState=source.SyncState;
        candidate.Era=source.Era;
        candidate.Uncertainty=source.Uncertainty;
        candidate.Provenance=source.Provenance;
        candidate.DiagnosticSchemaFingerprint=source.DiagnosticSchemaFingerprint;
        candidate.Facts=source.Facts;
        output=std::move(candidate);
        return outcome;
    }
}
