# Compiler and conditional compilation

This implementation requires C++20. EDP_LOG_WHEN_ENABLED provides a compile-time threshold gate and should eliminate disabled argument evaluation entirely. Application and test build variants must select the same canonical EDP owner dependencies and SDK configuration when comparing code size or timing.

## Boundaries

G11 site-level and whole-application compiler proofs are separate. Logging opt-in headers introduce dependencies only when imported. No platform-wide enabled logging severity or unbounded global queue is implied.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.

## Core demo selection

`FramingAndInstrumentation` adds no repository-defined compiler definitions. The C++20 `EDP_LOG_WHEN_ENABLED` macro uses template `SeverityThreshold`/emitted `Severity` constants chosen by the application, **not** preprocessor switches; it must leave disabled-site expression evaluation absent. The two PlatformIO projects explicitly request `-std=gnu++20`. `Arduino_IDE` requires a board/core toolchain configured for C++20. There is no hidden default threshold enable, universal diagnostic log site registration, or new provider-selection flag.

## G11 test-only compiler definitions (source-owned versus fixture-owned)

`EDP_LOGGING_G11_ENABLE_SITE` is defined **only** by the host `tests/run-g11-compile-out-proof.sh` `-D` for `tests/G11CompileOutProof.cpp`; the fixture intentionally requires an explicit value **0** (disabled site) or **1** (enabled). There is no production default; the paired linked-symbol and side-effect tests compare the exact same source with only this fixture value changed. It is not a Bootstrap severity setting or a globally enabled Logging runtime.

`EDP_LOGGING_G11_APP_SITE_ENABLED` is an independent **test-only** definition for the ESP-IDF HIL `tests/hil/g1-espidf/src/main.cpp`: **0** disables the instrumented proof site (its effective fixture default), **1** enables the Info-level proof; values outside the supported set are rejected at compilation by that fixture. It measures a single site in a constrained target, not a universal application binary footprint or a new production compiler knob.

There are no new repository-defined public `#define` configuration switches in `Severity.hpp` or `Instrumentation.hpp`: the supported `EDP_LOG_WHEN_ENABLED` macro takes **compile-time template arguments**, not an external preprocessor severity knob.
