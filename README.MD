# EDP-Logging

Bounded, structured embedded diagnostic logging for the ESPressio Development Platform.

**Status — `feature/logging-v1`: foundation-only implementation.** Severity and attempt ticket primitives exist; a complete LogRecord, producer admission, coherent EDP-Clock capture, queue, coordinator, sinks, archive and decoder are **not yet implemented**. Do not use this library as a functioning logging pipeline.

## Implemented API

`#include <ESPressio_Logging.hpp>` exposes:

- `Severity` — six emitted levels Trace=0 through Fatal=5; Off is never an emitted severity.
- `SeverityThreshold` and `SeverityEnabled<Minimum,Level>` — compile-time threshold check, default Off. The full zero-evaluation instrumentation macro is **not yet implemented**.
- `AttemptSequenceWidth::Bits16/Bits32/Bits64` with a compile-time default Bits32.
- `AttemptSequence<Width>` — strong invalid-zero ticket, exact 2/4/8-byte storage.
- `AttemptSequenceCounter<Width>` — no-wrap generator of 1..max per-incarnation attempt tickets, typed `Issued`/`OutputOccupied`/`SequenceExhausted`.

**Concurrency boundary:** `AttemptSequenceCounter::Issue` is intentionally **not thread-safe on its own** and must be invoked while the owning Logging runtime's single sequencer lock is held. Do not assume 64-bit C++ atomics are lock-free on all ESP32 variants. The future integrated producer composes the proper EDP-Threading mutex/wait semantics.

## Future architecture

The complete D1–D13 approved design and ordered seven-tranche implementation plan live in the authoritative [Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-10-15/2026-10-15-11-46-EDP-Logging-Architecture-Living-Handoff.MD). Package version remains exactly `0.1.0`. No implementation branch is a production release until host/target validation is actually run and recorded.

## Tests

`bash tests/run-host-tests.sh` is provided for a later authorised host execution environment. It covers ticket width/default and full 16-bit exhaustion. **Test source is committed but it has not been executed.** Neither RPI400 nor AI-AGENT-02 is available for use.

See [Architecture](docs/ARCHITECTURE.MD) and [Dependency Graph](DEPENDENCY_GRAPH.MD).

## Python host framing (partial, independent of target firmware)

The `tools/edp_logging` package (version **0.1.0**) provides strict incremental `EDLG` frame validation, finite forensic resynchronisation, `EDLS` segmented-header validation and a structural CLI. See [Host Decoder](docs/HOST-DECODER.MD). It does **not** yet interpret typed LogRecord arguments or original translations. No tests have been executed.

## Important correctness boundary

The scanner validates framing and the optional structural Typed Envelope only. Full LogRecord semantics require EDP-System-issued root TypeIdentifier and historical governed argument schema. The JSON reader rejects duplicate member names (rather than silently keeping the last), while CBOR requires canonical integer lengths/FieldId order. No typed args or translations are inferred when schema is missing. A separate malformed inner record produces a `recordError` alongside verified frame location rather than destroying capture provenance.

## Source-only EDLG v1 framing

`logging/EdlgFrame.hpp` provides `WriteEdlgFrame` over caller-owned spans: binary 20-byte overhead, CRC32C, little-endian length, completion marker and preflight capacity checking. It formats no LogRecord semantics. `EdlgFrameTests.cpp` supplies a byte-for-byte golden fixture also usable with Python host tooling. No build or execution test has run.

## Portable EDLS segment header

`src/logging/EdlsSegment.hpp` and `tools/edp_logging/archive.py` now encode/decode the same explicit 40-byte EDLS segment header, including generation, device/incarnation and CRC32C. C++/Python golden vectors and negative tests are authored. This is a format contract, not a durable archive implementation or a validated interoperable build.

## Archive slot inspection (read-only)

Python `inspect_archive_slots` validates a finite explicitly named archive topology, identifies known generations conservatively and reports only contiguous verified frame prefixes. It refuses confident generation advancement with missing/corrupt/ambiguous evidence; damaged tail is never an append target. Does not replace future production EDP-Persistence archive rotation/recovery, and no executable proof has been obtained.

## Compile-time disabled-site evaluation

The source now includes an explicitly limited `EDP_LOG_WHEN_ENABLED(Minimum, Severity, statements...)` macro wrapping a lazy `InvokeLogSite` template. Disabled sites do not evaluate time/provider/policy/argument expressions because those are located within the discarded callable. Enabled sites execute the thunk once. It does NOT yet submit a LogRecord, type-check message schemas or provide sink delivery; it is a D4 compile-out prerequisite. An authored host test probes disabled/filtered side effects and enabled once-only evaluation; no executable validation yet.

## Allocation-free native EDLG reader

`ReadEdlgFrame` now validates the canonical EDLG envelope directly from caller-owned bytes and returns a borrowed payload span only after complete CRC/trailer/version/length validation. It refuses malformed frames instead of scanning past them, preserves the output view on failure, and reports exact consumed bytes for safe contiguous-prefix inspection. This is not the EDP-Persistence archive coordinator, a CBOR LogRecord decoder or hardware validation. Negative source tests are added but not yet executed.

## Read-only chronological analysis primitives

`tools/edp_logging/analysis.py` now offers bounded per-device/incarnation attempt-sequence evidence and uncertainty-aware time-order comparisons. It does not mistake observed gaps for proven loss stages, or incomparable Clock Eras for timestamps on a shared UTC clock. See the host-decoder contract and outstanding tests.
