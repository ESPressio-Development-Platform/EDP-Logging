#pragma once
#include <array>
#include <cstdint>
#include <ESPressio_BoundedTypes.hpp>
#include <ESPressio_Serialisation.hpp>
#include "LogRecord.hpp"

namespace ESPressio::Logging {
    enum class ApprovedLogFieldConversion : std::uint8_t {
        Succeeded = 0U,
        UnknownClockValue = 1U,
        ReservedRecordFacts = 2U
    };
    constexpr bool IsApprovedProvenance(
        const LogTemporalProvenance& p
    ) noexcept {
        return static_cast<std::uint8_t>(p.Projection) <=
                   static_cast<std::uint8_t>(
                       Clock::ClockCorrelationProjectionStatus::SynchronizedCoordinateOverflow) &&
               static_cast<std::uint8_t>(p.Quality) <=
                   static_cast<std::uint8_t>(Clock::TemporalQualityAssessment::Reliable);
    }
}

namespace ESPressio::Serialisation {
    template<> struct CanonicalRepresentation<ESPressio::Logging::LogTemporalProvenance> final {
        using Type = std::array<std::uint8_t, 2U>;
    };
    template<> struct CanonicalRepresentation<ESPressio::Logging::LogRecordFacts> final {
        using Type = std::uint8_t;
    };
}

namespace ESPressio::Bounded {
    template<> struct TypeConversionAdapter<
        ESPressio::Logging::LogTemporalProvenance,
        std::array<std::uint8_t, 2U>
    > final {
        using ResultType = ESPressio::Logging::ApprovedLogFieldConversion;
        static constexpr bool IsAvailable = true;
        static constexpr bool IsNoexcept = true;
        static constexpr bool IsSuccessful(ResultType r) noexcept {
            return r == ResultType::Succeeded;
        }
        static constexpr ResultType Convert(
            const ESPressio::Logging::LogTemporalProvenance& src,
            std::array<std::uint8_t, 2U>& dst
        ) noexcept {
            if (!ESPressio::Logging::IsApprovedProvenance(src)) {
                return ResultType::UnknownClockValue;
            }
            dst = {static_cast<std::uint8_t>(src.Projection),
                   static_cast<std::uint8_t>(src.Quality)};
            return ResultType::Succeeded;
        }
    };

    template<> struct TypeConversionAdapter<
        std::array<std::uint8_t, 2U>,
        ESPressio::Logging::LogTemporalProvenance
    > final {
        using ResultType = ESPressio::Logging::ApprovedLogFieldConversion;
        static constexpr bool IsAvailable = true;
        static constexpr bool IsNoexcept = true;
        static constexpr bool IsSuccessful(ResultType r) noexcept {
            return r == ResultType::Succeeded;
        }
        static constexpr ResultType Convert(
            const std::array<std::uint8_t, 2U>& src,
            ESPressio::Logging::LogTemporalProvenance& dst
        ) noexcept {
            const ESPressio::Logging::LogTemporalProvenance tmp{
                static_cast<ESPressio::Clock::ClockCorrelationProjectionStatus>(src[0]),
                static_cast<ESPressio::Clock::TemporalQualityAssessment>(src[1])
            };
            if (!ESPressio::Logging::IsApprovedProvenance(tmp)) {
                return ResultType::UnknownClockValue;
            }
            dst = tmp;
            return ResultType::Succeeded;
        }
    };

    template<> struct TypeConversionAdapter<
        ESPressio::Logging::LogRecordFacts, std::uint8_t
    > final {
        using ResultType = ESPressio::Logging::ApprovedLogFieldConversion;
        static constexpr bool IsAvailable = true;
        static constexpr bool IsNoexcept = true;
        static constexpr bool IsSuccessful(ResultType r) noexcept {
            return r == ResultType::Succeeded;
        }
        static constexpr ResultType Convert(
            const ESPressio::Logging::LogRecordFacts& src,
            std::uint8_t& dst
        ) noexcept {
            if (src.Value != 0U) return ResultType::ReservedRecordFacts;
            dst = 0U;
            return ResultType::Succeeded;
        }
    };

    template<> struct TypeConversionAdapter<
        std::uint8_t, ESPressio::Logging::LogRecordFacts
    > final {
        using ResultType = ESPressio::Logging::ApprovedLogFieldConversion;
        static constexpr bool IsAvailable = true;
        static constexpr bool IsNoexcept = true;
        static constexpr bool IsSuccessful(ResultType r) noexcept {
            return r == ResultType::Succeeded;
        }
        static constexpr ResultType Convert(
            const std::uint8_t& src,
            ESPressio::Logging::LogRecordFacts& dst
        ) noexcept {
            if (src != 0U) return ResultType::ReservedRecordFacts;
            dst.Value = 0U;
            return ResultType::Succeeded;
        }
    };
}

static_assert(ESPressio::Serialisation::SerialisableType<
    ESPressio::Logging::LogTemporalProvenance>);
static_assert(ESPressio::Serialisation::SerialisableType<
    ESPressio::Logging::LogRecordFacts>);
