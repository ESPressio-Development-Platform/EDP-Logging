#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <ESPressio_Logging.hpp>

namespace Test {

    using namespace ESPressio::Logging;

    static_assert(DefaultSeverityThreshold == SeverityThreshold::Off);
    static_assert(!SeverityEnabled<SeverityThreshold::Off, Severity::Fatal>);
    static_assert(!SeverityEnabled<SeverityThreshold::Warning, Severity::Info>);
    static_assert(SeverityEnabled<SeverityThreshold::Warning, Severity::Warning>);
    static_assert(SeverityEnabled<SeverityThreshold::Warning, Severity::Fatal>);
    static_assert(AttemptSequence<>::WidthBits == 32U);

    /// Verifies the full 16-bit ticket space can be exhausted without silent wrap.
    void Exhaustion16Bit() {
        AttemptSequenceCounter<AttemptSequenceWidth::Bits16> counter;
        for (std::uint32_t ordinal = 1U; ordinal <= 65535U; ++ordinal) {
            AttemptSequence<AttemptSequenceWidth::Bits16> ticket;
            assert(counter.Issue(ticket) == AttemptSequenceAllocationResult::Issued);
            assert(ticket.Value() == ordinal);
        }
        assert(counter.IsExhausted());
        AttemptSequence<AttemptSequenceWidth::Bits16> last;
        assert(counter.Issue(last) == AttemptSequenceAllocationResult::SequenceExhausted);
        assert(!last.IsValid());
        assert(counter.LastIssued() == std::numeric_limits<std::uint16_t>::max());
    }

    /// Verifies 32- and 64-bit storage and refusal to replace a prior allocated ticket.
    template<AttemptSequenceWidth TWidth>
    void ValidateStartAndOccupied() {
        AttemptSequenceCounter<TWidth> counter;
        AttemptSequence<TWidth> first;
        assert(counter.Issue(first) == AttemptSequenceAllocationResult::Issued);
        assert(first.Value() == 1U);
        assert(counter.Issue(first) == AttemptSequenceAllocationResult::OutputOccupied);
        assert(counter.LastIssued() == 1U);
        AttemptSequence<TWidth> second;
        assert(counter.Issue(second) == AttemptSequenceAllocationResult::Issued);
        assert(second.Value() == 2U);
    }

} // Test

int main() {
    Test::Exhaustion16Bit();
    Test::ValidateStartAndOccupied<ESPressio::Logging::AttemptSequenceWidth::Bits32>();
    Test::ValidateStartAndOccupied<ESPressio::Logging::AttemptSequenceWidth::Bits64>();
    return 0;
}
