#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <ESPressio_Logging.hpp>

namespace Demo {

    /// Checks the allocation-free core without enabling a worker, filesystem, or provider.
    int Run() noexcept {
        using namespace ESPressio::Logging;

        static_assert(SeverityEnabled<SeverityThreshold::Info, Severity::Warning>);
        static_assert(!SeverityEnabled<SeverityThreshold::Off, Severity::Fatal>);

        int evaluated{0};
        EDP_LOG_WHEN_ENABLED(SeverityThreshold::Off, Severity::Error, ++evaluated);
        if (evaluated != 0) { return 1; }

        EDP_LOG_WHEN_ENABLED(SeverityThreshold::Info, Severity::Warning, ++evaluated);
        if (evaluated != 1) { return 2; }

        AttemptSequenceCounter<AttemptSequenceWidth::Bits16> sequence;
        AttemptSequence<AttemptSequenceWidth::Bits16> ticket;
        if (sequence.Issue(ticket) != AttemptSequenceAllocationResult::Issued) { return 3; }
        if (!ticket.IsValid() || ticket.Value() != 1U) { return 4; }

        // This is a valid illustrative JSON payload, NOT a governed LogRecord schema.
        constexpr std::array<std::byte, 2U> payload{
            std::byte{'{'}, std::byte{'}'}
        };
        std::array<std::byte, 64U> frame{};
        std::size_t written{0U};

        if (
            WriteEdlgFrame(
                EdlgCodec::Json,
                payload,
                frame,
                written,
                payload.size()
            ) != EdlgFrameWriteResult::Succeeded
        ) {
            return 5;
        }

        EdlgFrameView view{};
        if (
            ReadEdlgFrame(
                std::span<const std::byte>(frame.data(), written),
                view,
                payload.size()
            ) != EdlgFrameReadResult::Succeeded
        ) {
            return 6;
        }
        if (
            view.Codec != EdlgCodec::Json ||
            view.Payload.size() != payload.size() ||
            view.Payload[0U] != payload[0U] ||
            view.Payload[1U] != payload[1U] ||
            view.Consumed != written
        ) {
            return 7;
        }

        // Corruption must fail closed; a frame CRC is NOT an authenticity proof.
        frame[16U] ^= std::byte{1U};
        EdlgFrameView rejected{};
        if (
            ReadEdlgFrame(
                std::span<const std::byte>(frame.data(), written),
                rejected,
                payload.size()
            ) != EdlgFrameReadResult::ChecksumMismatch
        ) {
            return 8;
        }

        return 0;
    }

} // Demo

/// Debug-visible result of the minimal provider-free framing demonstration.
volatile int EdpLoggingFramingDemoResult{0};

extern "C" void app_main() {
    EdpLoggingFramingDemoResult = Demo::Run();
}
