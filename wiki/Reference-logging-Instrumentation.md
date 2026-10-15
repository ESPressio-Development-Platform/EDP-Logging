# src/logging/Instrumentation.hpp

Platform-neutral compile-time Logging site gate.

- `SiteInvocationResult` distinguishes `NotInstrumented` and `Invoked`, explicitly NOT admission success/failure.
- `InvokeLogSite<MinimumThreshold,EmittedSeverity>(thunk)` selects an instantiation through `if constexpr`, calls thunk once only when the compile-time threshold permits and never in disabled instantiations.
- `EDP_LOG_WHEN_ENABLED(MinimumThreshold, EmittedSeverity, statements...)` is a statement-only lazy wrapper. Put **all** potentially evaluated expressions (argument collection, Clock capture, provider selection, deadline construction) inside it.

The site gate alone does not claim any complete emitter, schema validation or queue work. Future Bootstrap Composition authoritatively selects minimum severity per library/facility, and future LogSite adapter performs type/sink admission. Disabled sites need no Logging runtime allocation. A host source test covers Off, below-threshold and exactly-once enabled semantics but is **not executed**.
