#pragma once

#include <cstdint>

namespace ESPressio::Logging {

    /// One of the six emitted diagnostic severities; Off is never an emitted level.
    enum class Severity : std::uint8_t {
        /// Finest-grained execution tracing.
        Trace = 0,
        /// Diagnostics primarily for developers.
        Debug = 1,
        /// An ordinary noteworthy operation.
        Info = 2,
        /// An unexpected condition from which operation can continue.
        Warning = 3,
        /// An operation failed.
        Error = 4,
        /// A severe failure requiring application-level response.
        Fatal = 5
    };

    /// Compile-time lower severity threshold, including the excluded-instrumentation mode.
    enum class SeverityThreshold : std::uint8_t {
        /// Enable all emitted levels.
        Trace = 0,
        /// Enable Debug and higher levels.
        Debug = 1,
        /// Enable Info and higher levels.
        Info = 2,
        /// Enable Warning and higher levels.
        Warning = 3,
        /// Enable Error and Fatal.
        Error = 4,
        /// Enable Fatal only.
        Fatal = 5,
        /// Compile out all diagnostic sites using this threshold.
        Off = 255
    };

    /// Default: no instrumentation is enabled until the application chooses a threshold.
    inline constexpr SeverityThreshold DefaultSeverityThreshold = SeverityThreshold::Off;

    /// Compile-time decision for one diagnostic site in its configured facility.
    ///
    /// @tparam TMinimum Bootstrap-selected compile-time threshold.
    /// @tparam TSeverity This site's emitted severity (never Off).
    template<SeverityThreshold TMinimum, Severity TSeverity>
    inline constexpr bool IsSeverityEnabled =
        TMinimum != SeverityThreshold::Off &&
        static_cast<std::uint8_t>(TSeverity) >= static_cast<std::uint8_t>(TMinimum);

} // ESPressio::Logging
