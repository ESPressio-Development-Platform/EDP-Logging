# src/logging/Severity.hpp

Public, platform-neutral C++20 primitive.

`Severity` is the emitted 0–5 diagnostic level (Trace, Debug, Info, Warning, Error, Fatal). It deliberately contains no Off state.

`SeverityThreshold` is the compile-time filtering level with numeric values 0–5 and Off=255. `DefaultSeverityThreshold` is Off. `SeverityEnabled<TMinimum,TSeverity>` uses a compile-time Boolean to gate future instrumented sites; it neither calls a clock nor evaluates arguments by itself. A full zero-evaluation caller-facing instrumentation macro remains to be implemented.

The file includes only `<cstdint>` and is independent of Platform and any concrete provider.
