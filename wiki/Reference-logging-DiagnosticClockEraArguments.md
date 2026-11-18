# Reference — `src/logging/DiagnosticClockEraArguments.hpp`

**Classification:** internal opt-in compile-time adapter for generated diagnostic arguments. [Reviewed source](../src/logging/DiagnosticClockEraArguments.hpp).

The explicit specialization of `Detail::IsCertifiedDiagnosticClockEra<ESPressio::Clock::ClockEra>` inherits `std::true_type`. It enables the [diagnostic argument dispatcher](Reference-logging-DiagnosticArgumentEncoding) to recognise exactly the EDP-Clock-owned `ClockEra` as a supported generated argument type when the consumer selects this header. The primary trait remains in `DiagnosticArgumentEncoding.hpp`. This header adds neither another trait parameter nor runtime state.

The specialization imports EDP-Serialisation's existing `ClockEraAdapters.hpp` for the canonical codec contract. Applications selecting this path must declare the opt-in Serialisation and Clock dependencies, and the generated EDP-Localisation parameter kind must agree with the exact type. Certification applies to type selection only. It supplies no observation of era origin, clock synchronization or coherence, and does not assign an era value. Those facts come from the selected clock and producer path. The trait itself has no runtime, memory, synchronization or provider lifetime cost.
