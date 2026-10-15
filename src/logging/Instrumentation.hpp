#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

#include "Severity.hpp"

namespace ESPressio::Logging {

    /// Compile-time instrumentation gate result, NOT a producer-admission outcome.
    enum class SiteInvocationResult : std::uint8_t {
        NotInstrumented = 0,
        Invoked = 1
    };

    /// Template isolated, lazy instrumentation-site evaluation.
    ///
    /// Unlike a function that accepts previously evaluated argument values, this
    /// template receives one caller expression inside an unevaluated-until-called
    /// thunk. Disabled instantiations never invoke that thunk, so runtime
    /// provider discovery, time reads, admission-policy evaluation and message
    /// arguments do not execute.
    ///
    /// TThunk must be a callable with no arguments only for enabled sites.
    /// Its returned value (which will later be the typed admission outcome)
    /// is intentionally not interpreted by the compile-out facility.
    template<SeverityThreshold TMinimum, Severity TSeverity, class TThunk>
    [[nodiscard]] constexpr SiteInvocationResult InvokeLogSite(
        TThunk&& thunk
    ) {
        if constexpr (SeverityEnabled<TMinimum, TSeverity>) {
            static_cast<void>(std::forward<TThunk>(thunk)());
            return SiteInvocationResult::Invoked;
        } else {
            static_cast<void>(thunk);
            return SiteInvocationResult::NotInstrumented;
        }
    }

} // ESPressio::Logging

/// Statement-only lazy instrumentation, wrapped around the entire expression.
///
/// Bootstrap/library author supplies one compile-time minimum threshold and
/// one emitted Severity. All site work must live inside __VA_ARGS__; putting
/// evaluated arguments outside the macro before calling it defeats compile-out.
///
/// This is a gating prerequisite only: it cannot allocate a LogRecord or claim
/// delivery. A future generated-schema-checked LogSite adapter will supply
/// producer admission and a separate typed admission result.
///
/// The prefix deliberately does not claim the shorter EDP_LOG entry point while
/// a complete caller-facing instrumentation contract remains unimplemented.
#define EDP_LOG_WHEN_ENABLED(MINIMUM_THRESHOLD, EMITTED_SEVERITY, ...)                \
    do {                                                                            \
        static_cast<void>(                                                         \
            ::ESPressio::Logging::InvokeLogSite<                                    \
                (MINIMUM_THRESHOLD), (EMITTED_SEVERITY)                             \
            >([&]() { __VA_ARGS__; })                                               \
        );                                                                          \
    } while (false)
