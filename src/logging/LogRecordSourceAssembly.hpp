#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include "DiagnosticArgumentEncoding.hpp"
#include "LogRecord.hpp"
#include "LogTemporalEvidence.hpp"

namespace ESPressio::Logging {

    namespace Detail {

        /// Supported owner-backed capture vocabulary, independent of CBOR tags.
        /// @tparam TValue Exact source argument Type.
        template<class TValue>
        consteval bool IsOwnedSourceCaptureType() {
            using Value = std::remove_cv_t<TValue>;
            return
                std::is_same_v<Value, bool> ||
                std::is_same_v<Value, std::int8_t> ||
                std::is_same_v<Value, std::uint8_t> ||
                std::is_same_v<Value, std::int16_t> ||
                std::is_same_v<Value, std::uint16_t> ||
                std::is_same_v<Value, std::int32_t> ||
                std::is_same_v<Value, std::uint32_t> ||
                std::is_same_v<Value, std::int64_t> ||
                std::is_same_v<Value, std::uint64_t> ||
                std::is_same_v<Value, float> ||
                std::is_same_v<Value, double> ||
                IsBoundedDiagnosticString<Value>::value ||
                IsBoundedDiagnosticBytes<Value>::value ||
                std::is_same_v<Value, System::TypeIdentifier> ||
                std::is_same_v<Value, System::FieldIdentifier> ||
                std::is_same_v<Value, System::Identity::DeviceIdentifier> ||
                std::is_same_v<Value, System::Identity::RuntimeIncarnationId> ||
                IsCertifiedDiagnosticClockEra<Value>::value;
        }

        /// Copies one exact generated-kind value via its existing owned arena.
        /// Never converts a semantic strong value to a generic integer kind.
        /// @tparam TOwned Existing bounded Logging-owned argument arena.
        /// @tparam TValue Supported owner-backed Type.
        /// @param owned Caller scratch destination.
        /// @param value Source borrowed only during this operation.
        /// @returns Typed bounded copy result.
        template<class TOwned, class TValue>
        [[nodiscard]] OwnedArgumentResult CaptureOwnedSourceArgument(
            TOwned& owned,
            const TValue& value
        ) noexcept {
            using Value = std::remove_cv_t<TValue>;
            static_assert(IsOwnedSourceCaptureType<Value>());
            if constexpr (IsBoundedDiagnosticString<Value>::value) {
                return owned.AddUtf8(value.View());
            } else if constexpr (IsBoundedDiagnosticBytes<Value>::value) {
                return owned.AddBytes(
                    std::as_bytes(std::span<const std::uint8_t>(
                        value.Data(),
                        value.Size()
                    ))
                );
            } else if constexpr (std::is_same_v<Value, System::TypeIdentifier>) {
                return owned.AddTypeIdentifier(value);
            } else if constexpr (std::is_same_v<Value, System::FieldIdentifier>) {
                return owned.AddFieldIdentifier(value);
            } else if constexpr (
                std::is_same_v<Value, System::Identity::DeviceIdentifier>
            ) {
                return owned.AddDeviceIdentifier(value);
            } else if constexpr (
                std::is_same_v<Value, System::Identity::RuntimeIncarnationId>
            ) {
                return owned.AddRuntimeIncarnationId(value);
            } else if constexpr (
                IsCertifiedDiagnosticClockEra<Value>::value
            ) {
                return owned.AddClockEra(value);
            } else {
                return owned.AddScalar(value);
            }
        }

    } // ESPressio::Logging::Detail


    /// Result of preparing an owned structural record before capacity admission.
    /// No outcome promises serialized bytes, queue publication or durable storage.
    enum class LogRecordAssemblyStatus : std::uint8_t {
        Prepared = 0U,
        OutputOccupied = 1U,
        InvalidTemporalEvidence = 2U,
        InvalidSourceRecord = 3U,
        ArgumentCountExceeded = 4U,
        ArgumentBytesExceeded = 5U,
        InvalidOwnedArgument = 6U
    };


    /// Prepares a structurally complete, source-owned, canonical LogRecord.
    ///
    /// Localisation-generated descriptor identity (rather than a caller-supplied
    /// numeric identifier) binds the source String to the checked signature.
    /// The full Localisation-generated diagnostic fingerprint is captured,
    /// distinct from translated ContractFamilyFingerprint.
    ///
    /// This is the ordinary-context PRE-ADMISSION source stage: Bootstrap
    /// captures a genuinely coherent Clock observation and issues a ticket
    /// before calling; the caller must do so before bounded queue waits.
    /// An invalid or exhausted input leaves output untouched. No new timer,
    /// allocator, codec or platform provider is introduced.
    ///
    /// Supports exact-width arithmetic, bounded Utf8/Bytes, TypeIdentifier,
    /// FieldIdentifier, DeviceIdentifier, RuntimeIncarnationId and the
    /// explicitly opted-in ClockEra via existing owner-backed capture
    /// facilities. Other strong types remain compile-time rejected until
    /// a semantically exact owned capture adapter is added.
    /// Fields 11/14 have approved child vocabularies. This operation
    /// conservatively initializes Clock provenance as CorrelationUnavailable
    /// and Unreliable, and V1 record facts as zero; it does not manufacture
    /// a Clock projection or quality assessment. Full canonical record wire
    /// emission remains gated on a complete owner-qualified serializer.
    ///
    /// @tparam TMessageSchema The generated declared source-message schema,
    ///         including its same-family owning diagnostic fingerprint.
    /// @tparam TWidth Bootstrap-selected attempt width.
    /// @tparam TMaximumArguments Maximum owned positional argument count.
    /// @tparam TArgumentBytes Maximum owned argument bytes.
    /// @tparam TValues Owner-certified, exact generated source argument Types.
    /// @param output Unoccupied source slot, mutated only on complete success.
    /// @param severity One emitted severity, never Off.
    /// @param device Authoritative source DeviceIdentifier.
    /// @param incarnation Source's RuntimeIncarnationId.
    /// @param attempt Already-issued nonzero unique attempt ticket.
    /// @param temporal One previously captured and qualified Clock observation.
    /// @param values Exactly the generated schema's positional arguments.
    /// @returns Typed source preparation result, not a queue admission.
    template<
        class TMessageSchema,
        AttemptSequenceWidth TWidth,
        std::size_t TMaximumArguments,
        std::size_t TArgumentBytes,
        class... TValues
    >
    [[nodiscard]] LogRecordAssemblyStatus AssembleLogRecordSource(
        BasicLogRecord<TWidth, TMaximumArguments, TArgumentBytes>& output,
        Severity severity,
        System::Identity::DeviceIdentifier device,
        System::Identity::RuntimeIncarnationId incarnation,
        AttemptSequence<TWidth> attempt,
        const QualifiedLogTime& temporal,
        const TValues&... values
    ) noexcept {
        static_assert(TMessageSchema::Declared,
            "LogRecord requires one declared, generated EDP-Localisation message schema");
        static_assert(
            DiagnosticSchemaMatches<TMessageSchema, TValues...>(),
            "LogRecord arguments must exactly match the generated Localisation diagnostic schema"
        );
        static_assert(
            (Detail::IsOwnedSourceCaptureType<TValues>() && ...),
            "LogRecord source assembly needs exact owner-backed positional capture"
        );
        static_assert(
            TMessageSchema::OwningContract::DiagnosticSchemaFingerprint.size() == 16U,
            "EDP-Localisation generated diagnostic fingerprint must be 16 bytes"
        );
        static_assert(
            TMessageSchema::ArgumentCount <= TMaximumArguments,
            "Generated message argument count exceeds the chosen record resource capacity"
        );

        if (output.Attempt.IsValid() || output.Device.IsValid() ||
            output.Arguments.Count() != 0U) {
            return LogRecordAssemblyStatus::OutputOccupied;
        }
        if (temporal.Status == LogTemporalEvidenceStatus::Incoherent) {
            return LogRecordAssemblyStatus::InvalidTemporalEvidence;
        }
        const bool synchronized =
            temporal.Status == LogTemporalEvidenceStatus::Synchronized ||
            temporal.Status == LogTemporalEvidenceStatus::Degraded;
        if (
            (!synchronized &&
             (temporal.Era.IsValid() || !temporal.Uncertainty.IsSaturated())) ||
            (temporal.Status == LogTemporalEvidenceStatus::Synchronized &&
             temporal.ClockState != Clock::SynchronizationState::Synchronized) ||
            (temporal.Status == LogTemporalEvidenceStatus::Degraded &&
             temporal.ClockState != Clock::SynchronizationState::Holdover &&
             temporal.ClockState != Clock::SynchronizationState::Reacquiring) ||
            (temporal.Status == LogTemporalEvidenceStatus::Unavailable &&
             temporal.ClockState != Clock::SynchronizationState::NeverSynchronized &&
             temporal.ClockState != Clock::SynchronizationState::LostSynchronization)
        ) {
            return LogRecordAssemblyStatus::InvalidTemporalEvidence;
        }

        // Construct and validate entirely in independent caller stack scratch.
        // Nothing in output changes on failed argument capture/validation.
        BasicLogRecord<TWidth, TMaximumArguments, TArgumentBytes> candidate{};
        candidate.Message = {
            TMessageSchema::SourceDomain,
            TMessageSchema::SourceSubDomain,
            TMessageSchema::SourceString
        };
        candidate.Level = severity;
        candidate.Device = device;
        candidate.Incarnation = incarnation;
        candidate.Attempt = attempt;
        candidate.Monotonic = temporal.Monotonic;
        candidate.SyncState = temporal.ClockState;
        // Field 11 is copied from the SAME owner-qualified temporal sample.
        // No Logging-local inference of Clock projection or policy quality.
        candidate.Provenance = {temporal.ProjectionStatus, temporal.Assessment};
        if (synchronized) {
            candidate.Synchronized = temporal.Synchronized;
            candidate.Era = temporal.Era;
            candidate.Uncertainty = temporal.Uncertainty;
        } else {
            candidate.Uncertainty = Clock::SynchronizationUncertainty::Maximum();
        }
        candidate.DiagnosticSchemaFingerprint =
            TMessageSchema::OwningContract::DiagnosticSchemaFingerprint;

        const std::array<OwnedArgumentResult, sizeof...(TValues)> captures{
            Detail::CaptureOwnedSourceArgument(candidate.Arguments, values)...
        };
        for (const auto result : captures) {
            if (result == OwnedArgumentResult::CountExceeded) {
                return LogRecordAssemblyStatus::ArgumentCountExceeded;
            }
            if (result == OwnedArgumentResult::BytesExceeded) {
                return LogRecordAssemblyStatus::ArgumentBytesExceeded;
            }
            if (result != OwnedArgumentResult::Added) {
                return LogRecordAssemblyStatus::InvalidOwnedArgument;
            }
        }
        if (ValidateLogRecord(candidate) != LogRecordValidation::Valid) {
            return LogRecordAssemblyStatus::InvalidSourceRecord;
        }

        output = candidate;
        return LogRecordAssemblyStatus::Prepared;
    }

} // ESPressio::Logging
