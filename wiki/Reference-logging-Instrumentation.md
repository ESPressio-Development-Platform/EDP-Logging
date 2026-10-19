# src/logging/Instrumentation.hpp

Platform-neutral compile-time Logging site gate.

- `SiteInvocationResult` distinguishes `NotInstrumented` and `Invoked`, explicitly NOT admission success/failure.
- `InvokeLogSite<MinimumThreshold,EmittedSeverity>(thunk)` selects an instantiation through `if constexpr`, calls thunk once only when the compile-time threshold permits and never in disabled instantiations.
- `EDP_LOG_WHEN_ENABLED(MinimumThreshold, EmittedSeverity, statements...)` is a statement-only lazy wrapper. Put **all** potentially evaluated expressions (argument collection, Clock capture, provider selection, deadline construction) inside it.

The site gate alone does not claim any complete emitter, schema validation or queue work. Future Bootstrap Composition authoritatively selects minimum severity per library/facility, and future LogSite adapter performs type/sink admission. Disabled sites need no Logging runtime allocation. The source test `tests/InstrumentationTests.cpp` covers Off, below-threshold and exactly-once enabled semantics. It was **executed successfully on RPI400 with GCC and Clang** on 2026-10-18 (detached run `20261018T093004-0631a378`). The additional `tests/G11CompileOutProof.cpp` / `tests/run-g11-compile-out-proof.sh` compile both `EDP_LOGGING_G11_ENABLE_SITE=0` and `=1` settings from identical source, prove Clock/identity/argument side effects never execute in Off mode and execute once in enabled mode, and verify the disabled linked ELF omits the Clock-capture routine. RPI400 detached `20261018T094609-a2061881` passed with GCC, Clang and optional Xtensa object builds. `EDP_LOGGING_G11_ENABLE_SITE` is **test-only**, default undefined (the fixture intentionally demands an explicit 0 or 1), supplied by the proof runner via `-D`, and not a production Logging/Bootstrap feature flag. The host and Xtensa section deltas documented in `docs/WROOM-RESOURCE-QUALIFICATION.MD` do not represent a full application or board budget.


## G11 full-application callsite proof

The public EDP_LOG_WHEN_ENABLED compile-time lazy gate is used by one additional test-only site in [ESP32 HIL main](../tests/hil/g1-espidf/src/main.cpp). EDP_LOGGING_G11_APP_SITE_ENABLED (0 default, 1 Info, other values compile-error) is a **test build switch**, not a production API. Each source Clock/identity/argument operand is not evaluated when Off, and evaluated once per enabled site invocation. Native UART evidence contains one assertion for each of the two NVS-restart boots. This cannot be extrapolated to complete unused codec or sink elimination.
