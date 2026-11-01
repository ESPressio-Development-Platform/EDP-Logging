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
    enum class LogRecordCoreFieldConversion : std::uint8_t {
        Succeeded = 0U,
        InvalidSemanticValue = 1U
    };

    namespace Detail {
        // Only explicitly approved strong source/wire pairs are specialized.
        template<class TSemantic, class TWire>
        struct LogRecordScalarRepresentation;

        template<>
        struct LogRecordScalarRepresentation<
            LogMessageIdentifier, std::array<std::uint32_t,3U>
        > final {
            static constexpr LogRecordCoreFieldConversion To(
                const LogMessageIdentifier& v,
                std::array<std::uint32_t,3U>& w
            ) noexcept {
                if(v.String < 3U) return LogRecordCoreFieldConversion::InvalidSemanticValue;
                w = {v.Domain, v.SubDomain, v.String};
                return LogRecordCoreFieldConversion::Succeeded;
            }
            static constexpr LogRecordCoreFieldConversion From(
                const std::array<std::uint32_t,3U>& w,
                LogMessageIdentifier& v
            ) noexcept {
                if(w[2] < 3U) return LogRecordCoreFieldConversion::InvalidSemanticValue;
                v = {w[0], w[1], w[2]};
                return LogRecordCoreFieldConversion::Succeeded;
            }
        };

        template<>
        struct LogRecordScalarRepresentation<Severity,std::uint8_t> final {
            static constexpr LogRecordCoreFieldConversion To(const Severity& v, std::uint8_t& w) noexcept {
                w = static_cast<std::uint8_t>(v);
                return w <= static_cast<std::uint8_t>(Severity::Fatal)
                    ? LogRecordCoreFieldConversion::Succeeded
                    : LogRecordCoreFieldConversion::InvalidSemanticValue;
            }
            static constexpr LogRecordCoreFieldConversion From(const std::uint8_t& w, Severity& v) noexcept {
                if(w > static_cast<std::uint8_t>(Severity::Fatal)) return LogRecordCoreFieldConversion::InvalidSemanticValue;
                v = static_cast<Severity>(w);
                return LogRecordCoreFieldConversion::Succeeded;
            }
        };

        template<AttemptSequenceWidth TWidth>
        struct LogRecordScalarRepresentation<
            AttemptSequence<TWidth>,
            typename AttemptSequence<TWidth>::Storage
        > final {
            using Storage = typename AttemptSequence<TWidth>::Storage;
            static constexpr LogRecordCoreFieldConversion To(
                const AttemptSequence<TWidth>& v, Storage& w
            ) noexcept {
                w = v.Value();
                return w != 0U
                    ? LogRecordCoreFieldConversion::Succeeded
                    : LogRecordCoreFieldConversion::InvalidSemanticValue;
            }
            static constexpr LogRecordCoreFieldConversion From(
                const Storage& w, AttemptSequence<TWidth>& v
            ) noexcept {
                if(w == 0U) return LogRecordCoreFieldConversion::InvalidSemanticValue;
                v = AttemptSequence<TWidth>::FromValidatedValue(w);
                return LogRecordCoreFieldConversion::Succeeded;
            }
        };

        template<>
        struct LogRecordScalarRepresentation<
            Clock::MonotonicTimestamp,std::uint64_t
        > final {
            static constexpr LogRecordCoreFieldConversion To(
                const Clock::MonotonicTimestamp& v,std::uint64_t& w
            ) noexcept {w = v.Nanoseconds();return LogRecordCoreFieldConversion::Succeeded;}
            static constexpr LogRecordCoreFieldConversion From(
                const std::uint64_t& w,Clock::MonotonicTimestamp& v
            ) noexcept {v = Clock::MonotonicTimestamp::FromNanoseconds(w);return LogRecordCoreFieldConversion::Succeeded;}
        };

        template<>
        struct LogRecordScalarRepresentation<
            Clock::SynchronizedTimestamp,std::uint64_t
        > final {
            static constexpr LogRecordCoreFieldConversion To(
                const Clock::SynchronizedTimestamp& v,std::uint64_t& w
            ) noexcept {w = v.Nanoseconds();return LogRecordCoreFieldConversion::Succeeded;}
            static constexpr LogRecordCoreFieldConversion From(
                const std::uint64_t& w,Clock::SynchronizedTimestamp& v
            ) noexcept {v = Clock::SynchronizedTimestamp::FromNanoseconds(w);return LogRecordCoreFieldConversion::Succeeded;}
        };

        template<>
        struct LogRecordScalarRepresentation<
            Clock::SynchronizationState,std::uint8_t
        > final {
            static constexpr LogRecordCoreFieldConversion To(
                const Clock::SynchronizationState& v,std::uint8_t& w
            ) noexcept {
                w = static_cast<std::uint8_t>(v);
                return w <= static_cast<std::uint8_t>(
                    Clock::SynchronizationState::LostSynchronization)
                    ? LogRecordCoreFieldConversion::Succeeded
                    : LogRecordCoreFieldConversion::InvalidSemanticValue;
            }
            static constexpr LogRecordCoreFieldConversion From(
                const std::uint8_t& w,Clock::SynchronizationState& v
            ) noexcept {
                if(w > static_cast<std::uint8_t>(
                    Clock::SynchronizationState::LostSynchronization)) return LogRecordCoreFieldConversion::InvalidSemanticValue;
                v = static_cast<Clock::SynchronizationState>(w);
                return LogRecordCoreFieldConversion::Succeeded;
            }
        };

        template<>
        struct LogRecordScalarRepresentation<
            Clock::SynchronizationUncertainty,std::uint32_t
        > final {
            static constexpr LogRecordCoreFieldConversion To(
                const Clock::SynchronizationUncertainty& v,std::uint32_t& w
            ) noexcept {w = v.Nanoseconds();return LogRecordCoreFieldConversion::Succeeded;}
            static constexpr LogRecordCoreFieldConversion From(
                const std::uint32_t& w,Clock::SynchronizationUncertainty& v
            ) noexcept {v = Clock::SynchronizationUncertainty::FromNanoseconds(w);return LogRecordCoreFieldConversion::Succeeded;}
        };
    }
}

namespace ESPressio::Serialisation {
    // Enum certification selects the owner's exact underlying integer width.
    // Root LogRecord decode must still call ValidateLogRecord to reject
    // invalid numeric enumerators. No new enum values are introduced.
    template<>
    struct EnumSerialisationTraits<ESPressio::Logging::Severity> final {
        using UnderlyingType = std::uint8_t;
    };
    template<>
    struct EnumSerialisationTraits<ESPressio::Clock::SynchronizationState> final {
        using UnderlyingType = std::uint8_t;
    };

    template<>
    struct CanonicalRepresentation<ESPressio::Logging::LogMessageIdentifier> final {
        using Type = std::array<std::uint32_t,3U>;
    };
    template<ESPressio::Logging::AttemptSequenceWidth TWidth>
    struct CanonicalRepresentation<
        ESPressio::Logging::AttemptSequence<TWidth>
    > final {
        using Type = typename ESPressio::Logging::AttemptSequence<TWidth>::Storage;
    };
    template<>
    struct CanonicalRepresentation<ESPressio::Clock::MonotonicTimestamp> final {
        using Type = std::uint64_t;
    };
    template<>
    struct CanonicalRepresentation<ESPressio::Clock::SynchronizedTimestamp> final {
        using Type = std::uint64_t;
    };
    template<>
    struct CanonicalRepresentation<
        ESPressio::Clock::SynchronizationUncertainty
    > final {
        using Type = std::uint32_t;
    };
}

namespace ESPressio::Logging::Detail {
    template<class TSemantic,class TWire>
    struct ToCoreWire {
        using ResultType = LogRecordCoreFieldConversion;
        static constexpr bool IsAvailable = true;
        static constexpr bool IsNoexcept = true;
        static constexpr bool IsSuccessful(ResultType r) noexcept {
            return r == ResultType::Succeeded;
        }
        static constexpr ResultType Convert(
            const TSemantic& source,TWire& wire
        ) noexcept {
            return LogRecordScalarRepresentation<TSemantic,TWire>::To(source,wire);
        }
    };
    template<class TWire,class TSemantic>
    struct FromCoreWire {
        using ResultType = LogRecordCoreFieldConversion;
        static constexpr bool IsAvailable = true;
        static constexpr bool IsNoexcept = true;
        static constexpr bool IsSuccessful(ResultType r) noexcept {
            return r == ResultType::Succeeded;
        }
        static constexpr ResultType Convert(
            const TWire& wire,TSemantic& source
        ) noexcept {
            return LogRecordScalarRepresentation<TSemantic,TWire>::From(wire,source);
        }
    };
}


namespace ESPressio::Bounded {
    // All mapping specialization pairs are owned by this optional Logging
    // adapter; no implicit namespace-wide arithmetic conversion is allowed.
    template<> struct TypeConversionAdapter<
        ESPressio::Logging::LogMessageIdentifier,
        std::array<std::uint32_t,3U>
    > final : ESPressio::Logging::Detail::ToCoreWire<
        ESPressio::Logging::LogMessageIdentifier,
        std::array<std::uint32_t,3U>> {};
    template<> struct TypeConversionAdapter<
        std::array<std::uint32_t,3U>,
        ESPressio::Logging::LogMessageIdentifier
    > final : ESPressio::Logging::Detail::FromCoreWire<
        std::array<std::uint32_t,3U>,
        ESPressio::Logging::LogMessageIdentifier> {};

    template<ESPressio::Logging::AttemptSequenceWidth TWidth>
    struct TypeConversionAdapter<
        ESPressio::Logging::AttemptSequence<TWidth>,
        typename ESPressio::Logging::AttemptSequence<TWidth>::Storage
    > final : ESPressio::Logging::Detail::ToCoreWire<
        ESPressio::Logging::AttemptSequence<TWidth>,
        typename ESPressio::Logging::AttemptSequence<TWidth>::Storage> {};
    template<ESPressio::Logging::AttemptSequenceWidth TWidth>
    struct TypeConversionAdapter<
        typename ESPressio::Logging::AttemptSequence<TWidth>::Storage,
        ESPressio::Logging::AttemptSequence<TWidth>
    > final : ESPressio::Logging::Detail::FromCoreWire<
        typename ESPressio::Logging::AttemptSequence<TWidth>::Storage,
        ESPressio::Logging::AttemptSequence<TWidth>> {};

    template<> struct TypeConversionAdapter<
        ESPressio::Clock::MonotonicTimestamp,std::uint64_t
    > final : ESPressio::Logging::Detail::ToCoreWire<
        ESPressio::Clock::MonotonicTimestamp,std::uint64_t> {};
    template<> struct TypeConversionAdapter<
        std::uint64_t,ESPressio::Clock::MonotonicTimestamp
    > final : ESPressio::Logging::Detail::FromCoreWire<
        std::uint64_t,ESPressio::Clock::MonotonicTimestamp> {};

    template<> struct TypeConversionAdapter<
        ESPressio::Clock::SynchronizedTimestamp,std::uint64_t
    > final : ESPressio::Logging::Detail::ToCoreWire<
        ESPressio::Clock::SynchronizedTimestamp,std::uint64_t> {};
    template<> struct TypeConversionAdapter<
        std::uint64_t,ESPressio::Clock::SynchronizedTimestamp
    > final : ESPressio::Logging::Detail::FromCoreWire<
        std::uint64_t,ESPressio::Clock::SynchronizedTimestamp> {};

    template<> struct TypeConversionAdapter<
        ESPressio::Clock::SynchronizationUncertainty,std::uint32_t
    > final : ESPressio::Logging::Detail::ToCoreWire<
        ESPressio::Clock::SynchronizationUncertainty,std::uint32_t> {};
    template<> struct TypeConversionAdapter<
        std::uint32_t,ESPressio::Clock::SynchronizationUncertainty
    > final : ESPressio::Logging::Detail::FromCoreWire<
        std::uint32_t,ESPressio::Clock::SynchronizationUncertainty> {};
}

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
