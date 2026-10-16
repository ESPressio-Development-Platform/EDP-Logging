# EDP-Logging

Bounded, structured embedded diagnostic logging for the ESPressio Development Platform.

**Status — `feature/logging-v1`: partial implementation, NOT a production logging pipeline.** Severity, attempt tickets, EDLG/EDLS framing, partial Clock/diagnostic tooling, bounded Memory/Threading admission, tracked leases and now a transactional FIFO record queue are implemented as opt-in facilities. The complete governed LogRecord, unified producer, coherent Clock/Mesh source, production sinks and embedded durable archive remain incomplete. Do not use this library as a functioning end-to-end logging pipeline.

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

## Optional composed multi-producer wake coordinator (additional G1 tranche)

`logging/AdmissionWakeCoordinator.hpp` adds a fixed-slot, Bootstrap-composed wake fanout over the existing `AcquirePoolUntil` bounded admission seam. One producer owns one stable EDP-Threading wait/wake slot; no dynamic waiter registry or Memory finite-wait stack node is created. `AcquireUntil` preserves a single EDP-Clock deadline and reports grant/lateness and wake failures independently. `ReleaseAndNotify` releases a real EDP-Memory lease and wakes all fixed producer slots, preserving release-provider and notification failures. This remains an optional profile (not imported by `ESPressio_Logging.hpp`).

**Actual EDP-Memory/EDP-Threading composition and two-thread regression:** `EDP_LOGGING_DEPENDENCY_ROOT=/path/to/pinned-sibling-repositories CXX=g++ bash tests/run-composed-admission-tests.sh` (also `clang++`). On RPI400, both compilers passed 12 successive real multi-producer/full-capacity/wake-handoff cycles, wake-before-wait latching, expired deadline, notification failure after Memory release and native Memory mutex contention (detached run `20261016T073232-16bb29e5`). This does **not** prove full strict D2 multi-producer lifecycle, FIFO fairness, every independent RAII release notification, provider release deadline or HIL. See [precise contract and limitations](docs/ADMISSION-WAKE-COORDINATOR.MD), [Wiki](wiki/Reference-logging-AdmissionWakeCoordinator.md) and [Dependency Graph](DEPENDENCY_GRAPH.MD).

## Optional finite G1 admission shutdown and quiescence (additional tranche)

`logging/AdmissionLifecycleCoordinator.hpp` adds a **one-way shutdown flag**, cancellation-aware Memory-NoWait/Threading-WaitUntil admission loop, and finite event-driven draining of **in-flight admission calls** over a dedicated reserved Threading wait slot. Bootstrap must provide **at least producer-count+1 slots**, exclusively owned by distinct producer Tasks plus one drain caller. `ShutdownUntil<MaxDrainWakes>(singleMonotonicDeadline)` reports quiescence, timeout/retry/provider error, and wake-publication health independently. A Memory grant racing shutdown returns **`GrantedDuringShutdown` with its lease still owned**; it is never converted into a false timeout or silently discarded. Existing `AcquirePoolUntil` signature remains unchanged.

**This does not make G1 production-ready:** draining admission calls is not draining **outstanding leases** or joining Tasks; RAII lease destruction can still bypass capacity notifications, and native finite Memory acquisition remains unsafe. Tests use genuine EDP-Memory topology with concurrent producers and faulted wake providers. Native GCC and Clang PASS detached run `20261016T082446-9a9fb25f`. [Precise lifecycle contract](docs/ADMISSION-LIFECYCLE.MD), [Wiki](wiki/Reference-logging-AdmissionLifecycleCoordinator.md).

## Optional tracked G1 lease ownership and strict drain (additional tranche)

`logging/TrackedAdmissionCoordinator.hpp` adds an opt-in **nonmovable RAII lease guard** that privately owns the EDP-Memory ObjectPoolLease, pairs every successful explicit release or ordinary scope exit with composed EDP-Threading producer wake publication, and retains accurate outstanding-lease counts. `ShutdownTrackedUntil<MaxDrainWakes>(oneMonotonicDeadline)` can distinguish zero active calls from **still-outstanding tracked leases** and conservatively refuses a clean shutdown if an underlying native Memory release failed during guard destruction and the later native destructor's outcome could not be observed. A trailing notifier publishes a best-effort wake after native destruction to avoid a missed capacity wake if that retry succeeded. The original untracked G1 public surfaces remain unchanged.

This **does not solve all G1**: raw Memory leases outside this opt-in owner are not tracked, a native lease destructor can still fail without exposing its status, producer FIFO/fairness and end-to-end provider operation deadlines are not proven, and Bootstrap must join all Tasks. GCC/Clang real EDP-Memory/Threading host tests including destructor-only wake of a blocked producer passed RPI400 `20261016T085118-8d57dc8a`. See [implementation limitations](docs/TRACKED-ADMISSION-LEASES.MD), [Wiki](wiki/Reference-logging-TrackedAdmissionCoordinator.md) and [Dependency Graph](DEPENDENCY_GRAPH.MD).

## Optional G1 transactional record publication FIFO (new, partial)

`logging/TransactionalRecordQueue.hpp` now composes canonical EDP-BoundedTopology stable slot occupancy **and** intrusive FIFO over already-owned EDP-Memory ObjectPool leases, guarded by a Bootstrap-provided EDP-Threading OrdinaryMutex and notified via existing G1 bounded producer wake coordination. `Begin(lease,ticket)` reserves unpublished ownership; `Commit(ticket)` establishes the authoritative commit-order FIFO; `Abort(ticket)` returns reserved capacity; `Pop(ticket)` transfers exclusive consumer interest; `Complete(ticket)` releases the native Memory lease and publishes capacity availability. A generation-stamped ticket prevents reuse after reclamation, while a retirement claim prevents concurrent duplicate native release. No heap, private Signal, new Clock timeline, extra allocation bitmap or permanent Memory waiter registry is introduced.

**Move-only RAII reservation and consumer guards are now implemented**: `Begin(lease, Reservation&)` and `Pop(ConsumerClaim&)` return noncopyable, move-constructible handles that automatically abort unpublished claims or release completed source records at scope exit. Failed automatic release fails the queue's integrity state rather than pretending a clean drain. This is **still not the complete D12 producer**: one end-to-end Memory+queue admission Clock deadline, full coordinated shutdown after provider failure, governed production record envelope, sink fan-out and physical target qualification remain pending. Host GCC/Clang tests with actual EDP-Memory and EDP-Threading providers including duplicate-ticket concurrency have passed on RPI400. Use `CXX=g++ EDP_LOGGING_DEPENDENCY_ROOT=/path/to/pinned-siblings bash tests/run-transactional-record-queue-tests.sh` (also `clang++`). See [contract and limitations](docs/TRANSACTIONAL-RECORD-QUEUE.MD). This opt-in header is not part of the lightweight default umbrella.

## Optional G2 coherent Clock transfer/qualification seam (partial)

`logging/LogTemporalEvidence.hpp` introduces a strong EDP-Clock-only `CoherentLogClockSample` return-by-value contract, `CaptureLogTime(provider)` (exactly **one** composed provider capture), and `QualifyLogClockSample` to reject contradictory synchronized/ClockEra quality, distinguish Synchronized/Degraded/Unavailable/Incoherent, and always retain the original canonical Monotonic coordinate. A synchronized coordinate of zero is **valid**; availability uses explicit status, not a numeric sentinel.

**G2 is NOT complete**: the required concrete Clock/coordinator owner must still atomically capture Monotonic, synchronized value/uncertainty/state and upstream-assigned Era in one revision. Logging cannot derive Era or independently read three clocks. New C++20 strict GCC+Clang qualification tests passed on RPI400 (run `20261016T073929-8ef41bf6`); no production provider or HIL was tested. See [G2 contract](docs/COHERENT-TEMPORAL-EVIDENCE.MD), [Wiki](wiki/Reference-logging-LogTemporalEvidence.md) and [Dependencies](DEPENDENCY_GRAPH.MD).

## Optional G3 generated diagnostic scalar and bounded signature validation (partial)

`logging/DiagnosticArgumentEncoding.hpp` checks an application's **EDP-Localisation-generated** `DiagnosticMessageSchema<D,S,I>` declaration (active, count, exact ordered parameter widths) at C++20 compile time and then delegates to EDP-Serialisation's new `SerialisePositionalCbor/Json` owner APIs. No duplicate scalar encoder, heap, runtime variant, source coercion or default umbrella dependency. In addition to the original exact-width numeric/bool kinds, this opt-in helper now accepts owner-certified `Bounded::String<N>` (`Utf8`), `Bounded::Bytes<N>` (`Bytes`), `System::TypeIdentifier` and `System::FieldIdentifier`, with **strict generated parameter-kind/arity ordering** and a separate EDP-Serialisation certification check. Previous `ScalarDiagnosticSchemaMatches` remains source-compatible; `DiagnosticSchemaMatches` is the preferred extended query. Strong DeviceIdentifier, RuntimeIncarnationId, ClockEra, family-width StringIdentifier and custom semantic adapters remain unimplemented until the proper owner supplies reversible representations; **no invented TypeIdentifier or full LogRecord typed envelope**. GCC+Clang generated owner tests, negative swapped-kind compile failures and Xtensa compile-only coverage: `tests/run-diagnostic-bounded-generated-tests.sh`, `tests/DiagnosticBoundedXtensaSmoke.cpp`. [Bounded G3 contract](docs/DIAGNOSTIC-BOUNDED-ARGUMENTS.MD). Actual RPI400 interoperability with the EDP-Localisation **generated contract header** also passed using the owner-generated fixture; the script is `tests/run-diagnostic-generated-contract-tests.sh`, with `EDP_LOGGING_DEPENDENCY_ROOT` pointing at the pinned sibling checkouts (GCC and Clang). Precise source, governed-contract limits and GCC+Clang test commands: [G3 notes](docs/DIAGNOSTIC-ARGUMENT-ENCODING.MD), [Wiki](wiki/Reference-logging-DiagnosticArgumentEncoding.md).

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

## Optional G5 archived diagnostic schema host bridge (offline only)

`tools/edp_logging/historical_schema.py` adds **conditional host-only** Localisation historical family selection, without importing Localisation Python tooling into the embedded Logging library. It opens the exact caller-selected historical `ContractFamilyFingerprint` through EDP-Localisation's existing archive verifier, retrieves the pinned `diagnostic-schema.json`, and parses via EDP-Logging's existing strict schema loader. The record's Field12 diagnostic fingerprint is **different from the translation-sensitive ContractFamily fingerprint** and cannot uniquely identify historical translations. An external trusted archive-manifest SHA256 pin is optional and **separately reported**; internal hashes alone are NOT provenance. CLI flags and GCC-free Python regression: [G5 host bridge](docs/HISTORICAL-DIAGNOSTIC-SCHEMA-BRIDGE.MD), [Wiki](wiki/Tooling-edp-logging-historical-diagnostic.md) and `tests/run-historical-schema-tests.sh`. No G4 LogRecord root authority, MCU runtime decoder, hardware or durable archive claims.

The optional `--historical-language` CLI flag retrieves the **exact stored historic template** and language fallback, but deliberately does **not** interpolate argument values or invent presentation formats.

## G3 generated strong Device and Incarnation diagnostic arguments

`logging/DiagnosticArgumentEncoding.hpp` now recognizes **generated Localisation** `DeviceIdentifier` and `RuntimeIncarnationId` parameter kinds for their *exact* System strong types and routes both through the certified EDP-Serialisation reversible canonical owner adapter. CBOR uses binary-16 plus uint32; JSON uses Base64 plus decimal. Strong identity assignment/validity and full LogRecord root still governed elsewhere. Full contract: [System identity diagnostics](docs/DIAGNOSTIC-SYSTEM-IDENTITY-ARGUMENTS.MD), tests `tests/run-diagnostic-system-identity-tests.sh`. All versions remain `0.1.0`.

## Optional G3 generated ClockEra values

An opt-in `logging/DiagnosticClockEraArguments.hpp` profile imports the EDP-Serialisation owner-certified `ClockEra` ↔ `uint64_t` adapter, and uses the **real EDP-Localisation generated `clock-era`** diagnostic kind in strict compile-time positional matching. This is a conditional EDP-Clock abstract dependency only; no Mesh concrete, ClockEra issuance or synchronized-evidence qualification is implied. [Contract](docs/DIAGNOSTIC-CLOCK-ERA-ARGUMENTS.MD), [GCC/Clang owner suite](tests/run-diagnostic-clock-era-tests.sh), [Wiki](wiki/Reference-logging-ClockEraDiagnostic.md).

## Optional G1 queue-before-Memory admission (partial)

The optional [BoundedRecordAdmission.hpp](src/logging/BoundedRecordAdmission.hpp) composes queue-slot reservation, EDP-Memory NoWait acquisition, and the existing EDP-Threading producer wake with ONE absolute canonical EDP-Clock deadline. The queue entitlement is reserved BEFORE scarce Memory is acquired. An active move-only reservation is returned for later fill/commit or RAII abort, not sink delivery. Lifecycle's full-scope accounting prevents premature shutdown quiescence while a producer waits for queue capacity. This is a partial composer, not the completed governed LogRecord runtime/worker/sinks or on-device HIL qualification. Native finite EDP-Memory ObjectPool wait remains unsafe (EDP-Memory#16). See [contract](docs/BOUNDED-RECORD-ADMISSION.MD), [Wiki](wiki/Reference-logging-BoundedRecordAdmission.md), [dependency graph](DEPENDENCY_GRAPH.MD) and the strict GCC/Clang script tests/run-bounded-record-admission-tests.sh.

**G1 source safety qualification update:** the composed queue-before-Memory producer rejects the reserved shutdown-drainer slot before any wait or capacity claim and carries an independent producer-wake publication result even when the native Memory acquisition succeeds; see bounded admission documentation and its negative/fault-injection tests. Complete source/sink runtime and target physical validation are still outstanding.
