# src/logging/Severity.hpp

Public, platform-neutral C++20 primitive.

`Severity` is the emitted 0–5 diagnostic level (Trace, Debug, Info, Warning, Error, Fatal). It deliberately contains no Off state.

`SeverityThreshold` is the compile-time filtering level with numeric values 0–5 and Off=255. `DefaultSeverityThreshold` is Off. `SeverityEnabled<TMinimum,TSeverity>` uses a compile-time Boolean to gate future instrumented sites; it neither calls a clock nor evaluates arguments by itself. The existing public site gate and zero-evaluation statement macro EDP_LOG_WHEN_ENABLED are implemented in Instrumentation.hpp. A schema-checked complete LogRecord-producing short-form logging macro is a separate API and is not implied.

The file includes only `<cstdint>` and is independent of Platform and any concrete provider.
