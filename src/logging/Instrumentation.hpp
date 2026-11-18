#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

#include "Severity.hpp"

namespace ESPressio::Logging {

    /// Compile-time instrumentation gate result, NOT a producer-admission outcome.
    enum class SiteInvocationResult : std::uint8_t {
        /// The compile-time threshold excluded the site and its callable was skipped.
        NotInstrumented = 0,
        /// The site callable ran and returned.
        Invoked = 1
    };

    /// Evaluates a diagnostic site lazily according to its severity threshold.
    ///
    /// Unlike a function that accepts previously evaluated argument values, this
    /// template receives one caller expression inside an unevaluated-until-called
    /// thunk. Disabled instantiations never invoke that thunk, so runtime
    /// provider discovery, time reads, admission-policy evaluation and message
    /// arguments do not execute.
    ///
    /// The compile-time gate discards the callable's return value.
    ///
    /// @tparam TMinimum Compile-time threshold selected for the facility.
    /// @tparam TSeverity Severity emitted by the diagnostic site.
    /// @tparam TThunk Caller-supplied callable type, invocable without arguments when enabled.
    /// @param thunk Deferred site work, invoked once only for an enabled site.
    /// @return Invoked if the callable returned, or NotInstrumented for an excluded site.
    template<SeverityThreshold TMinimum, Severity TSeverity, class TThunk>
    [[nodiscard]] constexpr SiteInvocationResult InvokeLogSite(
        TThunk&& thunk
    ) {
        if constexpr (IsSeverityEnabled<TMinimum, TSeverity>) {
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
/// The macro provides lazy evaluation. A producer invoked by the site body
/// supplies its own admission result and delivery evidence.
#define EDP_LOG_WHEN_ENABLED(MINIMUM_THRESHOLD, EMITTED_SEVERITY, ...)                \
    do {                                                                            \
        static_cast<void>(                                                         \
            ::ESPressio::Logging::InvokeLogSite<                                    \
                (MINIMUM_THRESHOLD), (EMITTED_SEVERITY)                             \
            >([&]() { __VA_ARGS__; })                                               \
        );                                                                          \
    } while (false)
