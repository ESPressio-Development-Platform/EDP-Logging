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
documents the bounded two-to-eight sink call interface, which reuses
one EDP-Memory claim and records each sink outcome without pretending
acceptance equals durability. [EDLS archive inspection](docs/EDLS-ARCHIVE-INSPECTION.MD)
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

## G4 generated strong arguments — System incarnation and Clock Era (October 2026)

Source-owned bounded diagnostic arguments now support System RuntimeIncarnationId and an explicitly opted-in ClockEra, with generated Localisation schema qualification. Their native copied bytes are not wire tags: EDP-Serialisation remains the canonical positional CBOR/JSON owner. GCC and Clang real generated-contract tests exercise zero diagnostic sentinels, input mutation independence, exact byte capacity rejection and golden positional outputs. See [bounded diagnostic arguments](./docs/DIAGNOSTIC-BOUNDED-ARGUMENTS.MD) and [source assembly](./docs/LOG-RECORD-SOURCE-ASSEMBLY.MD). The complete typed LogRecord envelope and release gates remain unfinished.

## G4 source-owned Field 13 canonical positional reconstruction

A new opt-in [owned diagnostic positional re-encoding adapter](./docs/OWNED-DIAGNOSTIC-POSITIONAL-REENCODING.MD) rehydrates the finite, privately native source arena through the **actual generated Localisation message schema** and forwards only the owner-typed values to EDP-Serialisation's canonical CBOR/JSON positional writers. The complete fixed scratch tuple is supplied and cleared by the caller; private `OwnedArgumentKind/Offset/Size` values are *never* serialized. GCC/Clang and UBSan real generated-schema fixtures cover owner-backed strong, bounded UTF-8/binary and mixed numeric/DeviceIdentifier values and negative failure semantics. This is an independent **Field 13 payload** milestone, not the full fifteen-field Typed Envelope or final Persistence/Mesh/PowerLoss qualification.
