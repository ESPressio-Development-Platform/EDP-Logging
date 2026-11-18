# Reference — `src/logging/Instrumentation.hpp`

**Classification:** PUBLIC API, compile-time-only expression gate and statement macro. [Exact source](../src/logging/Instrumentation.hpp). This header includes `Severity.hpp` and C++20 standard-library utilities only. It owns **neither** a record allocator nor a timestamp provider.

## Public result Type

`SiteInvocationResult : std::uint8_t` has **NotInstrumented (0)**, meaning the selected site was disabled and its thunk was not called, and **Invoked (1)**, meaning the enabled thunk was invoked once. Neither value says whether an actual record was admitted, represented, delivered or durably acknowledged. The separate typed admission result belongs to the producer/worker composition, not the compile-out gate.

## `InvokeLogSite<TMinimum,TSeverity,TThunk>(thunk)`

**`TMinimum`** is the compile-time severity threshold, **`TSeverity`** the actual emitted site level and **`TThunk`** an application-supplied callable Type. `TThunk` only needs to be callable with zero arguments for *enabled* sites. The argument is a forwarding reference; an enabled instantiation invokes it exactly once using `std::forward` and discards its return value, then returns `Invoked`. A disabled instantiation uses `if constexpr` to omit the invocation, casts the thunk expression to void, and returns `NotInstrumented`. It neither evaluates deferred message arguments nor obtains a time/identity, discovers a provider or enqueues a record.

The thunk's **captured expressions** must remain lazy; precomputing arguments before the call invalidates the zero-evaluation guarantee. Exceptions, lifetime and reentrancy follow the supplied callable; this wrapper makes no allocation-free claim about arbitrary user code.

## `EDP_LOG_WHEN_ENABLED(MINIMUM_THRESHOLD,EMITTED_SEVERITY,...)`

This statement macro wraps all `__VA_ARGS__` inside a zero-argument lambda and invokes the site template inside a `do { ... } while(false)` statement. `MINIMUM_THRESHOLD` and `EMITTED_SEVERITY` must be compile-time template-compatible constants; the variadic body holds **all** potential argument assembly, Clock capture, identity discovery, admission policy and producer work. It is not an expression macro, returns no typed admission outcome and must not be treated as the complete generated-schema-checked logger API. No shorter `EDP_LOG` macro is claimed by this header.

## Provider and compiler-definition boundaries

Only the consumer's selected site code determines actual provider dependencies; the default Logging umbrella remains C++20-only. The test fixture defines `EDP_LOGGING_G11_ENABLE_SITE=0|1` to compare linked code and zero-vs-on side effects; this is **not** an EDP-owned production configuration. The physical G11 test fixture separately uses `EDP_LOGGING_G11_APP_SITE_ENABLED`; see [Compiler Definitions](Compiler-Definitions) for defaults and scope.

GCC/Clang host tests, independent linked-symbol/elimination tests and the minimal three-target [demo](../demos/FramingAndInstrumentation/README.MD) have real finite receipts in the [G1–G15 acceptance matrix](../docs/ACCEPTANCE-G1-G15.MD). These do not establish universal full-firmware code-size costs, ISR safety or Flash durability.
