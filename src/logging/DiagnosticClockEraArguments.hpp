#pragma once

#include <serialisation/ClockEraAdapters.hpp>

#include "DiagnosticArgumentEncoding.hpp"

namespace ESPressio::Logging::Detail {

    /// Optional exact G3 diagnostic ClockEra type certification.
    /// Requires the existing EDP-Serialisation ↔ ClockEra canonical adapter
    /// and an explicit consumer dependency on EDP-Clock.
    ///
    /// This specialization NEVER qualifies ClockEra origin or time coherence.
    template<>
    struct IsCertifiedDiagnosticClockEra<ESPressio::Clock::ClockEra> final :
        std::true_type {};

} // namespace ESPressio::Logging::Detail
