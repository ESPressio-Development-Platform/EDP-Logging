# Changelog — EDP-Logging

## Complete consumer walkthrough — unreleased 0.1.0

- Added a complete source-led PIOArduino application for governed LogRecord capture and typed JSON console output.
- Included schema generation, committed boot identity, canonical Clock composition, lazy severity sites, exact capacities and explicit failure handling.
- Expanded the README dependency tree recursively and documented the application-only provider closure and synchronous console contract.
- Runtime, package versions and existing demos remain unchanged.

## Demo dependency portability — unreleased0.1.0

- PlatformIO demos now resolve library dependencies from explicit GitHub URLs.
- Removed symlink and sibling-header dependency bypasses and reconciled consumer build guidance.
- Arduino recipes retain the qualified framework/library bundle; native provider selection stays explicit.

All notable, implemented changes to the ESPressio Logging library are recorded here by **public contract and supported integration profile**, not by agent sessions. Repository versions remain **`0.1.0`** during this development; entries below describe the current **unreleased 0.1.0 development baseline**, not an already tagged or generally certified release.

## Unreleased — development baseline 0.1.0

### Added

- A three-target OwnedAsyncSink example composes one existing Memory payload slot and Dedicated Worker, verifies source-copy independence and final Task/release outcomes, and demonstrates owner teardown.

- Optional Bootstrap-owned claim binding for `DedicatedCoordinator`. After execution quiescence, the application can use the queue claim's existing retirement operation with a queue-mutex deadline to reconcile a failed reclaim without repeating sink delivery.

- Allocation-free **C++20 severity and instrumentation foundation**: six emitted severities, Off/default-off compile-time thresholds, expression-lazy gated logging sites, no argument/Clock evaluation when compiled out, and nonwrapping 16/32/64-bit attempt identity. Full generated-schema producer activation is opt-in, not a default global logger.
- Generated, type-governed **15-field diagnostic LogRecord** integration through the existing EDP-System, EDP-Localisation, EDP-Serialisation, EDP-Clock and EDP-BoundedTypes contracts. Canonical typed positional CBOR and JSON encoders have strict argument width/kind validation, optional coherent temporal/ClockEra evidence and bounded owned source arguments.
- Application-composed finite **source acquisition, transactional record queue, tracked lease ownership, bounded deadline/wake coordination, a dedicated Threading worker and sink fan-out**. No duplicate Memory pool, scheduler, clock or platform synchronizer was introduced. A late acquisition remains an owned lease requiring explicit release; sink acceptance is separate from source retirement and durable completion.
- Portable CRC32C-checked **EDLG frames and EDLS headers**, complete immutable segment inspection, optional multi-frame EDLC sealing, bounded archive publication, strict invalid-tail rejection, guarded physical empty-slot successor and explicit protected N+1 finite rolling lifecycle.
- Application-chosen finite persisted log capacity, configured before initialization; oldest **eligible** entries are retired only after safe successor verification and required durable **KeyValueStorage/NVS acknowledgement**. Durable diagnostics retain explicit failure/uncertainty states for Flash/KeyValueStorage provider faults. A separate NVS key/value history adapter is available for its narrowly qualified atomic-key profile.
- Host tooling for bounded decode, corruption validation, historical schema provenance and forensic CSV output. Archived wording is not fabricated from a schema fingerprint.
- **Four focused demonstration families**, each containing Arduino IDE, PlatformIO Arduino-ESP32 and PlatformIO ESP-IDF source surfaces: `FramingAndInstrumentation` (in-memory only), `TypedManagedBatchWorker` (generated-record/worker/strict ACK) and `ConfigurableBoundedEdlsBatch` (application-selected finite batch sizing and dwell), plus `OwnedAsyncSink` (independent Task payload ownership). Each demo distinguishes source availability, compile success and actual physical execution.
- Maintained V2 normative `docs/`, comprehensive repository-local source/reference `wiki/` and a conditional, transitive library dependency graph.

### Corrected and clarified

- The framing demo explicitly pins Arduino-ESP32 3.3.12 and its ESP-IDF 5.5 library bundle, matching the managed demo recipes and preventing incompatible development-framework resolution.

- On the selected ESP32-WROOM-32 async profile, qualified Threading shutdown during active delivery and native Memory slot recovery after an injected release acquisition failure. The validated path uses immediate worker admission.

- Documented the dedicated coordinator's retained claim after a retirement failure and the selected async Threading dispatch boundary. Immediate worker admission still uses the owner's facility mutex. Native queued cancellation requires a profile that exposes a queued Task handle.

- EDLG writer migration: include `logging/EdlgFrameWriter.hpp` and pass the application-selected ByteOperations provider as the last argument to `WriteEdlgFrame`. EDLS bulk workspace copies and exact-byte readback comparisons in oldest-eligible rotation and sealed successor publication use EDP-Memory. The default umbrella and reader still use the C++20 standard library.

- Severity predicate migration: use IsSeverityEnabled for direct compile-time threshold checks. InvokeLogSite and EDP_LOG_WHEN_ENABLED keep their existing call forms.

- Diagnostic source migration: use `IsDiagnosticSchemaMatching` for all generated argument signatures. For owned diagnostic values, call `EncodeOwnedDiagnosticArgumentsCbor` or `EncodeOwnedDiagnosticArgumentsJson` with an explicit `OwnedPositionalScratch`. Inspect the typed outcome and its `Encoding` member. The scalar-named predicate alias and stack-scratch replay facade were removed.

- Source compatibility: `TransactionalRecordQueue::Borrow(Ticket)` now returns `BorrowResult`. Check `Status()` and use `Lease()` on success. This exposes contention, invalid tickets, incompatible slot phases and integrity failures individually.

- Canonical owner-boundary and dependency pins: generated Localisation and Serialisation contracts, Clock main, Memory NoWait, Threading ordinary-context bounded coordination, Portable narrow byte/resource providers and Persistence abstract FileStorage/KeyValueStorage remain in their respective owning repositories rather than being duplicated inside Logging.
- Optimized bounded immutable EDLS/strict ACK workflows through exact scanned-plan reuse and one necessary post-retirement inspection, without weakening prior CRC, generation, protected history or acknowledgement checks.
- Generated archive publication and typed Task delivery use EDP-Memory byte comparisons for the configured device identity and canonical CBOR readback. Capturing strong System identifiers writes directly from the owner-provided byte view to the argument arena.
- The optional key-value snapshot and journal use the existing EDP-Memory copy and compare contract. Acceptance still requires byte-exact readback. Both profiles passed ESP-IDF WROOM relay recovery tests.
- Protected EDLS predecessor revalidation now preserves distinct internal outcomes for an invalid baseline, provider failure, an unclean readback, a changed header and a changed verified prefix. The public rolling and acknowledgement APIs retain their fail-closed outcomes and retirement order.
- Moved large EDLS scan/publish scratch to caller-owned, application-budgeted storage for constrained ESP32 WROOM tasks. Stack-backed helper overloads remain and require their own measured stack budget.
- Removed transient implementation/agent history from the repository README, which now serves as the public discovery/navigation entry point. Current durable architecture and failure contracts reside in focused `docs/` pages; internal/public/private member reference belongs in `wiki/`.

- The native G1 schema fixture can be generated repeatedly in the same project. Its input workspace is temporary, and generated outputs were verified byte-for-byte against the previous clean generation.

### Validation and qualifications

- Coordinator reports preserve the exact queue retirement result when reclamation is unconfirmed.
- The scoped 252-slot, 20 ms fixture passed the 30,000-record workload at 100 records/s and its independent LittleFS/NVS audit.

- Sealed EDLS publication compares the verified whole-file capture in caller scratch. Hardware measured one fewer native read per successor, and the sealed 64-record relay fixture passed independent LittleFS and NVS verification.

- Single-frame EDLS successor publication compares the exact intended frame before acknowledgement or retirement. The empty-slot path reuses its verified readback in caller scratch.
- The 18 November WROOM strict ACK fixture passed an 8.382-second POR-confirmed power cut and independent LittleFS/NVS media audits. The separate 252-slot 100 records/s retest hit full-queue admission deadlines before completing 30,000 records. See the [qualification ledger](./docs/LOGGING-QUALIFICATION-STATUS.MD).

- Real **GCC/Clang host** test suites cover framed wire, metadata, generated schema, malformed input, bounded admission, queue transitions, producer ownership, sinks, strict ACK, protected rolling and non-destructive read-only scan/failure negatives; relevant UBSan and owner tests are maintained and run against selected exact source revisions.
- Real **RPI400 ESP32-WROOM-32** named native workloads previously sustained a bounded **30,000-record / 300-second / 100 records per second** scheduling profile with strict NVS acknowledgement and finite EDLS retention. Named GPIO17 external relay cuts and independent media CRC scans qualified **specific** protected-data durability profiles.
- All nine Logging and all six optional downstream Serial demo target builds passed on the ByteOperations baseline. The five directly required Memory demo families have separate Arduino-ESP32 and native ESP-IDF compile/link receipts. A build-only result is **not** physical runtime proof; physical and POR-classified subsets are distinguished in the [G1–G15 acceptance matrix](./docs/ACCEPTANCE-G1-G15.MD) and [qualification status](./docs/LOGGING-QUALIFICATION-STATUS.MD).

- The optional Serial and ESP-IDF companions are available from their respective `main` branches. The Logging integration fixtures select the Platform SpinLock required by the current FIFO contract. The native WROOM fixture independently verified receipt of a CRC-valid EDLG frame on UART0.

### Deliberate non-guarantees and deferred functionality

- **No indefinite retention, Flash lifespan guarantee, universal throughput guarantee or generic LittleFS/VFS FileStorage interruption atomicity.** Storage placement, bootstrapped finite capacity and external-Flash preference for high-volume production use remain application decisions.
- **Direct EDP-Memory finite/Forever blocked ObjectPool waiter provider-failure safety** remains tracked in [Memory #16](https://github.com/ESPressio-Development-Platform/EDP-Memory/issues/16). Logging uses the separately tested Memory NoWait path with Threading/Clock-owned bounded waits; this does not certify the unrelated native Memory path.
- Optional **Serial frame pumping and ESP-IDF UART concretization** have separately versioned source branches in their owning EDP-Serial repositories. They are not dependencies of Logging's default or strict-ACK local profile. EDP-Serial-Arduino remains a deferred independent provider feature.
- Network clock synchronization, arbitrary external historical text reconstruction, every MCU/profile or ISR logging sink, automatic filesystem repair and complete end-to-end application shutdown deadlines are not asserted.

See the [current README](./README.MD), [dependency graph](./DEPENDENCY_GRAPH.MD) and [V2 qualification matrix](./docs/ACCEPTANCE-G1-G15.MD) for current implementation and limits. The 13 November ByteOperations baseline `5d854d7` passed 68 GCC/Clang host runners, 40 Python tests and the paired Serial consumer checks. The physical WROOM UART fixture passed 20/20 with independent frame CRC verification. The strict LittleFS/NVS WROOM fixture recovered after an 8.283-second power cut at the acknowledgement gap. Independent media checks verified generations 3 and 4 and NVS ACK4. See the qualification ledger for exact source, demo and hardware receipts. Re-review this changelog **before any future release tag**.
