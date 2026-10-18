#include <cstdint>
#include <cstdio>
#include <logging/Instrumentation.hpp>

#ifndef EDP_LOGGING_G11_ENABLE_SITE
#error "G11 proof must select enabled (1) or compiled-out (0)"
#endif

#if EDP_LOGGING_G11_ENABLE_SITE != 0 && EDP_LOGGING_G11_ENABLE_SITE != 1
#error "G11 proof accepts only 0 or 1"
#endif

namespace ESPressio::Logging::G11Proof {

    // Measurement-only volatile counters prevent the host optimizer from
    // collapsing intentional enabled-site effects into compile-time values.
    volatile std::uint32_t ClockReads = 0U;
    volatile std::uint32_t IdentityReads = 0U;
    volatile std::uint32_t ArgumentReads = 0U;

    /// Simulates the full Clock snapshot evaluation at one instrumented site.
    [[gnu::noinline]] void CaptureClock() noexcept {
        ClockReads = ClockReads + 1U;
    }

    /// Simulates device/incarnation identity capture at one site.
    [[gnu::noinline]] void CaptureIdentity() noexcept {
        IdentityReads = IdentityReads + 1U;
    }

    /// Simulates one owned argument evaluation at one site.
    [[gnu::noinline]] void CaptureArgument() noexcept {
        ArgumentReads = ArgumentReads + 1U;
    }

    /// Selects the same callsite at build time rather than using runtime flags.
#if EDP_LOGGING_G11_ENABLE_SITE == 1
    inline constexpr SeverityThreshold SiteThreshold = SeverityThreshold::Info;
#else
    inline constexpr SeverityThreshold SiteThreshold = SeverityThreshold::Off;
#endif

    /// Exercises the public lazy instrumentation macro exactly once.
    void EvaluateOneSite() noexcept {
        EDP_LOG_WHEN_ENABLED(
            SiteThreshold,
            Severity::Info,
            CaptureClock();
            CaptureIdentity();
            CaptureArgument()
        );
    }

    /// Reports only the observable probe effects, not a production budget.
    bool IsExpectedResult() noexcept {
        const auto expected = static_cast<std::uint32_t>(
            EDP_LOGGING_G11_ENABLE_SITE
        );
        return ClockReads == expected &&
               IdentityReads == expected &&
               ArgumentReads == expected;
    }

} // ESPressio::Logging::G11Proof

int main() {
    ESPressio::Logging::G11Proof::EvaluateOneSite();
    const auto passed = ESPressio::Logging::G11Proof::IsExpectedResult();
    std::printf(
        "EDP_G11_SITE|selected=%d|clock_reads=%u|identity_reads=%u|argument_reads=%u|result=%s\n",
        EDP_LOGGING_G11_ENABLE_SITE,
        static_cast<unsigned>(ESPressio::Logging::G11Proof::ClockReads),
        static_cast<unsigned>(ESPressio::Logging::G11Proof::IdentityReads),
        static_cast<unsigned>(ESPressio::Logging::G11Proof::ArgumentReads),
        passed ? "PASS" : "FAIL"
    );
    return passed ? 0 : 1;
}
