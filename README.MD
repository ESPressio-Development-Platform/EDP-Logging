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

## Optional G1 bounded pool admission seam (partial)

The opt-in header `#include <logging/BoundedAdmission.hpp>` adds `AcquirePoolUntil<TMaximumWakeRetries>`: a bounded, non-polling, EDP-Clock-deadline-aware bridge using **only EDP-Memory NoWait claims** and the existing **EDP-Threading WaitUntil** wake mechanism. It does **not** invoke Memory's currently unbounded finite ObjectPool path. A caller supplies one exclusive lifetime-stable Threading wait point, one empty Memory pool lease and one immutable canonical monotonic deadline. Wake/retry counts are compile-time bounded. A grant which finishes after the deadline returns `GrantedAfterDeadline` **with the lease still owned**; the caller must release it explicitly.

This is a **prerequisite prototype only**, not a complete Logging producer, queue, fair multi-producer wake coordinator or complete G1 proof. Correct release and competing-claim wake notification is a separate owner obligation. Memory finite stack-waiter cleanup remains unresolved. See [bounded admission design/validation](docs/BOUNDED-ADMISSION.MD), [optional Wiki reference](wiki/Reference-logging-BoundedAdmission.md) and [Dependency Graph](DEPENDENCY_GRAPH.MD). The header is deliberately excluded from `ESPressio_Logging.hpp` so ordinary severity/sequence-only builds remain dependency-free. Apps opting in declare EDP-Clock, EDP-Memory, EDP-Threading and EDP-Platform abstractions directly; concrete providers stay under Bootstrap Composition.

## Future architecture

The complete D1–D13 approved design and ordered seven-tranche implementation plan live in the authoritative [Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-10-15/2026-10-15-11-46-EDP-Logging-Architecture-Living-Handoff.MD). Package version remains exactly `0.1.0`. No implementation branch is a production release until host/target validation is actually run and recorded.

## Tests

`bash tests/run-host-tests.sh` covers ticket width/default, 16-bit exhaustion, framing/segment integrity and compile-out behaviour. **The current foundation suites passed on RPI400 using GCC and Clang (2026-10-16)**, as did the Python host decoder suite. These are not complete logging-runtime or physical-device results. AI-AGENT-02 was not accessed.

The additional **optional** G1 host test can run with `EDP_LOGGING_DEPENDENCY_ROOT=/path/to/sibling-repositories CXX=g++ bash tests/run-bounded-admission-tests.sh` (repeat with `clang++`). GCC/Clang passed on RPI400 on 2026-10-16 (detached run `20261016T071414-d6ec50d8`). This suite currently proves the bounded algorithm's contract with fake providers; it does not prove multi-producer MemoryTopology integration. See [Architecture](docs/ARCHITECTURE.MD), [G1 bridge scope](docs/BOUNDED-ADMISSION.MD) and [Dependency Graph](DEPENDENCY_GRAPH.MD).

## Python host framing (partial, independent of target firmware)

The `tools/edp_logging` package (version **0.1.0**) provides strict incremental `EDLG` framing, finite forensic resynchronisation, `EDLS` header validation, candidate outer-field and schema-governed **CBOR** diagnostic argument inspection, and a structural CLI. The optional `--schema-sha256` pins exact `diagnostic-schema.json` bytes to a separately trusted manifest SHA-256; without a pin, declared schema digests are not independently verified. See [Host Decoder](docs/HOST-DECODER.MD). JSON semantic argument decoding, translations, operational log producer/sinks and hardware validation remain pending. The 20 Python host tests passed on RPI400 on 2026-10-16.

## Important correctness boundary

The scanner validates framing and the optional structural Typed Envelope only. Full LogRecord semantics require EDP-System-issued root TypeIdentifier and historical governed argument schema. The JSON reader rejects duplicate member names (rather than silently keeping the last), while CBOR requires canonical integer lengths/FieldId order. No typed args or translations are inferred when schema is missing. A separate malformed inner record produces a `recordError` alongside verified frame location rather than destroying capture provenance.

## Source-only EDLG v1 framing

`logging/EdlgFrame.hpp` provides `WriteEdlgFrame` over caller-owned spans: binary 20-byte overhead, CRC32C, little-endian length, completion marker and preflight capacity checking. It formats no LogRecord semantics. `EdlgFrameTests.cpp` supplies a byte-for-byte golden fixture also usable with Python host tooling. Existing C++20 GCC/Clang foundation suites passed on RPI400; device validation remains outstanding.

## Portable EDLS segment header

`src/logging/EdlsSegment.hpp` and `tools/edp_logging/archive.py` now encode/decode the same explicit 40-byte EDLS segment header, including generation, device/incarnation and CRC32C. C++/Python golden vectors and negative tests passed in their respective RPI400 host suites. This is a format contract, not a durable archive implementation or validated firmware interoperability.

## Archive slot inspection (read-only)

Python `inspect_archive_slots` validates a finite explicitly named archive topology, identifies known generations conservatively and reports only contiguous verified frame prefixes. It refuses confident generation advancement with missing/corrupt/ambiguous evidence; damaged tail is never an append target. Does not replace future production EDP-Persistence archive rotation/recovery; host Python tests passed, but physical archive validation remains outstanding.

## Compile-time disabled-site evaluation

The source now includes an explicitly limited `EDP_LOG_WHEN_ENABLED(Minimum, Severity, statements...)` macro wrapping a lazy `InvokeLogSite` template. Disabled sites do not evaluate time/provider/policy/argument expressions because those are located within the discarded callable. Enabled sites execute the thunk once. It does NOT yet submit a LogRecord, type-check message schemas or provide sink delivery; it is a D4 compile-out prerequisite. A host test probes disabled/filtered side effects and enabled once-only evaluation; it passed on RPI400 under GCC and Clang in the foundation suite.

## Allocation-free native EDLG reader

`ReadEdlgFrame` now validates the canonical EDLG envelope directly from caller-owned bytes and returns a borrowed payload span only after complete CRC/trailer/version/length validation. It refuses malformed frames instead of scanning past them, preserves the output view on failure, and reports exact consumed bytes for safe contiguous-prefix inspection. This is not the EDP-Persistence archive coordinator, a CBOR LogRecord decoder or hardware validation. Negative source tests passed in the RPI400 host suites; no physical target test was executed.

## Read-only chronological analysis primitives

`tools/edp_logging/analysis.py` now offers bounded per-device/incarnation attempt-sequence evidence and uncertainty-aware time-order comparisons. It does not mistake observed gaps for proven loss stages, or incomparable Clock Eras for timestamps on a shared UTC clock. See the host-decoder contract and outstanding tests.
