# Reference — `src/logging/Severity.hpp`

**Classification:** PUBLIC API; platform-neutral C++20 constants and compile-time predicates. [Exact source](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/8d2b307a8a1db1aa5596abc904cd0da1f3c032cd/src/logging/Severity.hpp). Only `<cstdint>` is included: this primitive imports no EDP-Platform, Clock, Memory, Threading, Serialization, or concrete provider.

## Public vocabulary and every enum value

`Severity : std::uint8_t` represents a severity **actually emitted** by an application-selected diagnostic site. Its values are **Trace (0)**, fine-grained flow; **Debug (1)**, developer diagnostics; **Info (2)**, ordinary noteworthy operation; **Warning (3)**, recoverable unexpected condition; **Error (4)**, operation failure; and **Fatal (5)**, severe failure requiring application-level response. These labels are ordered to support compile-time threshold comparisons; they are not hardwired to any sink, policy or logger buffer. `Off` is intentionally not a valid emitted `Severity`.

`SeverityThreshold : std::uint8_t` chooses the **minimum enabled site**. **Trace (0)** enables all emitted levels; **Debug (1)** excludes Trace; **Info (2)** excludes Trace/Debug; **Warning (3)** excludes lower three; **Error (4)** includes Error/Fatal; **Fatal (5)** enables only Fatal; and **Off (255)** excludes all sites. These values distinguish a configured level threshold from the actual emitted severity. `DefaultSeverityThreshold = SeverityThreshold::Off` is a public compile-time constant; no mutable global severity, hidden producer or runtime lookup exists.

## Compile-time site test

`SeverityEnabled<TMinimum, TSeverity>` is an `inline constexpr bool` variable template. **`TMinimum`** is the application/Bootstrap-chosen compile-time threshold; **`TSeverity`** is the emitted site's severity. The predicate is true **only** when the selected minimum is not Off and the emitted level's ordinal is greater than or equal to it. The predicate itself performs no I/O, time reading, allocation or argument evaluation. It cannot prove record admission, sink acceptance or durable acknowledgement.

The lazy `InvokeLogSite` wrapper and `EDP_LOG_WHEN_ENABLED` macro are documented separately in [Instrumentation](Reference-logging-Instrumentation). Their disabled branch avoids invoking producer/Clock/argument work when those expressions are **inside** the thunk. A conditional compile test is not a guarantee that unrelated application code, accidentally evaluated before the macro, is eliminated.

## Resource and thread semantics

All three declarations are compile-time value facilities; no instance lifetime, synchronization, ISR operation, heap allocation or task is introduced. They may be referenced from ordinary or ISR code as constants, but this does **not** certify an actual logging site or any sink as ISR-safe. GCC/Clang owner tests and the generated dependency-free three-target [Framing and Instrumentation demo](../demos/FramingAndInstrumentation/README.MD) exercise the correct cases; only separately qualified native runtime runs prove physical behaviour.
