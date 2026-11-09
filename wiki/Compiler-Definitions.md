# Compiler and conditional compilation

This implementation requires C++20. EDP_LOG_WHEN_ENABLED provides a compile-time threshold gate and should eliminate disabled argument evaluation entirely. Application and test build variants must select the same canonical EDP owner dependencies and SDK configuration when comparing code size or timing.

## Boundaries

G11 site-level and whole-application compiler proofs are separate. Logging opt-in headers introduce dependencies only when imported. No platform-wide enabled logging severity or unbounded global queue is implied.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.

## Core demo selection

`FramingAndInstrumentation` adds no repository-defined compiler definitions. The C++20 `EDP_LOG_WHEN_ENABLED` macro uses template `SeverityThreshold`/emitted `Severity` constants chosen by the application, **not** preprocessor switches; it must leave disabled-site expression evaluation absent. The two PlatformIO projects explicitly request `-std=gnu++20`. `Arduino_IDE` requires a board/core toolchain configured for C++20. There is no hidden default threshold enable, universal diagnostic log site registration, or new provider-selection flag.
