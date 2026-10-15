#include <cassert>
#include <cstdint>

#include <ESPressio_Logging.hpp>

namespace Test {

    using namespace ESPressio::Logging;

    struct TimeSource final {
        int Reads = 0;
        std::uint64_t Now() noexcept {
            ++Reads;
            return 123U;
        }
    };

    struct AdmissionPolicy final {
        int Evaluations = 0;
        int BuildDeadline() noexcept {
            ++Evaluations;
            return 50;
        }
    };

    void CompilerGateMustSkipAllEvaluation() {
        int evaluatedArguments = 0;
        int invoked = 0;
        TimeSource clock{};
        AdmissionPolicy policy{};

        // Off disables even the most important emitted severity.
        EDP_LOG_WHEN_ENABLED(SeverityThreshold::Off, Severity::Fatal,
            ++evaluatedArguments;
            clock.Now();
            policy.BuildDeadline();
            ++invoked;
        );
        assert(evaluatedArguments == 0);
        assert(clock.Reads == 0);
        assert(policy.Evaluations == 0);
        assert(invoked == 0);

        // A severity below the library threshold must also perform no work.
        EDP_LOG_WHEN_ENABLED(SeverityThreshold::Warning, Severity::Debug,
            ++evaluatedArguments;
            clock.Now();
            policy.BuildDeadline();
            ++invoked;
        );
        assert(evaluatedArguments == 0);
        assert(clock.Reads == 0);
        assert(policy.Evaluations == 0);
        assert(invoked == 0);

        // One enabled site evaluates each expression exactly once.
        EDP_LOG_WHEN_ENABLED(SeverityThreshold::Warning, Severity::Error,
            ++evaluatedArguments;
            (void)clock.Now();
            (void)policy.BuildDeadline();
            ++invoked;
        );
        assert(evaluatedArguments == 1);
        assert(clock.Reads == 1);
        assert(policy.Evaluations == 1);
        assert(invoked == 1);

        const auto disabled = InvokeLogSite<SeverityThreshold::Off, Severity::Fatal>(
            [&]() { ++evaluatedArguments; }
        );
        const auto enabled = InvokeLogSite<SeverityThreshold::Info, Severity::Info>(
            [&]() { ++evaluatedArguments; }
        );
        assert(disabled == SiteInvocationResult::NotInstrumented);
        assert(enabled == SiteInvocationResult::Invoked);
        assert(evaluatedArguments == 2);
    }

} // Test

int main() {
    Test::CompilerGateMustSkipAllEvaluation();
    return 0;
}
