#pragma once

// Optional G4 mapping from source-owned LogRecord scalar Field values to
// the exact canonical EDP-Serialisation representations. The Field13
// heterogeneous positional array remains separately schema-bound.
#include <array>
#include <cstdint>
#include <type_traits>
#include <ESPressio_BoundedTypes.hpp>
#include <ESPressio_Serialisation.hpp>
#include <serialisation/ClockEraAdapters.hpp>
#include "LogRecordApprovedFieldsSerialisation.hpp"

namespace ESPressio::Logging {

    /// Result of one approved semantic scalar conversion.
    enum class LogRecordCoreFieldConversion : std::uint8_t {
        /// The approved scalar mapping completed.
        Succeeded = 0U,
        /// The value violated the mapped field’s domain.
        InvalidSemanticValue = 1U
    };

    namespace Detail {

        /// Maps an explicitly approved semantic type to its canonical scalar representation.
        /// @tparam TSemantic Strong source value.
        /// @tparam TWire Exact scalar or fixed aggregate consumed by the owner codec.
        template <class TSemantic, class TWire>
        struct LogRecordScalarRepresentation;

        /// Maps the governed message identity to its Domain, SubDomain and String triple.
        template <>
        struct LogRecordScalarRepresentation<
            LogMessageIdentifier,
            std::array<std::uint32_t, 3U>>
            final {
            /// Maps a valid message identifier to Domain, SubDomain and String in that order.
            static constexpr LogRecordCoreFieldConversion To(
                const LogMessageIdentifier &v,
                std::array<std::uint32_t, 3U> &w) noexcept {
                if (v.String < 3U) return LogRecordCoreFieldConversion::InvalidSemanticValue;
                w = {v.Domain, v.SubDomain, v.String};
                return LogRecordCoreFieldConversion::Succeeded;
            }
            /// Reconstructs the identifier only when the String number is outside the reserved range.
            static constexpr LogRecordCoreFieldConversion From(
                const std::array<std::uint32_t, 3U> &w,
                LogMessageIdentifier &v) noexcept {
                if (w[2] < 3U) return LogRecordCoreFieldConversion::InvalidSemanticValue;
                v = {w[0], w[1], w[2]};
                return LogRecordCoreFieldConversion::Succeeded;
            }
        };

        /// Maps the six emitted severities to their validated one-byte wire ordinals.
        template <>
        struct LogRecordScalarRepresentation<Severity, std::uint8_t> final {
            /// Writes the attempted emitted ordinal and rejects a value above Fatal.
            static constexpr LogRecordCoreFieldConversion To(const Severity &v, std::uint8_t &w) noexcept {
                w = static_cast<std::uint8_t>(v);
                return w <= static_cast<std::uint8_t>(Severity::Fatal)
                           ? LogRecordCoreFieldConversion::Succeeded
                           : LogRecordCoreFieldConversion::InvalidSemanticValue;
            }
            /// Checks the ordinal before constructing an emitted severity.
            static constexpr LogRecordCoreFieldConversion From(const std::uint8_t &w, Severity &v) noexcept {
                if (w > static_cast<std::uint8_t>(Severity::Fatal)) return LogRecordCoreFieldConversion::InvalidSemanticValue;
                v = static_cast<Severity>(w);
                return LogRecordCoreFieldConversion::Succeeded;
            }
        };

        /// Converts a nonzero attempt sequence using its exact storage width.
        /// @tparam TWidth Selected attempt sequence width.
        template <AttemptSequenceWidth TWidth>
        struct LogRecordScalarRepresentation<
            AttemptSequence<TWidth>,
            typename AttemptSequence<TWidth>::Storage>
            final {
            /// Exact unsigned storage chosen by the source attempt width.
            using Storage = typename AttemptSequence<TWidth>::Storage;
            /// Writes the selected-width source ticket and rejects the reserved zero ticket.
            static constexpr LogRecordCoreFieldConversion To(
                const AttemptSequence<TWidth> &v, Storage &w) noexcept {
                w = v.Value();
                return w != 0U
                           ? LogRecordCoreFieldConversion::Succeeded
                           : LogRecordCoreFieldConversion::InvalidSemanticValue;
            }
            /// Reconstructs a nonzero attempt ticket using its selected storage width.
            static constexpr LogRecordCoreFieldConversion From(
                const Storage &w, AttemptSequence<TWidth> &v) noexcept {
                if (w == 0U) return LogRecordCoreFieldConversion::InvalidSemanticValue;
                v = AttemptSequence<TWidth>::FromValidatedValue(w);
                return LogRecordCoreFieldConversion::Succeeded;
            }
        };

        /// Maps a local Clock coordinate to its exact unsigned nanosecond scalar.
        template <>
        struct LogRecordScalarRepresentation<
            Clock::MonotonicTimestamp,
            std::uint64_t>
            final {
            /// Extracts the existing monotonic coordinate in nanoseconds.
            static constexpr LogRecordCoreFieldConversion To(
                const Clock::MonotonicTimestamp &v, std::uint64_t &w) noexcept {
                w = v.Nanoseconds();
                return LogRecordCoreFieldConversion::Succeeded;
            }
            /// Reconstructs a monotonic coordinate without querying a Clock.
            static constexpr LogRecordCoreFieldConversion From(
                const std::uint64_t &w, Clock::MonotonicTimestamp &v) noexcept {
                v = Clock::MonotonicTimestamp::FromNanoseconds(w);
                return LogRecordCoreFieldConversion::Succeeded;
            }
        };

        /// Maps a synchronized coordinate to nanoseconds while the enclosing record owns presence.
        template <>
        struct LogRecordScalarRepresentation<
            Clock::SynchronizedTimestamp,
            std::uint64_t>
            final {
            /// Extracts the synchronized coordinate, including zero.
            static constexpr LogRecordCoreFieldConversion To(
                const Clock::SynchronizedTimestamp &v, std::uint64_t &w) noexcept {
                w = v.Nanoseconds();
                return LogRecordCoreFieldConversion::Succeeded;
            }
            /// Reconstructs the synchronized coordinate; the enclosing record validates presence.
            static constexpr LogRecordCoreFieldConversion From(
                const std::uint64_t &w, Clock::SynchronizedTimestamp &v) noexcept {
                v = Clock::SynchronizedTimestamp::FromNanoseconds(w);
                return LogRecordCoreFieldConversion::Succeeded;
            }
        };

        /// Maps the canonical Clock state ordinal with explicit semantic range validation.
        template <>
        struct LogRecordScalarRepresentation<
            Clock::SynchronizationState,
            std::uint8_t>
            final {
            /// Writes the attempted state ordinal and rejects values above LostSynchronization.
            static constexpr LogRecordCoreFieldConversion To(
                const Clock::SynchronizationState &v, std::uint8_t &w) noexcept {
                w = static_cast<std::uint8_t>(v);
                return w <= static_cast<std::uint8_t>(
                                Clock::SynchronizationState::LostSynchronization)
                           ? LogRecordCoreFieldConversion::Succeeded
                           : LogRecordCoreFieldConversion::InvalidSemanticValue;
            }
            /// Checks the ordinal before constructing the canonical Clock synchronization state.
            static constexpr LogRecordCoreFieldConversion From(
                const std::uint8_t &w, Clock::SynchronizationState &v) noexcept {
                if (w > static_cast<std::uint8_t>(
                            Clock::SynchronizationState::LostSynchronization)) return LogRecordCoreFieldConversion::InvalidSemanticValue;
                v = static_cast<Clock::SynchronizationState>(w);
                return LogRecordCoreFieldConversion::Succeeded;
            }
        };

        /// Maps the Clock owner's uncertainty to its exact unsigned nanosecond representation.
        template <>
        struct LogRecordScalarRepresentation<
            Clock::SynchronizationUncertainty,
            std::uint32_t>
            final {
            /// Extracts the Clock uncertainty nanoseconds at the owner-defined width.
            static constexpr LogRecordCoreFieldConversion To(
                const Clock::SynchronizationUncertainty &v, std::uint32_t &w) noexcept {
                w = v.Nanoseconds();
                return LogRecordCoreFieldConversion::Succeeded;
            }
            /// Reconstructs the strong Clock uncertainty from its nanosecond value.
            static constexpr LogRecordCoreFieldConversion From(
                const std::uint32_t &w, Clock::SynchronizationUncertainty &v) noexcept {
                v = Clock::SynchronizationUncertainty::FromNanoseconds(w);
                return LogRecordCoreFieldConversion::Succeeded;
            }
        };

    } // namespace Detail

} // namespace ESPressio::Logging

namespace ESPressio::Serialisation {

    // Enum certification selects the owner's exact underlying integer width.
    // Root LogRecord decode must still call ValidateLogRecord to reject
    // invalid numeric enumerators. No new enum values are introduced.
    /// Certifies the exact emitted-severity integer width for the Serialisation owner.
    template <>
    struct EnumSerialisationTraits<ESPressio::Logging::Severity> final {
        /// One-byte ordinal. Record validation separately rejects values above Fatal.
        using UnderlyingType = std::uint8_t;
    };

    /// Certifies the Clock state width for this opt-in LogRecord field mapping.
    template <>
    struct EnumSerialisationTraits<ESPressio::Clock::SynchronizationState> final {
        /// One-byte ordinal. Record validation separately checks the Clock vocabulary.
        using UnderlyingType = std::uint8_t;
    };

    /// Selects the exact governed message triple consumed by the owner codec.
    template <>
    struct CanonicalRepresentation<ESPressio::Logging::LogMessageIdentifier> final {
        /// Ordered Domain, SubDomain and String values at their exact unsigned width.
        using Type = std::array<std::uint32_t, 3U>;
    };

    /// Selects the exact attempt-ticket scalar width for the owner codec.
    /// @tparam TWidth Selected width of the source record attempt sequence.
    template <ESPressio::Logging::AttemptSequenceWidth TWidth>
    struct CanonicalRepresentation<
        ESPressio::Logging::AttemptSequence<TWidth>>
        final {
        /// The same unsigned 16-, 32- or 64-bit storage selected for the source ticket.
        using Type = typename ESPressio::Logging::AttemptSequence<TWidth>::Storage;
    };

    /// Selects the exact local monotonic nanosecond scalar for record encoding.
    template <>
    struct CanonicalRepresentation<ESPressio::Clock::MonotonicTimestamp> final {
        /// Unsigned nanoseconds from the record's captured local Clock sample.
        using Type = std::uint64_t;
    };

    /// Selects the synchronized coordinate scalar whose presence is validated by the record.
    template <>
    struct CanonicalRepresentation<ESPressio::Clock::SynchronizedTimestamp> final {
        /// Unsigned synchronized nanoseconds. Zero remains a valid present coordinate.
        using Type = std::uint64_t;
    };

    /// Selects the Clock owner's exact uncertainty width for the record codec.
    template <>
    struct CanonicalRepresentation<
        ESPressio::Clock::SynchronizationUncertainty>
        final {
        /// Unsigned uncertainty nanoseconds at the Clock owner's canonical width.
        using Type = std::uint32_t;
    };

} // namespace ESPressio::Serialisation

namespace ESPressio::Logging::Detail {

    /// Adapts the semantic-to-wire mapping to the BoundedTypes conversion protocol.
    /// @tparam TSemantic Strong source type.
    /// @tparam TWire Destination wire representation.
    template <class TSemantic, class TWire>
    struct ToCoreWire {
        /// Typed conversion outcome required by the BoundedTypes bridge.
        using ResultType = LogRecordCoreFieldConversion;
        /// This approved pair provides a direct conversion.
        static constexpr bool IsAvailable = true;
        /// The scalar conversion executes without exceptions.
        static constexpr bool IsNoexcept = true;
        /// Returns whether the conversion published its intended output.
        static constexpr bool IsSuccessful(ResultType r) noexcept {
            return r == ResultType::Succeeded;
        }
        /// Delegates to the approved scalar conversion in the selected direction.
        static constexpr ResultType Convert(
            const TSemantic &source, TWire &wire) noexcept {
            return LogRecordScalarRepresentation<TSemantic, TWire>::To(
                source,
                wire
            );
        }
    };

    /// Adapts the wire-to-semantic mapping to the BoundedTypes conversion protocol.
    /// @tparam TWire Encoded scalar source representation.
    /// @tparam TSemantic Strong destination type.
    template <class TWire, class TSemantic>
    struct FromCoreWire {
        /// Typed conversion outcome required by the BoundedTypes bridge.
        using ResultType = LogRecordCoreFieldConversion;
        /// This approved pair provides a direct conversion.
        static constexpr bool IsAvailable = true;
        /// The scalar conversion executes without exceptions.
        static constexpr bool IsNoexcept = true;
        /// Returns whether the conversion published its intended output.
        static constexpr bool IsSuccessful(ResultType r) noexcept {
            return r == ResultType::Succeeded;
        }
        /// Delegates to the approved scalar conversion in the selected direction.
        static constexpr ResultType Convert(
            const TWire &wire, TSemantic &source) noexcept {
            return LogRecordScalarRepresentation<TSemantic, TWire>::From(
                wire,
                source
            );
        }
    };

} // namespace ESPressio::Logging::Detail

namespace ESPressio::Bounded {

    // All mapping specialization pairs are owned by this optional Logging
    // adapter; no implicit namespace-wide arithmetic conversion is allowed.
    /// Adapts the governed message triple through the existing exact semantic conversion.
    template <>
    struct TypeConversionAdapter<
        ESPressio::Logging::LogMessageIdentifier,
        std::array<std::uint32_t, 3U>>
        final : ESPressio::Logging::Detail::ToCoreWire<
                    ESPressio::Logging::LogMessageIdentifier,
                    std::array<std::uint32_t, 3U>> {};

    /// Restores a governed message identity through the existing validated triple conversion.
    template <>
    struct TypeConversionAdapter<
        std::array<std::uint32_t, 3U>,
        ESPressio::Logging::LogMessageIdentifier>
        final : ESPressio::Logging::Detail::FromCoreWire<
                    std::array<std::uint32_t, 3U>,
                    ESPressio::Logging::LogMessageIdentifier> {};

    /// @tparam TWidth Selected width of the source record attempt sequence.
    template <ESPressio::Logging::AttemptSequenceWidth TWidth>
    struct TypeConversionAdapter<
        ESPressio::Logging::AttemptSequence<TWidth>,
        typename ESPressio::Logging::AttemptSequence<TWidth>::Storage>
        final : ESPressio::Logging::Detail::ToCoreWire<
                    ESPressio::Logging::AttemptSequence<TWidth>,
                    typename ESPressio::Logging::AttemptSequence<TWidth>::Storage> {};

    /// @tparam TWidth Selected width of the source record attempt sequence.
    template <ESPressio::Logging::AttemptSequenceWidth TWidth>
    struct TypeConversionAdapter<
        typename ESPressio::Logging::AttemptSequence<TWidth>::Storage,
        ESPressio::Logging::AttemptSequence<TWidth>>
        final : ESPressio::Logging::Detail::FromCoreWire<
                    typename ESPressio::Logging::AttemptSequence<TWidth>::Storage,
                    ESPressio::Logging::AttemptSequence<TWidth>> {};

    /// Adapts a captured local Clock coordinate to its nanosecond wire scalar.
    template <>
    struct TypeConversionAdapter<
        ESPressio::Clock::MonotonicTimestamp,
        std::uint64_t>
        final : ESPressio::Logging::Detail::ToCoreWire<
                    ESPressio::Clock::MonotonicTimestamp,
                    std::uint64_t> {};

    /// Restores a local Clock coordinate without obtaining another time sample.
    template <>
    struct TypeConversionAdapter<
        std::uint64_t,
        ESPressio::Clock::MonotonicTimestamp>
        final : ESPressio::Logging::Detail::FromCoreWire<
                    std::uint64_t,
                    ESPressio::Clock::MonotonicTimestamp> {};

    /// Adapts synchronized nanoseconds while retaining record-level presence validation.
    template <>
    struct TypeConversionAdapter<
        ESPressio::Clock::SynchronizedTimestamp,
        std::uint64_t>
        final : ESPressio::Logging::Detail::ToCoreWire<
                    ESPressio::Clock::SynchronizedTimestamp,
                    std::uint64_t> {};

    /// Restores synchronized nanoseconds with presence supplied by the enclosing record.
    template <>
    struct TypeConversionAdapter<
        std::uint64_t,
        ESPressio::Clock::SynchronizedTimestamp>
        final : ESPressio::Logging::Detail::FromCoreWire<
                    std::uint64_t,
                    ESPressio::Clock::SynchronizedTimestamp> {};

    /// Adapts uncertainty using the exact width defined by the Clock owner.
    template <>
    struct TypeConversionAdapter<
        ESPressio::Clock::SynchronizationUncertainty,
        std::uint32_t>
        final : ESPressio::Logging::Detail::ToCoreWire<
                    ESPressio::Clock::SynchronizationUncertainty,
                    std::uint32_t> {};

    /// Restores the strong Clock uncertainty from its exact nanosecond wire scalar.
    template <>
    struct TypeConversionAdapter<
        std::uint32_t,
        ESPressio::Clock::SynchronizationUncertainty>
        final : ESPressio::Logging::Detail::FromCoreWire<
                    std::uint32_t,
                    ESPressio::Clock::SynchronizationUncertainty> {};

} // namespace ESPressio::Bounded

static_assert(ESPressio::Serialisation::SerialisableType<
              ESPressio::Logging::LogMessageIdentifier>);
static_assert(ESPressio::Serialisation::SerialisableType<
              ESPressio::Logging::Severity>);
static_assert(ESPressio::Serialisation::SerialisableType<
              ESPressio::Logging::AttemptSequence<ESPressio::Logging::AttemptSequenceWidth::Bits16>>);
static_assert(ESPressio::Serialisation::SerialisableType<
              ESPressio::Logging::AttemptSequence<ESPressio::Logging::AttemptSequenceWidth::Bits32>>);
static_assert(ESPressio::Serialisation::SerialisableType<
              ESPressio::Logging::AttemptSequence<ESPressio::Logging::AttemptSequenceWidth::Bits64>>);
static_assert(ESPressio::Serialisation::SerialisableType<
              ESPressio::Clock::MonotonicTimestamp>);
static_assert(ESPressio::Serialisation::SerialisableType<
              ESPressio::Clock::SynchronizedTimestamp>);
static_assert(ESPressio::Serialisation::SerialisableType<
              ESPressio::Clock::SynchronizationState>);
static_assert(ESPressio::Serialisation::SerialisableType<
              ESPressio::Clock::ClockEra>);
static_assert(ESPressio::Serialisation::SerialisableType<
              ESPressio::Clock::SynchronizationUncertainty>);
