# EDP-Logging architecture and source state

**Implementation branch:** feature/logging-v1. **Version:** 0.1.0.

The complete agreed normative design and the seven-tranche runbook remain in `Policy/main/Handoff/2026-10-15/2026-10-15-11-46-EDP-Logging-Architecture-Living-Handoff.MD`. This document accurately describes only source currently available.

## Primitive semantics

Logging's diagnostic severities are exactly Trace, Debug, Info, Warning, Error and Fatal, with numeric values 0–5. Off belongs exclusively to the compile-time `SeverityThreshold`; no record can emit Off. The default selected threshold is Off.

One application chooses 16, 32 or 64 bits at compile time for all instrumentation attempts within an incarnation, default 32. Ticket value zero is invalid. The counter emits distinct values starting at 1, never wraps or resets, and returns `SequenceExhausted` after its largest value. It counts enabled attempts *before* capacity waiting, including attempts later refused. Ticket order does not imply FIFO publication order.

The present counter is storage and arithmetic only. A future Logging coordinator composes the owning EDP-Threading mutex for atomic issue, without assuming native 64-bit lock-free operations. The higher-level admission logic will ensure every enabled call issues a ticket before waiting for pool capacity and will support canonical EDP-Clock deadline semantics.

## Deliberately incomplete production and current source evolution

A guarded structural C++ LogRecord now owns fifteen numbered System Field
bindings (0–14), compile-time-selectable 16/32/64-bit attempt sequence,
bounded owned arguments and a validation predicate. It retains the permanent
EDP-System issued root identifier and satisfies System::SchemaType, but it
does **not** yet satisfy Serialisation::SerialisableType or implement the
approved typed CBOR/JSON wire envelope. Field 11 provenance and Field 14
record-fact sub-vocabularies are still unissued, with zero-initialized
opaque **in-memory** placeholders, not invented wire bits. The previously
missing complete wire record remains OPEN; no raw object dump is legal.
Genuine *coherent producer-owned* timestamp publication and complete
producer/worker/sink lifetime remain OPEN. Bounded admission, FIFO,
synchronous multi-sink fanout and a fail-closed file archive successor
do exist as independently tested opt-in prerequisites, but **not** as
a complete autonomous logging runtime.

The following descriptions are historical prerequisite snapshots and
must be read together with the current owner handoff, not treated as
a denial of later partial implementations:
 The optional G3 `logging/DiagnosticArgumentEncoding.hpp` now DOES compose EDP-Serialisation's native positional CBOR/JSON encoder and enforce the generated Localisation schema signature for native scalar and owner-certified bounded UTF-8/Bytes/System TypeIdentifier/FieldIdentifier arguments at compile time; it still does not encode a governed LogRecord root or support all generated strong parameter kinds. See `docs/DIAGNOSTIC-ARGUMENT-ENCODING.MD`. The optional `logging/LogTemporalEvidence.hpp` exposes and validates a single-capture EDP-Clock/ClockEra transfer contract with monotonic-safe fallback, but the required atomically coherent Clock/coordinator **producer** remains unimplemented and G2 stays open. See `docs/COHERENT-TEMPORAL-EVIDENCE.MD`. The **separate optional** `logging/BoundedAdmission.hpp` offers a tested *partial* finite admission-loop seam over EDP-Memory **NoWait** and EDP-Threading **WaitUntil**, and the newer optional `logging/AdmissionWakeCoordinator.hpp` adds a composed fixed-slot EDP-Threading wake fanout, `ReleaseAndNotify` and actual EDP-Memory/Threading multi-threaded host coverage. The latter is still **not** a complete Logging producer, FIFO, autonomous RAII-capacity-release notifier, cancellation lifecycle, or end-to-end G1 proof. The extra `logging/AdmissionLifecycleCoordinator.hpp` adds finite stop-aware admission and a reserved Threading drain slot, but it tracks **admission invocations only**, not outstanding Memory leases or joined producer tasks. See `docs/ADMISSION-LIFECYCLE.MD` and `docs/ADMISSION-WAKE-COORDINATOR.MD` for actual coverage and remaining hazards. See `docs/BOUNDED-ADMISSION.MD`. Those will be integrated from owner-level EDP contracts after prerequisite facilities are complete. The code here does not import platform-specific implementations.

## Test evidence

`tests/AttemptSequenceTests.cpp` contains C++20 compile-time checks, full 16-bit sequence exhaustion and 32/64-bit boundary examples. `tests/run-host-tests.sh` describes how to compile/run these. **Executable RPI400 host foundation validation was conducted on 2026-10-16:** GCC and Clang suites passed (detached runs `20261016T064416-0f4afd63` and `20261016T064424-db3864fd`); Python tooling tests passed separately. The optional `logging/BoundedAdmission.hpp` G1 source contract tests also passed separately under GCC and Clang with pinned Clock/Threading/Memory/Platform abstract dependencies (detached run `20261016T071414-d6ec50d8`). They test fake providers, not the full multi-producer integration. No target firmware, physical device, power-loss durability or end-to-end Logging runtime validation has been performed. AI-AGENT-02 was not accessed.

## C++ EDLG framing prerequisite

`WriteEdlgFrame` is an allocation-free, bounded C++20 writer around *already encoded* CBOR/JSON payload bytes. It does not interpret LogRecord schema or claim durability/cryptographic integrity. Header/trailer and CRC32C match the 20-byte D13.2 format and Python host tooling; one 23-byte C++/Python golden vector for `abc` passed as part of the RPI400 GCC/Clang foundation suites on 2026-10-16. Source and output spans must not alias. The helper is not a stand-alone Logging sink/coordinator and remains pending verified target builds.

## D4 compile-time site evaluation gate

`logging/Instrumentation.hpp` adds `InvokeLogSite<SeverityThreshold, Severity>(thunk)` and statement-style `EDP_LOG_WHEN_ENABLED(...)`. The entire instrumentation work is inside a lambda, so disabled sites do not evaluate any runtime expression (unlike taking ordinary function parameters before checking a gate). `SiteInvocationResult` distinguishes `NotInstrumented` from `Invoked` but is **NOT** an admission status. When enabled, the callable is invoked exactly once; future strongly typed Localisation argument compatibility and Memory admission are independent work. No provider is instantiated solely by the gate. Test source is authored only; compiler/target evidence is pending.

## C++ EDLG bounded reader

`src/logging/EdlgFrameReader.hpp` defines `ReadEdlgFrame(span<const byte>, EdlgFrameView&, maxPayload)`. No allocations: result contains a borrowed payload span and exact consumed bytes after deterministic header, codec, flags, finite length, completion marker and CRC32C checks; output unchanged on any failure. Caller must honor buffer lifetime and stop at first error (no implicit resynchronisation). `EdlgFrameTests.cpp` includes successful read and corrupt/truncated/oversized native-read vectors. Tests are source-only, not actually run yet.

## Opt-in tracked lease G1 ownership (still partial)

`logging/TrackedAdmissionCoordinator.hpp` retains the native Memory ObjectPoolLease inside a private RAII guard, calls composed ReleaseAndNotify on ordinary scope exit, and counts outstanding tracked leases as well as in-flight producer acquisitions. Its strict tracked drain cannot report quiescence while a lease remains or a failed native destructor release is unconfirmed. After a failure, it emits a best-effort wake after native Memory lease destruction without inventing successful release. **It does not account for raw leases outside this opt-in owner, join Tasks, certify FIFO fairness or change the unsafe direct finite native Memory acquisition.** See `docs/TRACKED-ADMISSION-LEASES.MD`.

## Incremental G3 owner-certified bounded diagnostics

The optional `DiagnosticArgumentEncoding.hpp` now recognizes generated `Utf8`/`Bytes` from `Bounded::String`/`Bounded::Bytes` and canonical `System::TypeIdentifier`/`FieldIdentifier`, with validation against the already-generated EDP-Localisation Type list and Serialisation owner `SerialisableType`. It adds no new owner dependencies or direct concrete imports; the previous scalar comparator remains as an alias. All other semantic identifier kinds require explicit reversible owner adapters and remain fail-closed. See `docs/DIAGNOSTIC-BOUNDED-ARGUMENTS.MD`.

## G3 generated device and incarnation strong values

The existing conditional generated argument adapter now recognizes `System::Identity::DeviceIdentifier` and `System::Identity::RuntimeIncarnationId` under exact Localisation declared kinds. No new Logging owner dependencies: Serialisation owns the existing binary/uint32 conversion, System owns strong identity types and provenance. Cannot call these valid LogRecord identities solely from successful encoding, and G4 governed TypeIdentifier remains unissued. See `docs/DIAGNOSTIC-SYSTEM-IDENTITY-ARGUMENTS.MD`.

## Optional generated ClockEra diagnostic parameter

The `logging/DiagnosticClockEraArguments.hpp` opt-in header enables an exact `ClockEra` strong value in generated `clock-era` parameter positions, through the EDP-Serialisation owner canonical `uint64_t` adapter. The default header remains Clock-independent. This adds no new Clock source or coordinator; the supplied ClockEra must already have been independently qualified by the upstream coherent temporal capture contract. See `docs/DIAGNOSTIC-CLOCK-ERA-ARGUMENTS.MD`.
