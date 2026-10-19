#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <ESPressio_System.hpp>
#include <clock/ClockEra.hpp>
#include <clock/ClockCorrelationProjectionStatus.hpp>
#include <clock/TemporalQualityAssessment.hpp>
#include <clock/MonotonicTimestamp.hpp>
#include <clock/SynchronizationState.hpp>
#include <clock/SynchronizationUncertainty.hpp>
#include <clock/SynchronizedTimestamp.hpp>
#include <localisation/LocalisationTypes.hpp>

#include "AttemptSequence.hpp"
#include "OwnedDiagnosticArguments.hpp"
#include "Severity.hpp"

namespace ESPressio::Logging {

    /// Canonically widened in-memory representation of one generated
    /// EDP-Localisation GeneralStringIdentifier. No new identifier namespace.
    struct LogMessageIdentifier final {

        // Owner-issued Localisation identity components.

        /// Source Domain identifier, preserved without reducing owner width.
        std::uint32_t Domain{0U};

        /// Source SubDomain identifier, preserved without reducing owner width.
        std::uint32_t SubDomain{0U};

        /// Source String identifier, preserved without reducing owner width.
        std::uint32_t String{0U};


        // Source-generated Localisation import.

        /// Copies a generated strongly typed Localisation identity.
        /// @tparam TIdentifiers Generated owner ContractIdentifiers family.
        /// @param source Concrete generated GeneralStringIdentifier.
        /// @returns Exactly corresponding numeric Domain/SubDomain/String components.
        template<class TIdentifiers>
        [[nodiscard]] static constexpr LogMessageIdentifier From(
            const typename TIdentifiers::GeneralStringIdentifier& source
        ) noexcept {
            return {
                static_cast<std::uint32_t>(source.Domain.Value()),
                static_cast<std::uint32_t>(source.SubDomain.Value()),
                static_cast<std::uint32_t>(source.String.Value())
            };
        }

    };


    /// Approved Field 11: two strongly typed EDP-Clock owner statuses.
    /// V1 wire child pair: [Projection, Quality] as owner enum uint8 values.
    struct LogTemporalProvenance final {
        Clock::ClockCorrelationProjectionStatus Projection{
            Clock::ClockCorrelationProjectionStatus::CorrelationUnavailable
        };
        Clock::TemporalQualityAssessment Quality{
            Clock::TemporalQualityAssessment::Unreliable
        };
    };

    /// Approved Field 14: V1 unsigned byte, zero only. All other values
    /// are reserved and invalid for both encoding and decoding.
    struct LogRecordFacts final {
        std::uint8_t Value{0U};
    };


    /// Structural canonical LogRecord, with finite source-selected capacities.
    ///
    /// This satisfies System::SchemaType and binds all fifteen approved field
    /// identities. Fields 11/14 have governed child vocabularies. The
    /// complete 15-field serialisable envelope is still independently gated.
    ///
    /// @tparam TWidth Bootstrap-selected width of the per-incarnation sequence.
    /// @tparam TMaximumArguments Maximum owned positional argument count.
    /// @tparam TArgumentBytes Maximum aggregate owned positional argument bytes.
    template<
        AttemptSequenceWidth TWidth = DefaultAttemptSequenceWidth,
        std::size_t TMaximumArguments = 8U,
        std::size_t TArgumentBytes = 128U
    >
    struct BasicLogRecord final {

        // Permanent semantic Type and approved field identity metadata.

        /// One permanent EDP-System canonical 24/40-bit Type identity.
        inline static constexpr System::TypeIdentifier Identifier{
            System::TypeIdentifier::Storage{
                0x00U, 0x00U, 0x01U,
                0x00U, 0x00U, 0x00U, 0x00U, 0x01U
            }
        };


        // Identity and source timeline fields.

        /// Field 0: generated Localisation message identity.
        LogMessageIdentifier Message{};

        /// Field 1: one emitted severity (never SeverityThreshold::Off).
        Severity Level{Severity::Info};

        /// Field 2: permanent source device identity.
        System::Identity::DeviceIdentifier Device{};

        /// Field 3: durable source runtime incarnation.
        System::Identity::RuntimeIncarnationId Incarnation{};

        /// Field 4: nonzero per-incarnation instrumentation attempt ticket.
        AttemptSequence<TWidth> Attempt{};

        /// Field 5: exact selected sequence width, 16/32/64 bits.
        std::uint8_t SequenceWidth{static_cast<std::uint8_t>(TWidth)};

        /// Field 6: source's canonical monotonic nanosecond coordinate.
        Clock::MonotonicTimestamp Monotonic{};

        /// Field 7: optional synchronized coordinate (zero is valid when present).
        std::optional<Clock::SynchronizedTimestamp> Synchronized{};

        /// Field 8: synchronized source quality/availability state.
        Clock::SynchronizationState SyncState{
            Clock::SynchronizationState::NeverSynchronized
        };

        /// Field 9: upstream-owned nonzero synchronized ClockEra, when present.
        Clock::ClockEra Era{};

        /// Field 10: conservative synchronized-time nanosecond uncertainty.
        Clock::SynchronizationUncertainty Uncertainty{};


        // Provenance, generated diagnostic schema and record fact fields.

        /// Field 11: two typed EDP-Clock statuses in fixed order.
        LogTemporalProvenance Provenance{};

        /// Field 12: independent 16-byte diagnostic schema fingerprint.
        std::array<std::uint8_t, 16U> DiagnosticSchemaFingerprint{};

        /// Field 13: fully owned, bounded, heterogeneous positional arguments.
        OwnedDiagnosticArguments<TMaximumArguments, TArgumentBytes> Arguments{};

        /// Field 14: zero-only uint8, nonzero values are reserved.
        LogRecordFacts Facts{};


        // Approved numbered EDP-System Field binding.

        /// Exactly fifteen compile-time unique, Type-local FieldIdentifiers.
        using Fields = System::FieldSet<
            System::FieldBinding<&BasicLogRecord::Message, 0U>,
            System::FieldBinding<&BasicLogRecord::Level, 1U>,
            System::FieldBinding<&BasicLogRecord::Device, 2U>,
            System::FieldBinding<&BasicLogRecord::Incarnation, 3U>,
            System::FieldBinding<&BasicLogRecord::Attempt, 4U>,
            System::FieldBinding<&BasicLogRecord::SequenceWidth, 5U>,
            System::FieldBinding<&BasicLogRecord::Monotonic, 6U>,
            System::FieldBinding<&BasicLogRecord::Synchronized, 7U>,
            System::FieldBinding<&BasicLogRecord::SyncState, 8U>,
            System::FieldBinding<&BasicLogRecord::Era, 9U>,
            System::FieldBinding<&BasicLogRecord::Uncertainty, 10U>,
            System::FieldBinding<&BasicLogRecord::Provenance, 11U>,
            System::FieldBinding<&BasicLogRecord::DiagnosticSchemaFingerprint, 12U>,
            System::FieldBinding<&BasicLogRecord::Arguments, 13U>,
            System::FieldBinding<&BasicLogRecord::Facts, 14U>
        >;

    };


    /// Default 32-bit-sequence, 8-argument, 128-byte structural LogRecord.
    using LogRecord = BasicLogRecord<>;

    static_assert(System::SchemaType<LogRecord>);
    static_assert(System::SchemaType<BasicLogRecord<AttemptSequenceWidth::Bits16, 4U, 64U>>);
    static_assert(System::SchemaType<BasicLogRecord<AttemptSequenceWidth::Bits64, 16U, 512U>>);
    static_assert(System::TypeIdentifierOf<LogRecord> ==
                  System::TypeIdentifierOf<BasicLogRecord<AttemptSequenceWidth::Bits64>>);
    static_assert(LogRecord::Fields::Count == 15U);


    /// Outcome of validating source identity, record site and timestamp facts.
    enum class LogRecordValidation : std::uint8_t {
        Valid = 0U,
        InvalidDevice = 1U,
        InvalidIncarnation = 2U,
        InvalidAttempt = 3U,
        InvalidSequenceWidth = 4U,
        InvalidSyncEvidence = 5U,
        InvalidSeverity = 6U,
        InvalidMessage = 7U,
        InvalidTemporalProvenance = 8U,
        ReservedRecordFacts = 9U
    };


    /// Checks only source-semantic facts independent of serialisation or keys.
    /// Does NOT authenticate source, schema or record publisher.
    /// @tparam TWidth Bootstrap-selected attempt width.
    /// @tparam TMaximumArguments Finite positional argument capacity.
    /// @tparam TArgumentBytes Finite owned argument byte capacity.
    /// @param record Structural source record to validate.
    /// @returns Strongly typed validation result; never mutates the record.
    template<
        AttemptSequenceWidth TWidth,
        std::size_t TMaximumArguments,
        std::size_t TArgumentBytes
    >
    [[nodiscard]] constexpr LogRecordValidation ValidateLogRecord(
        const BasicLogRecord<TWidth, TMaximumArguments, TArgumentBytes>& record
    ) noexcept {
        if (!record.Device.IsValid()) {
            return LogRecordValidation::InvalidDevice;
        }
        if (!record.Incarnation.IsValid()) {
            return LogRecordValidation::InvalidIncarnation;
        }
        if (!record.Attempt.IsValid()) {
            return LogRecordValidation::InvalidAttempt;
        }
        if (record.SequenceWidth != static_cast<std::uint8_t>(TWidth)) {
            return LogRecordValidation::InvalidSequenceWidth;
        }
        if (static_cast<std::uint8_t>(record.Level) >
            static_cast<std::uint8_t>(Severity::Fatal)) {
            return LogRecordValidation::InvalidSeverity;
        }
        // Localisation String IDs 0..2 are reserved metadata identities.
        if (record.Message.String < 3U) {
            return LogRecordValidation::InvalidMessage;
        }
        if (static_cast<std::uint8_t>(record.SyncState) >
            static_cast<std::uint8_t>(
                Clock::SynchronizationState::LostSynchronization
            )) {
            return LogRecordValidation::InvalidSyncEvidence;
        }

        // A synchronized coordinate requires an upstream-issued Era and
        // an eligible synchronization state. Zero is a valid coordinate;
        // optional presence, never numerical value, is the discriminator.
        const auto hasCoordinate = record.Synchronized.has_value();
        const auto hasEra = record.Era.IsValid();
        const auto eligible =
            record.SyncState == Clock::SynchronizationState::Synchronized ||
            record.SyncState == Clock::SynchronizationState::Holdover ||
            record.SyncState == Clock::SynchronizationState::Reacquiring;

        if (
            (hasCoordinate && (!hasEra || !eligible)) ||
            (!hasCoordinate && hasEra) ||
            (!hasCoordinate && record.SyncState ==
                 Clock::SynchronizationState::Synchronized) ||
            (!hasCoordinate && !record.Uncertainty.IsSaturated())
        ) {
            // A missing synchronized coordinate cannot claim precise
            // uncertainty; preserve canonical monotonic evidence alone.
            return LogRecordValidation::InvalidSyncEvidence;
        }

        // Fail closed on unknown EDP-Clock enum values or reserved fact bits.
        if (static_cast<std::uint8_t>(record.Provenance.Projection) >
                static_cast<std::uint8_t>(
                    Clock::ClockCorrelationProjectionStatus::SynchronizedCoordinateOverflow) ||
            static_cast<std::uint8_t>(record.Provenance.Quality) >
                static_cast<std::uint8_t>(Clock::TemporalQualityAssessment::Reliable)) {
            return LogRecordValidation::InvalidTemporalProvenance;
        }
        if (record.Facts.Value != 0U) {
            return LogRecordValidation::ReservedRecordFacts;
        }

        return LogRecordValidation::Valid;
    }

} // ESPressio::Logging
