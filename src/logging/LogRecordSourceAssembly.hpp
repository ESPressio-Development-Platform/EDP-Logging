#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "DiagnosticArgumentEncoding.hpp"
#include "LogRecord.hpp"
#include "LogTemporalEvidence.hpp"

namespace ESPressio::Logging {

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
    /// This initial source-assembly profile supports only exact-width
    /// arithmetic parameters. Bounded text/byte and strong-value arguments
    /// require the separate owner-certified owned-capture adapter, and are
    /// deliberately compile-time-rejected here rather than cast or guessed.
    /// Fields 11 and 14 remain UNISSUED source-only zero placeholders;
    /// successfully preparing this struct NEVER authorises wire emission.
    ///
    /// @tparam TMessageSchema The generated declared source-message schema,
    ///         including its same-family owning diagnostic fingerprint.
    /// @tparam TWidth Bootstrap-selected attempt width.
    /// @tparam TMaximumArguments Maximum owned positional argument count.
    /// @tparam TArgumentBytes Maximum owned argument bytes.
    /// @tparam TValues Exact fixed-width scalar source argument Types.
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
            ((std::is_same_v<std::remove_cv_t<TValues>, bool> ||
              std::is_same_v<std::remove_cv_t<TValues>, std::int8_t> ||
              std::is_same_v<std::remove_cv_t<TValues>, std::uint8_t> ||
              std::is_same_v<std::remove_cv_t<TValues>, std::int16_t> ||
              std::is_same_v<std::remove_cv_t<TValues>, std::uint16_t> ||
              std::is_same_v<std::remove_cv_t<TValues>, std::int32_t> ||
              std::is_same_v<std::remove_cv_t<TValues>, std::uint32_t> ||
              std::is_same_v<std::remove_cv_t<TValues>, std::int64_t> ||
              std::is_same_v<std::remove_cv_t<TValues>, std::uint64_t> ||
              std::is_same_v<std::remove_cv_t<TValues>, float> ||
              std::is_same_v<std::remove_cv_t<TValues>, double>) && ...),
            "LogRecord source assembly accepts only owner-captured exact-width scalar values"
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
            candidate.Arguments.AddScalar(values)...
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
