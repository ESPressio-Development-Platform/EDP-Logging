# EDP-Logging architecture and source state

**Implementation branch:** feature/logging-v1. **Version:** 0.1.0.

The complete agreed normative design and the seven-tranche runbook remain in `Policy/main/Handoff/2026-10-15/2026-10-15-11-46-EDP-Logging-Architecture-Living-Handoff.MD`. This document accurately describes only source currently available.

## Primitive semantics

Logging's diagnostic severities are exactly Trace, Debug, Info, Warning, Error and Fatal, with numeric values 0–5. Off belongs exclusively to the compile-time `SeverityThreshold`; no record can emit Off. The default selected threshold is Off.

One application chooses 16, 32 or 64 bits at compile time for all instrumentation attempts within an incarnation, default 32. Ticket value zero is invalid. The counter emits distinct values starting at 1, never wraps or resets, and returns `SequenceExhausted` after its largest value. It counts enabled attempts *before* capacity waiting, including attempts later refused. Ticket order does not imply FIFO publication order.

The present counter is storage and arithmetic only. A future Logging coordinator composes the owning EDP-Threading mutex for atomic issue, without assuming native 64-bit lock-free operations. The higher-level admission logic will ensure every enabled call issues a ticket before waiting for pool capacity and will support canonical EDP-Clock deadline semantics.

## Deliberately absent

No production invocation/macro, identity attachment, source timestamp snapshot, owned typed arguments, LogRecord schema, native API/EDP-Serialisation wrapper, FIFO, worker, sink or archive exists yet. Those will be integrated from owner-level EDP contracts after prerequisite facilities are complete. The code here does not import platform-specific implementations.

## Test evidence

`tests/AttemptSequenceTests.cpp` contains C++20 compile-time checks, full 16-bit sequence exhaustion and 32/64-bit boundary examples. `tests/run-host-tests.sh` describes how to compile/run these. **No executable test/CI/device validation was conducted**; RPI400 and AI-AGENT-02 remain unavailable.

## C++ EDLG framing prerequisite

`WriteEdlgFrame` is an allocation-free, bounded C++20 writer around *already encoded* CBOR/JSON payload bytes. It does not interpret LogRecord schema or claim durability/cryptographic integrity. Header/trailer and CRC32C match the 20-byte D13.2 format and Python host tooling; one 23-byte C++/Python golden vector for `abc` is committed but unexecuted. Source and output spans must not alias. The helper is not a stand-alone Logging sink/coordinator and remains pending verified target builds.

## D4 compile-time site evaluation gate

`logging/Instrumentation.hpp` adds `InvokeLogSite<SeverityThreshold, Severity>(thunk)` and statement-style `EDP_LOG_WHEN_ENABLED(...)`. The entire instrumentation work is inside a lambda, so disabled sites do not evaluate any runtime expression (unlike taking ordinary function parameters before checking a gate). `SiteInvocationResult` distinguishes `NotInstrumented` from `Invoked` but is **NOT** an admission status. When enabled, the callable is invoked exactly once; future strongly typed Localisation argument compatibility and Memory admission are independent work. No provider is instantiated solely by the gate. Test source is authored only; compiler/target evidence is pending.
