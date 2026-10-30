#pragma once

#include <concepts>
#include <cstdint>
#include <optional>

#include <ESPressio_Clock.hpp>

namespace ESPressio::Logging {


    /// Whether an ordinary sink can continue after servicing owned pending data.
    enum class CoordinatorSinkServiceStatus : std::uint8_t {
        /// The existing owner can continue and may expose a pending deadline.
        Ready = 0U,
        /// Pending publication requires explicit Bootstrap reconciliation.
        RecoveryRequired = 1U
    };

    /// One optional sink-service observation, never a queue-admission ACK.
    struct CoordinatorSinkServiceOutcome final {
        /// Sticky uncertainty requires Bootstrap reconciliation and worker exit.
        CoordinatorSinkServiceStatus Status{CoordinatorSinkServiceStatus::Ready};

        /// Newly confirmed durable record completions for THIS sink since its prior poll.
        /// Counts are consumed once; summing multiple sinks does not count unique records.
        std::uint64_t DurableSinkCompletions{0U};

        /// Earliest required service in the SAME canonical origin as ThreadContext.
        /// No deadline means the existing coordinator finite idle fallback applies.
        std::optional<Clock::MonotonicTimestamp> NextDeadline{};
    };

    /// Optional callback on an existing ordinary sink; no execution provider is created.
    /// @tparam TSink Existing ordinary synchronous callback optionally exposing service.
    template <class TSink>
    concept CoordinatorServiceSink = requires(TSink& sink) {
        { sink.ServiceCoordinator() } noexcept -> std::same_as<CoordinatorSinkServiceOutcome>;
    };

} // namespace ESPressio::Logging
