# EDP-Logging

Bounded, structured embedded diagnostic logging for the ESPressio Development Platform.

**Status — `feature/logging-v1`: partial implementation, NOT a production logging pipeline.** Severity, attempt tickets, EDLG/EDLS framing, partial Clock/diagnostic tooling, bounded Memory/Threading admission, tracked leases and now a transactional FIFO record queue are implemented as opt-in facilities. The complete governed LogRecord, unified producer, optional coherent synchronized Clock source, production sinks and embedded durable archive remain incomplete. Do not use this library as a functioning end-to-end logging pipeline.

## Implemented API

`#include <ESPressio_Logging.hpp>` exposes:

- `Severity` — six emitted levels Trace=0 through Fatal=5; Off is never an emitted severity.
- `SeverityThreshold` and `SeverityEnabled<Minimum,Level>` — compile-time threshold check, default Off. The lazy EDP_LOG_WHEN_ENABLED macro is available, but the governed LogRecord producer remains unfinished.
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

## Optional generated-schema-bound LogRecord source assembly (partial G3/G4)

`logging/LogRecordSourceAssembly.hpp` now constructs one canonical **structural**
LogRecord using the actual EDP-Localisation-generated message identity,
128-bit diagnostic fingerprint, already-issued attempt, coherent
EDP-Clock temporal evidence, System device/incarnation and bounded
source-owned schema-certified arithmetic, UTF-8, Bytes, TypeIdentifier,
FieldIdentifier and DeviceIdentifier positional arguments. Construction
is transactional and performed before queue waiting. It does **not**
serialise any complete 15-field Typed Envelope; Fields 11 and 14
remain unissued zero-only facts. See the focused
[source assembly contract](docs/LOG-RECORD-SOURCE-ASSEMBLY.MD)
and [Wiki reference](wiki/Reference-logging-LogRecordSourceAssembly.md).

## Bounded G1/G10 coordinator drain quantum (partial)

`logging/CoordinatorQuantum.hpp` adds a compile-time bounded drain
budget and typed multi-sink/reclaim accounting on the existing RAII
`TransactionalRecordQueue`. Caller-owned unresolved claims are
preserved rather than discarded. This is a synchronous building
block; no actual Dedicated Thread lifecycle or async format fork
is yet integrated. See
[record production ownership](docs/RECORD-PRODUCTION-OWNERSHIP.MD).

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

## Physical ESP32-WROOM-32 G1 qualification (opt-in feature only)

The optional queue-before-Memory G1 composer was physically tested on RPI400's actual ESP32-D0WD using real EDP-Memory ObjectPool, EDP-Threading/FreeRTOS wake and mutex providers and a Bootstrap-composed EDP-Clock timebase. Three independent physical reset/run cycles **PASS** the record-pool/FIFO ownership, CPU1 producer-wake, dedicated shutdown wake-slot isolation and active producer cancellation/teardown cases. The **entire 4 MiB original firmware was backed up and restored with independent byte-identical digest verification before final normal reboot**. Build/run logs and exact source/target limitations: [HIL project](tests/hil/g1-espidf/README.MD), [durable evidence](tests/hil/g1-espidf/VALIDATION.MD) and [Wiki](wiki/Testing-G1-ESP32-Physical.md). **This is not evidence of complete Logging producer, sink, G2/G4 or durable persistence behaviour.**

## Optional G1/G10 standalone Clock production/consumer handoff (physical partial)

The optional source header logging/RecordProduction.hpp composes a dedicated
EDP-Threading sequencer, one injected EDP-Clock coherent temporal capture,
EDP-Memory record constructor, queue-first bounded transactional admission,
and one explicitly owned sink submission/reclamation path. EDP-Mesh is NOT
imported or needed: monotonic-only standalone Clock works correctly.

The new GCC+Clang host tests and three extended real ESP32 runs prove bounded
generic record production, actual FreeRTOS/Memory FIFO ownership, genuine
EDP-Serialisation CBOR, canonical EDLG CRC output over UART, source lease
reclamation and sink rejection accounting. The CBOR record is a TEST-ONLY
positional tuple, not a governed LogRecord. Permanent EDP-System TypeIdentifier,
full field envelope, multi-sink delivery, durable archives and power-cut
recovery remain outstanding. See docs/RECORD-PRODUCTION-OWNERSHIP.MD,
tests/hil/g1-espidf/VALIDATION.MD and Wiki RecordProduction reference.

## Optional bounded EDLG Persistence checkpoint and concrete Serial HIL

[EDLG KeyValueStorage checkpoint](docs/EDLG-KEYVALUE-CHECKPOINT.MD) provides
one optional, checksum-checked single-frame save/recover path over the existing
**abstract EDP-Persistence KeyValueStorage** contract. It cannot claim an
EDLS historical archive or durable storage stronger than the selected
Bootstrap provider. GCC/Clang host fault tests live in
tests/run-edlg-checkpoint-tests.sh; the physical ESP32 test app selects
EDP-Persistence-ESP-IDF NVS and EDP-Serial-ESP-IDF's UART FIFO writer via
Bootstrap only, proving NVS checkpoint readback after runtime restart and
owner-Serial binary EDLG transmission to an independent host parser.
EDP-Logging itself imports neither Serial nor either concrete provider.


## Optional multi-sink fanout and read-only EDLS inspection

[Record production ownership](docs/RECORD-PRODUCTION-OWNERSHIP.MD)
documents the bounded one-to-eight sink call interface, which reuses
one EDP-Memory claim and records each sink outcome without pretending
acceptance equals durability. G10 sink-count parity now extends through
record drain, coordinator quantum and topology-owned DedicatedCoordinator
(see #10): single-sink Serial-only and archive-only profiles are valid,
while zero sinks remain rejected at compile time.
[EDLS archive inspection](docs/EDLS-ARCHIVE-INSPECTION.MD)
is a bounded, read-only EDP-Persistence::FileStorage adapter using existing
canonical EDLS/EDLG CRC readers; it is **not** the rolling writer.

[EDLS existing-slot checked append](docs/EDLS-ARCHIVE-APPEND.MD)
adds one optional, strictly preflighted EDP-Persistence FileStorage
AppendFile operation. This is not an atomic rotating archive writer and
does not qualify external power-loss durability without a provider
whose actual retention/atomicity contract supports it.

## Optional conservative EDLS successor publication

[EDLS safe successor](docs/EDLS-SAFE-SUCCESSOR.MD)
extends the existing abstract FileStorage scanner and checked append
with one write-verified generation in a **previously empty** slot.
It refuses occupied-slot overwrite, ambiguous history and generation
exhaustion. It cannot be marketed as full circular/wear rotation or
as an independently durable power-cut recovery guarantee.


## Permanent universal LogRecord Type identity (G4 issuance)

The platform owner formally allocated the EDP Core Authority
`0x000001` and local Type `0x0000000001` on 2026-10-17.
Opt-in [LogRecord identity root](src/logging/LogRecord.hpp) declares the
permanent 8-byte `00 00 01 00 00 00 00 01` identity as
`System::TypeIdentifier`, with governance in the
[System registry](https://github.com/ESPressio-Development-Platform/EDP-System/blob/main/docs/TYPE-IDENTITY-REGISTRY.MD).
The identity now also carries **structural C++ bindings** for all
fifteen agreed Field IDs 0–14 and qualifies as `System::SchemaType`.
Application-selected finite sequence widths and owned argument
capacities are compile-time parameters. However Field 11 and 14
wire vocabularies remain unissued: this is **not yet**
a `Serialisation::SerialisableType`, canonical CBOR/JSON
envelope, fully owned task/sink pipeline, or durable archive.
See [LogRecord structural gate](docs/LOG-RECORD-IDENTITY.MD).

## Native EDP-Threading dedicated Logging coordinator (G10 partial)

Opt-in `logging/DedicatedCoordinator.hpp` binds one bounded
source-drain callable to an application-declared
`EDP-Threading::DedicatedThread`. It uses the **same native
ThreadContext wake/stop lifecycle**, never a parallel queue or task
provider; per-sink submission and independent Memory-reclaim reports
remain explicit. A complete production Logging pipeline still
requires the approved 15-field typed wire, real sink-specific
bounded encoding buffers and truthful persistence/Serial
semantics. See [dedicated coordinator](docs/DEDICATED-COORDINATOR.MD).


**Dedicated Thread join context:** `Thread::JoinFor/JoinUntil` require
an EDP-managed Threading caller. An external ESP-IDF Bootstrap
must observe `StaticThreadingRuntime::IsExecutionQuiescent()`
with its own finite deadline before terminal teardown. It must
not introduce an unmanaged join waiter or claim a foreign native
join succeeded. See the G10 dedicated coordinator contract.

## Bounded encoded representation fanout (G10 subset)

`logging/OwnedRepresentationFanout.hpp` composes an actual
EDP-Memory ObjectPool lease for one per-profile CBOR/JSON encoded
buffer, a caller-selected EDP-Serialisation encoder, and 2–8
independent *copying* sinks. Strict NoWait pool admission,
one encode per selected representation, explicit per-sink results,
native release-status and retained failed leases avoid hidden
allocation, source-buffer escaping and fake durability claims.
The test includes real EDP-Serial bounded FIFO owned-copy
backpressure and independently checked EDLG frame CRC.
See [owned representation fanout](docs/OWNED-REPRESENTATION-FANOUT.MD).

**G10 one-sink composition (issue #7):** an application may select exactly
one synchronous copying sink (e.g. Serial-only or archive-only) without a
dummy second sink. The same bounded EDP-Memory-owned representation is encoded
once and remains borrowed only for the duration of that synchronous callback.
The fanout still supports up to eight sinks and does NOT provide asynchronous
downstream shared ownership or durability. See the G10 Wiki reference.

## WROOM whole-app resource HIL (G11 partial)

The ESP-IDF test app records actual 8-bit free/minimum heap,
test-task stack margin and elapsed time alongside a mandatory
18th physical PASS gate. Three native tests with a HIL-only
8192-byte app_main stack passed 18/18 and raised margin to
5056-5072 bytes compared with 448 at the 3584-byte SDK default.
These are whole-application test-resource observations, not
isolated Logging footprint or final production CPU/flash budgets.
See docs/WROOM-RESOURCE-QUALIFICATION.MD.


## G11 dual whole-firmware compile-out qualification (test-only)

The integrated ESP32-WROOM HIL has one additional compile-time-selected lazy instrumentation site. The test flag EDP_LOGGING_G11_APP_SITE_ENABLED defaults to 0 (Off); 1 enables an Info-level site, and other values are rejected at compilation. The switch is not a public Logging configuration API. The original 18 functional HIL markers are unchanged.

The [whole-firmware comparator](tests/run-g11-whole-firmware-comparison.sh) builds Off/On from identical source and an explicitly pinned EDP_LOGGING_DEPENDENCY_ROOT using a new G11_REPORT_DIR. Its source, SDK configuration, ELF sizes, factory image hashes and tool versions are captured for reproducibility. The [native site verifier](tests/hil/g1-espidf/g11_site_evidence.py) validates both site-marker observations per full NVS-restarting UART session. The [Python negative suite](tests/test_g11_site_evidence.py) checks wrong modes, missing/duplicate/malformed markers, and unexpected source evaluations.

This proves only the chosen HIL source site; full production Logging integration and product resource budgets remain outstanding. See [G11 WROOM measurements](docs/WROOM-RESOURCE-QUALIFICATION.MD).

## 2026-10-19 — G10 one-sink *real ESP32-WROOM* integration

The real ESP-IDF WROOM HIL now exercises two genuinely static G10
single-sink configurations **in addition to** its existing two-sink fanout:
an EDP-Memory-owned CBOR buffer accepted by EDP-Serial's bounded
copy-on-enqueue FIFO then independently transmitted and EDLG CRC-checked
after the source representation lease has been returned; and a second
EDP-Memory-owned CBOR buffer accepted by one archive-style synchronous
copying sink with no dummy Serial consumer. Each case checks exactly one
encode and one sink call, accepted count 1, empty owner lease, independent
payload ownership and complete pool teardown. The existing native
two-sink ordering, rejection, CRC and 18 canonical markers remain intact.

RPI400 real integrated ESP32-D0WD (WROOM) 2026-10-19:
- Actual PioArduino/ESP-IDF build **20261019T102408-3ed33bde PASS exit0**,
  source Git blob 41cab54885badfafc3f5d1aa611c58d473e7e630,
  entire factory image SHA256
  65e28a05e853f8266390be98948563928fb0aa8096948849efc21d50a40ed146.
- Physical write from 0x0 and **separate on-chip digest verification**
  **20261019T103019-e7ce3060 PASS exit0**.
- Three independent live reset + intentional NVS restart UART sessions
  **20261019T103056-344072ea PASS exit0**, finish 10:31:06 CEST.
  Every run 18/18 native functional markers, EDLG CRC, full-test
  resource checks and both G11 marker observations PASS; the canonical
  MEMORY_OWNED_ENCODING_SERIAL_COPY gate now also proves both single sinks.
- The six retained capture files/hashes live privately in
  /srv/espressio/scratch/gpt6-logging-20261019-g11/g10-one-sink-captures;
  capture-sha256 manifest digest
  5f10a96f545f4ac5ce5f410ef31df0b1a0b0f9a37bb64b8d21cc3cc364fc7562.
- Whole test bundle free8 min 282016 B, dedicated Thread stack remaining
  5036 B and app_main stack remaining 5048–5064 B. These do **not**
  constitute a complete Logging product resource budget.

**Limits:** The archival callback is an independently copied bounded
memory fixture, **not** a durable EDP-Persistence sink. Accepted Serial
FIFO bytes are not proof of end-user physical receipt. This one-sink
change does not complete asynchronous copy/lease/fork-join, governed
15-field CBOR/JSON or power-interruption durability; issue #1 remains open.


## G4 approved Field 11 and 14 codec validation (2026-10-19)

The G4-approved Clock projection/quality pair (Field 11) and zero-only
validity fact byte (Field 14) are implemented through reversible opt-in
EDP-Serialisation/EDP-BoundedTypes adapters; [wire documentation](docs/G4-APPROVED-FIELD-WIRE.MD).
Native GCC and Clang exhaustive child-field CBOR/JSON tests pass. Real
ESP32-WROOM firmware independently flashed/verified and passed three
native G4 CBOR/JSON source checks within the standard 18/18 HIL.
These are **child-field** proofs, NOT a complete 15-field LogRecord
Typed Envelope or durability certification. No new TypeIdentifier,
parent FieldId, package version or mandatory dependency was introduced.


## G4 historical Clock quality and reserved-facts inspection

The host forensic inspector rejects malformed/unknown Field11 Clock
statuses and any nonzero Field14, following approved v1 child wire
contracts. Cross-device time comparison requires explicit Correlated
and Reliable evidence at both ends, not merely synchronized timestamps.
The CLI reports numeric owner statuses without claiming independent
producer authentication. Full root Typed Envelope and complete
schema-driven argument decoding remain open.


## G5 — schema-bound host JSON positional decoding (2026-10-19)

Optional host decoder validates EDP-Serialisation JSON positional values
against exact generated EDP-Localisation historical parameter kinds,
with strict canonical Base64 for bytes/DeviceIdentifier, an eight-byte
TypeIdentifier array, finite typed numeric widths and no hidden
underflow/overflow. JSON lexical floating values are parsed exactly
until bounded binary64/binary32 checks complete; no tiny nonzero token
becomes a fabricated zero. 

RPI400 full host Python suite: `20261019T124110-449cdd49`
PASS exit0, 34 tests. [Host Decoder](docs/HOST-DECODER.MD).
No complete 15-field record wire encoder, independent issuer
authentication or physical PowerLoss claim follows from these tests.


## G1 real ESP32 native finite full-queue wait (2026-10-19)

Fresh ESP32-D0WD firmware qualified a 40 ms monotonic-clock
AcquireRecordUntil deadline under completely occupied native queue:
measured 31.810 / 31.808 / 31.678 ms across three independent
physical runs; DeadlineElapsed, no Memory lease borrowed,
no active reservation or live records and released slots required.
Full build, actual flash+verify, all earlier 18/18 HIL checks PASS.
See [HIL qualification](tests/hil/g1-espidf/VALIDATION.MD).
GCC+Clang G1 Memory/Threading owner admission regressions also PASS.
Still partial G1 — no declaration of complete native contention,
shutdown or product-wide finite-time coverage.


## G2 Clock owner-status source propagation — host and WROOM (2026-10-19)

The optional coherent Clock capture now carries the **existing**
EDP-Clock correlation projection and temporal quality assessment
statuses directly through LogTemporalEvidence into the approved
LogRecord Field 11 source value. Unknown statuses and contradictory
correlated-without-coordinate evidence fail closed. Old Clock source
initializers conservatively default to unavailable/unreliable.
GCC+Clang real generated Localisation integration PASS and fresh
corrected ESP32 WROOM flash + 3/3 native HIL cycles PASS (18/18).
[Evidence and bounds](docs/COHERENT-TEMPORAL-EVIDENCE.MD).
A preceding physical image panicked due to the new test reading an
unbound Clock before bootstrap; decoded, corrected and physically
requalified. Not a claim of live multi-node Clock coherence or a
completed persistent 15-field record wire.


## G1 native cross-core mutex deadline — physically qualified

Alongside the real ESP32 full-queue finite wait, a separate actual
Core1 mutex-owner task now holds the **queue's same native mutex**
against a Core0 producer for a 40 ms Clock deadline.
Final three physical WROOM runs PASS 40.035 ms each, no Memory
borrow, no active output, zero live Record and verified owner
release. Intermediate failed diagnostic images traced to the
test's zero-tick `pdMS_TO_TICKS(2)` loop; corrected and
independently reflashed. Evidence
[HIL validation](tests/hil/g1-espidf/VALIDATION.MD).
Remaining shutdown, late Memory, concurrency and power-loss
bounds require further independent tests.


## G1 native exhausted Memory pool deadline (2026-10-19)

A real ESP32 WROOM test now fully occupies both EDP-Memory
dedicated record slots while the queue remains available.
The third producer's one 40ms Clock deadline times out in
36.863ms (3/3 physical cycles), cleanly aborts its Queue
reservation, retains no Memory lease, and confirms both
existing leases are released with capacity notifications.
Prior full-queue and cross-core mutex deadline cases and
all 18/18 full HIL markers remain PASS.
[Validation](tests/hil/g1-espidf/VALIDATION.MD).


## G1 real exhausted-pool shutdown (2026-10-19)

The real ESP32 HIL now confirms a Core1 producer is blocked
waiting on full EDP-Memory while Core0 triggers the
original bounded AdmissionLifecycleCoordinator::ShutdownUntil.
All 3 independent WROOM runs PASS, lifecycle Quiescent,
producer joined with ShutdownRequested, zero active
admissions/leaks and all 18/18 earlier HIL markers retained;
292–293µs observed controlled shutdown coordination.
[Validation](tests/hil/g1-espidf/VALIDATION.MD).


## G1 native Core0/Core1 Memory grant boundary (2026-10-19)

Actual ESP32-D0WD/WROOM two-core producer/capacity-release tests
now prove both grant-before-deadline and capacity-released-only-after
timeout with real EDP-Memory, EDP-Threading and rollback.
Fresh build, independent chip write/verify and 3/3 UART/NVS
restart sessions passed all original 18/18 HIL gates and verified
no live Record or active admission remains after either case.
[Evidence and limitations](tests/hil/g1-espidf/VALIDATION.MD).
Exact near-deadline late-provider-grant interleaving and final G1
release remain outstanding.

## G1 native late Memory grant recovery (2026-10-20)

The physical ESP32 HIL now injects a deliberate delay *after*
successful real EDP-Memory NoWait acquisition but before its result
returns, proving Logging's `GrantedAfterDeadline` error preserves
and returns the genuine lease through coordinated release.
The 40ms deadline was crossed; all three real WROOM runs measured
89.032ms, reclaimed all source/queue ownership, and retained 18/18
previous HIL markers plus framing CRC. Actual flash/digest verified.
This is controlled race-injection, not a production Memory delay
or proof of all G1/G14/G15 limits.

## G1 full transaction deadline after Queue attachment (2026-10-20)

The canonical producer deadline is now checked **after** the final
Queue::AttachLease, not just before it. Late attachment aborts its
already-owned EDP-Memory lease through the original queue lifecycle
and returns DeadlineElapsed rather than reporting a late record as
Admitted. A synthetic post-acquire delay on an **actual native mutex**
with an actual Memory-owned lease passed GCC+Clang tests and real
ESP32 write/verify plus three HIL boots (18/18 each), 89.623ms
measured under injected delay. See
[Bounded Admission](docs/BOUNDED-RECORD-ADMISSION.MD).


## 2026-10-20 08:38–08:42 CEST — native post-attachment cancellation

After the G1 post-AttachLease canonical deadline fix, a second test
exercises **cancellation becoming visible during** the final native
Queue::AttachLease. The HIL-only provider delegates to a *real*
EDP-Threading mutex and raises a test stop predicate on its second
successful timed Acquire, then introduces an 80ms + FreeRTOS tick
post-acquire delay. The actual EDP-Memory lease transfers to the
Queue and MUST be released by the normal Reservation abort path.
The expected result is `ShutdownRequested` (2), rather than
`Admitted`, even while the queue attachment returns Succeeded.
This is an injected stop predicate, not a claim that real lifecycle
ShutdownUntil or all inter-core stop races were exhausted.

Host real owner-composed GCC and Clang admission tests detached
`20261020T083848-50c62356` **PASS exit0**. Actual complete
PioArduino/ESP-IDF WROOM firmware build detached
`20261020T083937-a40a64b6` **PASS exit0**, factory SHA256
`360ef7ad8b732b32510e2f089318950a7a6df4e412b5b265a18da0cb0c7742c3`.
Real write at 0x0 followed by separately verified on-chip flash
digest `20261020T084050-323a7b0b` **PASS exit0**.
Three independent host-triggered native reset/NVS-restart UART cycles
`20261020T084135-8228e918` **PASS exit0**, each 18/18
original markers, full EDLG CRC/resource/G11 instrumentation.
Every cycle emitted `G1_CANCEL_DURING_ATTACH|locks=2|status=2|
memory=0|queue=0|live=0|elapsed_us=89563–89564|valid=1`;
no owner lease or admission was retained. Private capture files
at `/srv/espressio/scratch/gpt6-logging-20261019-g11/g1-final-attach-cancel-captures-20261020`,
checksum-manifest SHA256
`1bd6c5bde1d9de5a12e9d7aa72fe8296aa786d120982292d3e626fd183396f24`.
No arbitrary full-product timing, natural race probability,
unexpected external power cut or complete Logging release implied.



## G4 core field canonical adapters (2026-10-20)

The optional owner-composed
[canonical core-field adapters](docs/G4-CORE-FIELD-ADAPTERS.MD)
use EDP-Serialisation and EDP-BoundedTypes for approved scalar
LogRecord values: generated message identity, Severity, sequence
tickets in 16/32/64-bit profiles, owner Clock timestamp/state/Era
and uncertainty. Invalid reserved message IDs and zero attempt tickets
are rejected on both encode and decode. GCC+Clang strict CBOR/JSON
round-trip/negative tests PASS (RPI400 detached
`20261020T093523-3c930b16`); CI now exercises this contract.
It does **not** allocate Field13 wire tags, finalize the complete
15-field Typed Envelope, or change package version 0.1.0.


## G4 complete generated LogRecord Typed Envelope (2026-10-22)

Optional `GeneratedLogRecordWireProjection` reconstructs the
owner-governed, generated-schema positional Field13 as a finite tuple
while preserving LogRecord TypeIdentifier and fifteen exact System
FieldIds. Actual EDP-Serialisation CBOR/JSON Typed Envelope roundtrips
pass GCC, Clang and three newly verified physical ESP32-WROOM runs.
The real HIL required 16 KiB test-main stack; measured peak task
usage 10,824 B after an earlier 8 KiB task panic. No universal
production stack requirement or complete Logging release is claimed.
See [G4 contract and measured limits](docs/G4-GENERATED-WHOLE-RECORD.MD).



## Complete G4 root WROOM HIL — 14 KiB requalification (2026-10-22)

The **test application** now qualifies the full schema-generated
15-field CBOR/JSON Typed Envelope using a 14,336-byte app_main stack,
down from the 16,384-byte diagnostic fixture. Three physical ESP32
runs passed 18/18 functional markers; app_main retained 3,512 bytes
minimum. Cumulative high-water phase probes show the major increment
at the EDP-Serialisation JSON encoding path. This is not a new
production stack default; remaining performance and memory budgets
are open. [Complete evidence](docs/G4-GENERATED-WHOLE-RECORD.MD).


## 2026-10-22 — G4 complete record after EDP-Serialisation owner JSON bound

The previous 14 KiB test showed 3,512 bytes of cumulative minimum
app_main stack remaining after JSON. A separate owner fix
([EDP-Serialisation #10](https://github.com/ESPressio-Development-Platform/EDP-Serialisation/issues/10),
commit `08fecdcc74f72c7cc73dc777eb843db727c9827d`) bounds
compile-time canonical JSON field traversal by the maximum actually
declared EDP-System FieldIdentifier. The same complete 15-field
generated LogRecord CBOR/JSON test, with the same **14,336-byte**
app_main stack, was rebuilt and physically requalified without changing
Logging root field identity, codec, or Memory ownership.

- New full factory SHA256: `6ab5745e6ffc587c1c5ca6480e27a11447a6ac0bccc72e6baa6c483f3c3ba572`.
- ESP-IDF build `20261022T120813-3c70bd12`: **PASS exit 0**.
- On RPI400, the recovered build image hash was independently checked
  before real ESP32-D0WD flash/write and separate on-chip `verify-flash`.
  Detached physical job `20261022T122616-a4d5f0a3`: **PASS exit 0**,
  three distinct UART reset/NVS-restart runs, each 18/18 functional
  markers, real full G4 CBOR/JSON encode/decode, framing CRC and G11
  resource markers.
- Cumulative FreeRTOS stack remaining after source projection:
  **11,208 B** (unchanged); after CBOR encode/decode:
  **10,536 B** (unchanged); after JSON encode/decode:
  **10,472 B** (previously **3,512 B**).
- Linked whole HIL firmware flash use: **378,789 B** (previously
  **388,513 B**); statically reported RAM: **21,480 B**, unchanged.
- Private capture manifest SHA256:
  `cb39d128a142951efdb8a25954b2acac95c408dab20a2c10ed7f82aac396727b`.
  Raw evidence remains only under RPI400 path
  `/srv/espressio/scratch/gpt6-logging-20261022-0919-v4/g4-whole-record-physical-14k-jsonbounded-recovery-20261022`.

The full EDP-Serialisation owner GCC+Clang suite, with restored negative
enum-certification tests and sparse/255 boundary static assertions,
also passed `20261022T123157-67f0fdca` exit 0.
These comparisons describe **whole HIL application** high-water
measurements on one compiler/board; they do not prove a universal stack
budget, actual power interruption durability, or completion of the
remaining G1–G15 release requirements. The earlier 8 KiB physical
failure remains a genuine historical failure, not rewritten as a pass.

## 2026-10-22 — G4→G10 fully typed fifteen-field record, two independent copying sinks on real ESP32

Fresh dedicated ESP32-D0WD WROOM firmware contains `g4_root_owned_fanout_check.hpp` called from the native HIL. A generated EDP-Localisation schema supplies `RuntimeIncarnationId` and `ClockEra` Field 13 arguments, captured in owned source storage and projected into the complete, permanent fifteen-field typed envelope. The test encodes that envelope twice—once CBOR and once JSON—using **EDP-Serialisation**, each exactly once into one leased, bounded EDP-Memory representation. Two synchronous sinks independently copy the bytes before the source lease is released: an EDP-Serial bounded FIFO and an archive-*style in-memory copying test sink*. The FIFO subsequently pumps an EDLG frame, verifies frame CRC and confirms byte-for-byte equality with the archive copy; the typed CBOR/JSON payload is decoded from the transmitted frame and compared with the source identity, argument values, attempt, and schema fingerprint. Both encodings reclaim the memory lease and tear down the owner runtime successfully.

This is not a durable EDP-Persistence archive, async reference-counted lifetime, proof of physical UART receipt beyond the test endpoint, or unexpected PowerLoss validation. The coordinator/product G1–G15 completion gates remain open.

RPI400 authoritative evidence from 22 October 2026:

- Entire PioArduino/ESP-IDF WROOM image build **20261022T124919-346c630c PASS exit0**, source main blob `0090c59e73fe40dff0c7f6cc6c1ca8dd92367f1a`, new test header blob `996eb9f7cda61a1f52099d0b7c8cd2621eb284ed`, factory image SHA256 `21f423be8a9fac6e70ea0dc0d837367581e4f7d5beadd2dab03292f0f667beb4`.
- Physical write at address 0x0 and separate independent flash verification, followed by **three** actual reset + NVS-restart UART test captures in detached run **20261022T125506-8e81073b PASS exit0**, completed **12:55:43 CEST**.
- Each run achieved 18/18 native functional markers, EDLG CRC, resource and G11 instrumentation checks, complete CBOR/JSON record encode/decode and both independent copying-sink paths. Per-phase WROOM stack minimum margin was **10,472 bytes** over these three executions. This is measured test-task margin only, not Logging product worst-case budget.
- Raw captures and flash/verify logs remain on RPI400 in `/srv/espressio/scratch/gpt6-logging-20261022-0919-v4/g4-g10-owned-full-root-physical-20261022`, manifest SHA256 `a239f8fae1fd0d0e3ed3844059db5b7c134f289a2ff87af7b37afc454cd3c17c`. Do not commit private captures, firmware binary or log dumps.

The physical main.cpp was compared line-for-line against the exact remote branch blob `31bc06e1aed37f9381396752c47de25ab74c7be4`; it differs **only** by the new test header import and single call to the new test. No other HIL source code was replaced.


## 2026-10-22 — optional G10 copied asynchronous Task-owner adapter (#11, partial)

`logging/AsyncCopyingTaskSink.hpp` composes the **existing** EDP-Memory
ObjectPoolLease, ByteOperations and EDP-Threading Task dispatch/completion
contracts to allow a source-independent *one-in-flight-per-sink* copy into
bounded Memory before accepting asynchronous Task admission. This is a
separately-owned, opt-in callback for the existing G10 1–8 sink fanout.
Final sink outcomes (including cancel/provider failure) remain distinct
from admission, and Memory release is deferred until the Task is terminal.
No Mesh dependency, additional thread/queue/allocator or version mutation.
Only a deterministic synthetic-deferred Task owner and native Memory pool
have been host validated so far, **not** an actual worker execution on
ESP32 or persistence durability. See [full contract and remaining tests](
./docs/ASYNC-COPYING-TASK-SINK.MD) and [developer Wiki](
./wiki/Reference-logging-AsyncCopyingTaskSink.md).

## 2026-10-23 native G10 Task worker (#11 partial)

The optional independent-copying Task adapter was exercised through a
**real ESP32 ESP-IDF EDP-Threading DedicatedWorkerLease** with EDP-Memory.
Three physical RPI400 reset runs passed 19/19 HIL markers each after a
failed initial run exposed that `TaskDispatchPolicy::AbandonImmediately`
rejects when the Worker is occupied; it does **not** queue behind a
running Task. The actual device test verifies no-wait rejection/reclaim,
running cancel-request ownership, payload survival after source mutation,
and eventual independent final sink rejection. A canonical owner-generated
G4 fixture is required before HIL build. Test evidence, exact source
and limits: `tests/hil/g1-espidf/VALIDATION.MD`. The full #11 async
stress/queued-cancel/persistence-release gates and parent Logging #1
remain open, version stays 0.1.0.

## Eight-sink repeated G10 lifecycle stress (2026-10-23)

G10's maximum 8-sink async composition was exercised through 96
repeated native EDP-Memory allocations/reclaims with eight independent
simulated EDP-Threading public Task completions, including mixed
rejections, cancellations, task backpressure and source mutation.
RPI400 GCC/Clang/Clang UBSan pass with native Memory teardown.
This is host fixture evidence, **not** an eight-worker physical
execution. ASan cannot initialize on this RPI400, even for a trivial
smoke binary; its coverage remains outstanding rather than passed.
See `docs/ASYNC-COPYING-TASK-SINK.MD`.

## 2026-10-23 G3/G5 generated-replay coverage

The generated diagnostic-contract runner now executes the restored
`OwnedDiagnosticReplayTests.cpp` source-compatible facade fixture
and the existing mixed DeviceIdentifier/float source-rehydration
fixture. This repairs a real missing-file test failure and a previous
compile-only coverage gap without changing the core reencoding ABI.
See `docs/OWNED-DIAGNOSTIC-POSITIONAL-REENCODING.MD`.


## Finite oldest-eligible EDLS rolling generations (2026-10-23)

Opt-in `logging/EdlsArchiveRotation.hpp` implements normal-provider
readback-qualified bounded **oldest-clean** replacement when all EDLS slots
are occupied. It composes existing EDP-Persistence/FileStorage, EDLS
checksum inspection and EDLG writer, protects the newest certified
generation and fails closed on ambiguity, torn tails, generation conflicts
and provider/refreshed-readback uncertainty. Does not claim power-loss
atomicity or committed durability. Existing empty-slot-only successor remains
available unchanged. See [rotation contract](docs/EDLS-ARCHIVE-ROTATION.MD)
and [Wiki reference](wiki/Reference-logging-EdlsArchiveRotation.md).


The **20/20 live SPIFFS rolling** checks passed three times on the
RPI400 WROOM-32 (detached `20261023T083422-cc50dcba`, PASS exit0).
This qualifies bounded oldest-clean selection, readback and damaged-tail
refusal on real VFS/flash in controlled operation only, not external
power-interruption durability.

## 2026-10-23 — genuine relay-interrupted FileStorage test (G7 / #12)

The dedicated `tests/hil/g7-power-cut-espidf` fixture is qualified for the
RPI400-integrated WROOM, using the GPIO17 switched-supply relay introduced
in Policy V2. The user confirmed this wiring prevents back-powering. The
first real test **PASS** under RPI400 detached receipt
`20261023T110910-d66bc60e`: two independently verified EDLS generations
survived interruption of a separate 96 KiB native FileStorage replacement,
followed by true ESP32 power-on reset and read-only archive recovery.
The full previous 2 MiB device image was backed up before flashing.
The native FileStorage capability **remains** Restart-only /
`InterruptionAtomicity::None`; isolated empirical survival must never be
promoted to a general PowerLoss guarantee. See the test README and
`docs/EDLS-ARCHIVE-ROTATION.MD`; issue #12 tracks the remaining stronger
durability design.

## 2026-10-23 — separate real-power EDLS torn-slot recovery HIL

The additional dedicated `tests/hil/g7-torn-slot-espidf/` fixture
reuses independently verified generation-1 and generation-2 records from
the GPIO17 relay test. It publishes a deliberately incomplete generation-3
oldest-slot file, cuts real ESP32 power after this partial native write,
and on a new physical power-on reboot verifies that the previously committed
generation-2 slot remains CRC-clean while the generation-3 candidate has
a damaged/incomplete frame tail. The real oldest-eligible rollover API
refuses ambiguous archive mutation. Physical run
`20261023T113223-4de652e4` **PASS exit 0**. This is a cut **between**
logical writes, not proof of atomicity inside a write; the SPIFFS
VFS provider remains `InterruptionAtomicity::None`.


## 2026-10-23 — opt-in power-loss-atomic small EDLS snapshot profile

New `logging/PowerLossEdlsSnapshots.hpp` composes the existing
EDP-Persistence KeyValueStorage API, statically demands PowerLoss
retention **and** per-key interruption atomicity, and retains the
latest acknowledged, checksum-valid EDLS generation in bounded
independent key slots. On ESP32, EDP-Persistence-ESP-IDF's NVS provider
provides the advertised atomic key-write capability, with a 512-byte
per-value limit. This is a small-record *latest-generation* retention
profile, **not** an unbounded archival or universal SPIFFS atomicity
claim. See `docs/POWERLOSS-EDLS-SNAPSHOT-PROFILE.MD`; broader G7
journal, NVS physical hard-cut tests and G1–G15 production release
remain separate gates. All component versions stay 0.1.0.


## 2026-10-23 — physical NVS strict snapshot test

The opt-in bounded `PowerLossEdlsSnapshots` adapter was exercised with
the *real* ESP-IDF NVS KeyValueStorage provider on the RPI400 ESP32,
not just the mocked host provider. Genuine GPIO17 power removal during
an NVS commit campaign resulted in hardware power-on reset and
recovery of a CRC-valid generation 29, newer than the host's last
pre-cut observed acknowledged generation 5. Detached receipt
`20261023T122318-b1b4e020` PASS exit0. The test firmware image and
full 2 MiB pretest flash backup are retained on RPI400 only.
See `tests/hil/g7-nvs-powerloss-espidf/README.MD` and
`docs/POWERLOSS-EDLS-SNAPSHOT-PROFILE.MD`. This qualifies the
bounded latest-only NVS profile for the tested cut, **not** universal
provider atomicity of SPIFFS or a historical archive release.


## 2026-10-23 — bounded PowerLoss historical EDLS journal

The optional [PowerLossEdlsJournal](src/logging/PowerLossEdlsJournal.hpp) composes the existing atomic NVS KeyValueStorage-backed EDLS snapshot engine to retain N individual complete records using N+1 independently atomic physical slots (currently maximum 7 retained records). During append, no still-retained record is overwritten. Retired generations are not replayable even if physically present. CRC, cyclic slot ordering and continuity are checked; failed writes into sacrificial space fail closed while protected records remain readable. The complete specification, limits and wear caveats are in [docs/POWERLOSS-EDLS-JOURNAL.MD](docs/POWERLOSS-EDLS-JOURNAL.MD). This is a bounded history reference, not a large-capacity production EDLS archive. All versions remain 0.1.0.

Physical RPI400 ESP32 GPIO17 hard-power-cut journal HIL
20261023T125615-eded7957 PASS: ESP_RST_POWERON, both retained
generations 18 and 19 independently CRC-verified after power
restoration, with bounded two-record history. See normative
docs/POWERLOSS-EDLS-JOURNAL.MD for precise limitations.


## 2026-10-26 — finite immutable EDLS rolling with N protected + 1 physical reserve

The existing [EDLS rotation owner](src/logging/EdlsArchiveRotation.hpp) now exposes a separate opt-in `PublishEdlsProtectedRollingSuccessor` and `FinishEdlsProtectedRollingRetirement`. Application Bootstrap chooses a **finite N-record/segment retained capacity**, N+1 physical file paths and compile-time frame/segment bounds. The new path never replaces a still-retained physical filename: it commits/CRC-verifies an immutable successor in the empty spare, independently checks all previous complete history, and only then retires the oldest eligible record. After a crash at the retirement boundary, N+1 complete contiguous generations may be reconciled and reduced to N; a torn unknown candidate fails closed without destructive inference. **Indefinite history retention is not intended.**

Host GCC, Clang, UBSan and independently reopened real POSIX VfsFileStorage tests have passed on RPI400; see [contract and limitations](docs/EDLS-PROTECTED-ROLLING.MD). The previous best-effort oldest-slot overwrite remains a separate weaker API, **not suitable for strict LittleFS PowerLoss history**. This is not yet the native physical EDLS power-cut result, does not establish a hardware wear/GC reserve, and does not complete canonical typed LogRecord CBOR production integration. Generic FileStorage remains Restart/None/MayModify; EDP versions remain exactly 0.1.0.


## 2026-10-26 — native real-EDLS LittleFS cut evidence fixture (G7)

A new **test-only** [native G7 LittleFS application](tests/hil/g7-littlefs-protected-rolling-espidf/README.MD)
reuses existing Logging immutable N+1 EDLS owner, canonical EDLS and CRC-
bearing EDLG/CBOR frame, and selected EDP-Persistence-ESP-IDF LittleFS
conservative VFS provider. This is the first native target of the *actual*
new EDLS archive owner, as distinguished from earlier synthetic 8192-byte
generation probes. The fixture deliberately power-interrupts AFTER a
verified N+1 complete successor but BEFORE oldest physical retirement;
on reboot it re-verifies complete N+1, retires oldest, then rolls to
exactly two generations. Host raw electrical controller and independent
physical flash decoder are separate gate owners. The fixed tiny CBOR
payload is NOT the full typed LogRecord output; exhaustive GC/wear/
candidate-tear remains open. Actual PASS/FAIL is recorded in the Policy
living handoff, never inferred merely because the fixture exists.

## 2026-10-26: typed LogRecord canonical CBOR into finite EDLS

The optional [typed CBOR EDLS adapter](docs/TYPED-EDLS-CBOR-ARCHIVE.MD) now directly connects the existing generated diagnostic schema-qualified fifteen-field LogRecord projection and **EDP-Serialisation::Cbor TypedEnvelope** writer to the existing checked EDLG frame and finite immutable N+1 EDLS archive owner. It accepts Bootstrap/worker-owned fixed scratch buffers, validates source device/incarnation/fingerprint, rejects oversize CBOR before touching the archive, verifies the entire successor and earlier generations, and only then permits logical oldest-eligible retirement. No alternative codec, platform provider or worker is created. A new owner-generated real VFS test decodes every retained frame as the canonical typed envelope under GCC, Clang and UBSan; physical typed-payload VDD cut/flash reserve/worker saturation are additional outstanding production gates. All versions stay 0.1.0.

A further opt-in [pooled Task encoding and EDLS persistence adapter](docs/TYPED-EDLS-CBOR-ARCHIVE.MD) connects existing EDP-Memory representation and EDP-Threading copying Task admission with schema-verified canonical CBOR decode and immutable persistence in the existing Task worker. Task admission remains distinct from verified EDLS archive completion; no new worker is created.

**High-payload WROOM memory:** after a physically observed 4 KiB frame stack
panic, the existing EDLS inspection, immutable successor, protected retirement,
full generated typed CBOR pipeline and Task delivery paths now allow one
Bootstrap-owned reusable archive scratch buffer, rather than nested large
stack locals. For maximum 4096-byte CBOR payload this archive buffer is exactly
4156 bytes outside the task stack, source EDLG frame separate. See the
[physical failure and correction](docs/EDLS-PROTECTED-ROLLING.MD); the
legacy stack-scratch overloads remain for source compatibility.

### Oct 27 typed EDLS real hardware and Task owner evidence

Native ESP32-WROOM owner-generated 15-field CBOR/EDLS externally controlled VDD interruption **PASS** after a measured 8.254-second outage (run 20261027T114439-028890f5, POR confirmed) with retained g3/g4. The independently extracted raw physical 589824B LittleFS partition audit, including all mandatory fields and approved absent optional field, passed with strict generated schema fingerprint and CRC validation; prior incorrect auditor FAIL remains recorded. Existing EDP-Memory pool + EDP-Threading Task copying sink + real VFS EDLS 16-generation test: GCC/Clang and GCC UBSan PASS. These do **not** yet prove on-target production worker saturation, LittleFS write/GC crash consistency, automated provenance-based torn-candidate handling, or wear/reserve; production-ready status withheld.

## 27 October 2026 — actual unacknowledged candidate write-activity VDD interruption

With RPI400 solely authorised and locked, `esp32dev-midwrite` native firmware (SHA256 of factory image `bd05436b05fd2e0455adbc020c7f069125d5b17e889b22ad6c675e9b1d0c05e6`) created and CRC-verified immutable EDLS g1/g2, committed separate NVS `ack=2`, and started actual ESP-IDF LittleFS VfsFileStorage writes solely to physical **unacknowledged slot2**. The host independently commanded GPIO17 VDD OFF **8.275 seconds**, then ON. Detached run **20261027T133518-b251b2aa PASS exit0**. Actual hardware reported ROM `SDIO_RESET` and app reset=10; therefore the supported status is **EXTERNALLY_VERIFIED_POWER_CUT**, explicitly *not* POR_CONFIRMED. After reboot the candidate was a **zero-length existing physical file** (`InspectEdlsSlot` Empty is not proof path absence), so protected rolling conservatively **refused the unusable candidate** without removing/retiring g1 or g2. Reboot firmware checked the exact two ACKed CRC-complete generations and their NVS watermark.

Second independent physical partition flash extraction (0x170000 + 0x90000) **20261027T134001-1ae1e8e5 PASS exit0**, SHA256 `d27f56ece7d5ee7f1c5ec613eac45c3520bb630018598adfcc82b2a2fc167d42`, shows exactly complete 64B CRC-protected g1/g2 and zero-byte unacknowledged slot2, no unexpected files. Offline parser `audit_midwrite_extracted_edls.py` independently rejects damaged protected frames, wrong identities, unexpected files, and symlinks. **58 host fault/CRC tests PASS**, C++20 foundation PASS, Clang finite rolling PASS, existing real EDP-Memory/Threading Task integration under GCC UBSan PASS. Sources and logs are retained on appliance without publishing private partition images or UART dumps.

**Coverage boundaries:** the accepted electrical cut occurred during an *ongoing candidate write activity window*, but UART/controller latency prevents proving the physical switch opened between instructions of one synchronous `fwrite` / `fsync` call. The 65536B disposable candidate is a test-only GC/write-pressure payload, not a normal EDLS segment. No general FileStorage PowerLoss atomicity, production provenance-authorized automatic torn-candidate disposal, flash endurance, or on-target production scheduling guarantee follows from this test. Logging remains **not production-ready** until separately outstanding gates pass. All versions 0.1.0.

### Strict PowerLoss acknowledgement watermark for finite EDLS

The optional [acknowledgement and recovery contract](docs/EDLS-ACKNOWLEDGED-RECOVERY.MD) uses an existing PowerLoss-atomic KeyValueStorage owner as a monotonic, independently verified durable ACK watermark alongside immutable finite EDLS FileStorage. It rejects legacy unlabeled EDLS as unprovable and repairs a physically ambiguous unacknowledged candidate *only* while the exact last-N acknowledged CRC clean segments remain protected. Native physical qualification is tracked separately; no global FileStorage PowerLoss claim. All versions 0.1.0.

### Real strict NVS-acknowledged EDLS physical qualification (Oct 27)

## 2026-10-27 — real ESP32 PowerLoss NVS + LittleFS with new runtime incarnation

**Two independent, real RPI400 GPIO17 electrical VDD runs qualified.** Both used actual existing ESP-IDF 5.5.5 NVS KeyValueStorage and LittleFS FileStorage; no Logging flash SDK owner. Every run included a verified fresh firmware image, 2 MiB full-chip private backup, separately verified seeded LittleFS partition and a relay OFF hold of at least 8 seconds. A complete acknowledged g1/g2 immutable EDLS window and actual NVS KeyValue `ack=2` were established before the GPIO17 controller cut supply as the *unacknowledged* 65536-byte test-only candidate spare write activity began. Physical cut is verified by the relay state and OFF interval; reset classification is separately checked by ROM and app boot epoch. The 65536-byte spare is test stress, NOT a production EDLS frame, and precise mid-fwrite timing is **not** proven.

- **Initial strict ACK physical run** `20261027T152551-c81f6178` PASS exit0, `POR_CONFIRMED`, OFF **8.269 s**, initial/bootstrap/runtime incarnation 3 retained, fresh boot recovered g1/g2 under committed NVS 2, removed unacknowledged spare and durably committed/rolled g3 and g4. Firmware SHA256 `4b5029295365233dcec2317147cb1612a1ef7c4e03a234bcf577dca471a86966`. Independent raw LittleFS scan `20261027T153046-8e9ae8c5` PASS, partition SHA256 `5a946cd3f26daec8f2b25801387d5a0f70a73bc96b11f9b227a72379326cdf26`, exact two 64B EDLS/EDLG CRC-protected g3/g4 physical slots. Its real NVS state is also in the next full-chip backup; independent Espressif NVS page+entry+blob CRC and EDP-Logging EDLS ledger parse: `OFFLINE_REAL_NVS_WATERMARK_AUDIT=PASS ack=4 last_incarnation=3 ns=edlsack`.
- **Cross-boot/runtime-incarnation physical run** `20261027T153341-f365c3f2` PASS exit0, `POR_CONFIRMED`, OFF **8.260 s**, initial `RuntimeIncarnation=3`, new reboot `RuntimeIncarnation=4`, same canonical DeviceIdentifier. The new strict ACK reconstituted *old* acknowledged g1/g2, committed new g3/g4 under incarnation 4, retaining exactly g3/g4 and NVS ACK=4 bound to the latest writer's incarnation **4**. Firmware SHA256 `36ce5408dded1b0022d64f82fe3f674df81a6d77742f35a5208f26f02a250b0d`. Detached independent dual-physical-media scan `20261027T153856-41a898fa` PASS exit0: actual extracted LittleFS SHA256 `f263586dda681abea0a1994ae6a75828f4476462dee7eb7a8d988b644e12249e`, exact CRC-complete newest g3/g4, and NVS 0x9000/0x6000 SHA256 `219e04159848eadf4ec505bd14e2289fe45114418de5618bb7692ea4e450a2f0`; Espressif's installed independent NVS parser validated page CRC, individual entry CRC and blob value CRC plus existing Logging parser independently validated the 40-byte EDLS watermark: `OFFLINE_REAL_NVS_WATERMARK_AUDIT=PASS ack=4 last_incarnation=4 ns=edlsack2`.

**Actual conclusion supported:** real NVS-based durable ACK=2 survives external VDD cut, torn/zero-byte candidate cleanup can be authorized without prior ACK loss, and recovery can create new g3/g4 under a NEW application runtime incarnation, with independently verified NVS and LittleFS physical evidence. Retained history spanning incarnations is permitted; comparing the archived ledger incarnation to the *current boot* instead of to the **newest ACKed segment** was an identified and repaired design defect. The exact 15-field generated LogRecord source and full task queue durable-ACK semantics, high-stress GC interruption, high-water resource reserve, wear and all G1-G15 release acceptance remain separate OPEN gates. No general LittleFS FileStorage power-loss atomicity claim; all package versions 0.1.0. Full-chip backup, UART, raw flash and NVS images remain private RPI400 scratch.

## Strict NVS-acknowledged full typed LogRecord delivery

The existing `TypedCborProtectedEdlsTaskDelivery` generated 15-field TypedEnvelope adapter now supports an **optional compile-time injected** `PowerLossAcknowledgedEdls` owner. This is not a second CBOR serializer or Task/sink implementation: every code path shares the exact existing EDP-Serialisation canonical typed-envelope decode, re-encoding validation, EDLG frame writer and EDP-Threading Task callback. For the strict owner ONLY, `RecordSinkSubmission::Accepted` and `TypedEdlsTaskCompletion::DurablyAcknowledged` require a successful NVS PowerLoss-atomic acknowledgement commit/readback; the older FileStorage-only path retains its preexisting `Archived` completion and weaker durability semantics. In either mode, admission to `AsyncCopyingTaskSink` is NEVER a durability ACK. `AcknowledgedPendingRetirement` is accepted only after a committed watermark and verified successor; later publishing must successfully resume retirement first.

`tests/GeneratedEdlsStrictAckTaskIntegrationTests.cpp` reuses the actual existing EDP-Memory pool, bounded lease ownership, owner-generated Localisation schema and full 15-field canonical CBOR source, EDP-Threading deferred Task executor, POSIX genuine EDP-Persistence FileStorage and explicit PowerLoss-capability EDP-Persistence KeyValueStorage fake. It validates 17 separately Task-delivered generations, cross-boot incarnation 7->8, exact rolling physical latest two EDLS segments, no ACK on pending admission, terminal ACK only AFTER NVS commit, no false Accepted if KeyValueStorage StoreValue fails, correct failure terminal and retries after safe candidate reclamation. This is genuine C++ owner-composition testing but **not yet** proof of a deployed ESP32 scheduling, flash/worker stack, or wear budget; native HIL separately exercises strict typed NVS and EDLS on the physical chip.

Versions 0.1.0 unchanged; no new allocator, thread, codec, persistence store or SDK imports in production Logging code.

## 28 October 2026 — complete 15-field CBOR + strict NVS acknowledgement hard-cut qualification

RPI400-authenticated exclusive owner `gpt6-logging-production-20261027-1318-r1` recovered the previously stalled private source. The first native `esp32dev-typed-strict` compile on October 27 **FAILED** because the required owner-generated `GeneratedLocalisationContract.hpp` fixture was absent. The new deterministic `generate_typed_fixture.py` and `build_typed_strict.sh` call the **existing EDP-Localisation generator**; generated output remains ignored and never committed. The corrected native ESP32-WROOM / ESP-IDF 5.5.5 build **20261028T084741-7a2bbe25 PASS exit0**, factory image SHA256 `0c00c5a72f53c6fad1fba117da9d330a266ac060b85b065466e47c1fa89d7049`. Host owner-generated canonical 15-field typed CBOR -> existing EDP-Memory pool and EDP-Threading Task -> **PowerLoss-atomic NVS-ACKed** finite EDLS under delayed completion, failed-ACK refusal and cross-incarnation recovery passed GCC UBSan `20261028T084841-8eecfdde` and Clang `20261028T084841-3d67076f`. Strict ACK POSIX VFS GCC UBSan `20261028T084842-82c860e4` PASS; 64 native-controller/offline tests `20261028T084843-b670a7d1` PASS.

The first hardware launch `20261028T085428-2e10428c` **FAIL exit1** at the *safety preflight* because its evidence directory already existed; the runner did not flash/cut the hardware. The corrected genuine GPIO17 physical test `20261028T085517-e0e6b92c` **PASS exit0** included a fresh 2097152B full-chip backup, verified factory + seeded LittleFS, full typed CRC-qualified EDLS g1/g2 with separately NVS-committed ACK2, then a **complete, CRC-verified but UNACKed g3 candidate** deliberately written in the physically vacant slot. Host removed power for **8.226s**, confirmed a new ROM/app boot with **SDIO_RESET** (not POR), and correctly reported **EXTERNALLY_VERIFIED_POWER_CUT**. The real NVS ledger still read ACK2 after reboot; producer-authorised recovery discarded unacknowledged g3 without touching g1/g2; new g3/g4 were then committed and NVS-readback acknowledged, leaving only the latest g3/g4. GPIO17 was powered OFF by the controller at completion.

An **independent physically readback** of LittleFS 589824B at 0x170000 and NVS 24576B at 0x9000, detached run `20261028T085939-c63f1f40 PASS exit0`, verified (without repairing media) exact owner-generated **all 15 permanent LogRecord FieldIds** with the correctly absent optional Field7, canonical CBOR TypeIdentifier/fingerprint, EDLS/EDLG CRC and retained physical g3/g4. Raw LittleFS SHA256 `ccfbfabdabdff5e5adba2f345fdc2324e7ff886de1413524d8e25ffa3646adf9`. Independent Espressif NVS page/entry/blob CRC and EDLS-watermark parser checked namespace `typedack`, ACK4, runtime incarnation7; raw NVS SHA256 `cb8f517789ad5357f2847c794b95c969af2537219c4edd853570e0bc325fc20d`. Private UART and raw images remain **only on RPI400**. Initial resource boot telemetry: 290744B free heap / main-task high water 6688B; reboot 290712B free / 6688B high water. These are test-specific measurements, not worst-case full product budgets.

**Exact scope:** a cross-cut between completely written unacknowledged g3 and the absent NVS ACK3 has been physically qualified. No generic VfsFileStorage PowerLoss atomicity or instruction-perfect cut *inside* a flash write/GC is inferred. The true production scheduled multi-producer queue/worker with latency/backpressure, high-water CPU/stack/heap, flash wear, physical low-space reserve, complete G1–G15 gates and GitHub Actions status remain open. Versions remain 0.1.0; no Mesh changes.

## 28 October 2026 — bounded 256-record real ESP32 strict-ACK load and independent forensic check

RPI400 user-authorised ESP32-WROOM-32 native ESP-IDF 5.5.5 test-only `esp32dev-typed-strict-load` uses the existing 15-field owner-generated EDP-Localisation/EDP-Serialisation TypedEnvelope, EDLG/EDLS immutable N=2 finite archive, actual LittleFS FileStorage and actual **PowerLoss-atomic NVS KeyValueStorage acknowledgement for every record**. The test does **not** use a production asynchronously scheduled Logging worker: one synchronous app_main task serially publishes 256 records, collects 256 real esp_timer publication latencies and samples ESP-IDF heap, task stack and physical LittleFS use at generations 1 and 16..256. The first compile `20261028T091026-748555dc` **FAIL** (test-only std::min operand type mismatch for stack watermark) was corrected by explicit uint32_t conversion, without modifying the Logging library. Native rebuild `20261028T091337-6be65263 PASS exit0`, factory image SHA256 `0b68a70d02ee1620caeee1b1abf549ef31d67ecbfba178a2f59ccd39a0efcab8`; 64 host native-controller/offline integrity tests `20261028T091520-9d400688 PASS`.

**Physical RPI400 run `20261028T091659-28bd6730 PASS exit0`**: verified exact firmware and seeded LittleFS after fresh **2 MiB full-chip backup**, then actually committed 256 canonical typed records through the strict NVS-ACKed existing EDLS owner, retained final generations **255 and 256**, ACK=256. Measured `184292842 us` total, per-publish p50 **767372 us**, p95 **850256 us**, p99 **850282 us**, maximum **865901 us**, approximately **1.39 individually durably acknowledged records/second**. Minimum sampled free heap **289292 bytes**, minimum sampled app-main task stack high-water **3216 bytes remaining**, max observed LittleFS occupancy **8192 bytes** in the dedicated 589824B partition. This is a measured single-task 256-record sample, not an established universal stack/heap budget, full product workload distribution or endurance test. These records are **far too costly to automatically assume unrestricted Logging ingestion**; no performance target was authorised and no pass/fail throughput threshold is invented.

**Independent actual physical flash audit `20261028T092414-13af6a4a PASS exit0`** read LittleFS 0x170000+0x90000, raw SHA256 **`401271eb782047f29d36f822f60fc96f627de4b560ab8bb8a834c222375d110d`**. The existing independent Logging parsers verified actual CRC-valid immutable EDLS/EDLG physical slot2=g255 and slot0=g256, complete permanent fifteen-field owner-generated CBOR TypedEnvelope, unchanged TypeIdentifier, schema fingerprint, device/incarnation and attempt values. Real separately read NVS 0x9000+0x6000, raw SHA256 **`2598d051b58c74dc4dba86187e6fd2a9093bcc85bb67c852b92ccd4e701ecdd4`**, was independently parsed using Espressif's official NVS parser; exact namespace `loadack` ACK **256** incarnation7, page/entry/blob CRC **PASS**. The ESP32 was powered OFF after audit (GPIO17 HIGH) and private images/UART kept on RPI400 only.

**Remaining production gates:** clearly define acceptable sustained log-ingestion and per-record durable-ACK latency/flash write endurance before claiming a production budget. For higher rates consider separately explicit immutable *segment batching* with no per-record premature durable acknowledgement and preserved protected N+1 successor semantics; never silently weaken PowerLoss ACK. This particular test has no concurrent producers, no deployed full EDP-Threading worker/sinks, no crash during real GC at high wear, no exact lifetime wear characterization, and the still-red GitHub Actions jobs have no accessible step output via current connector. Versions remain 0.1.0.

### Measured real ESP32 per-record Publish bottleneck (28 October 2026)

On the ESP32-WROOM-32, 256 records individually acknowledged through LittleFS EDLS and PowerLoss-atomic NVS show that **87.37%** of observed runtime comes from repeated file reads and file-size queries (13,734 reads + 7,143 size calls), versus **1.64%** for NVS StoreValue/ReadValue. All 256 full 15-field records and NVS ACK256 passed independent raw physical flash CRC audits. See [measured 256-record provider latency report](docs/EDLS-PUBLISH-256-RECORD-PROVIDER-LATENCY-2026-10-28.MD). No production algorithm or semantic changes were made. Versions 0.1.0.


### Qualified post-retirement scan reduction

[Physical benchmark and power-cut qualification](./docs/EDLS-POSTRETIRE-SCAN-OPTIMISATION-2026-10-29.MD): redundant immediate post-delete CRC scan removed under exclusive ownership; strict per-record NVS ACK order preserved. Controlled 256-record WROOM benchmark: 1.970 durable records/sec. Overall production acceptance remains incomplete.
