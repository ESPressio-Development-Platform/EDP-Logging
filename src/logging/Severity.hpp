#pragma once

#include <cstdint>

namespace ESPressio::Logging {

    /// One of the six emitted diagnostic severities; Off is never an emitted level.
    enum class Severity : std::uint8_t {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warning = 3,
        Error = 4,
        Fatal = 5
    };

    /// Compile-time lower severity threshold, including the excluded-instrumentation mode.
    enum class SeverityThreshold : std::uint8_t {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warning = 3,
        Error = 4,
        Fatal = 5,
        Off = 255
    };

    /// Default: no instrumentation is enabled until the application chooses a threshold.
    inline constexpr SeverityThreshold DefaultSeverityThreshold = SeverityThreshold::Off;

    /// Compile-time decision for one diagnostic site in its configured facility.
    ///
    /// @tparam TMinimum Bootstrap-selected compile-time threshold.
    /// @tparam TSeverity This site's emitted severity (never Off).
    template<SeverityThreshold TMinimum, Severity TSeverity>
    inline constexpr bool SeverityEnabled =
        TMinimum != SeverityThreshold::Off &&
        static_cast<std::uint8_t>(TSeverity) >= static_cast<std::uint8_t>(TMinimum);

} // ESPressio::Logging
