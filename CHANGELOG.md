# Changelog — EDP-Logging

All notable, implemented changes to the ESPressio Logging library are recorded here by **public contract and supported integration profile**, not by agent sessions. Repository versions remain **`0.1.0`** during this development; entries below describe the current **unreleased 0.1.0 development baseline**, not an already tagged or generally certified release.

## Unreleased — development baseline 0.1.0

### Added

- Allocation-free **C++20 severity and instrumentation foundation**: six emitted severities, Off/default-off compile-time thresholds, expression-lazy gated logging sites, no argument/Clock evaluation when compiled out, and nonwrapping 16/32/64-bit attempt identity. Full generated-schema producer activation is opt-in, not a default global logger.
- Generated, type-governed **15-field diagnostic LogRecord** integration through the existing EDP-System, EDP-Localisation, EDP-Serialisation, EDP-Clock and EDP-BoundedTypes contracts. Canonical typed positional CBOR and JSON encoders have strict argument width/kind validation, optional coherent temporal/ClockEra evidence and bounded owned source arguments.
- Application-composed finite **source acquisition, transactional record queue, tracked lease ownership, bounded deadline/wake coordination, a dedicated Threading worker and sink fan-out**. No duplicate Memory pool, scheduler, clock or platform synchronizer was introduced. A late acquisition remains an owned lease requiring explicit release; sink acceptance is separate from source retirement and durable completion.
- Portable CRC32C-checked **EDLG frames and EDLS headers**, complete immutable segment inspection, optional multi-frame EDLC sealing, bounded archive publication, strict invalid-tail rejection, guarded physical empty-slot successor and explicit protected N+1 finite rolling lifecycle.
- Application-chosen finite persisted log capacity, configured before initialization; oldest **eligible** entries are retired only after safe successor verification and required durable **KeyValueStorage/NVS acknowledgement**. Durable diagnostics retain explicit failure/uncertainty states for Flash/KeyValueStorage provider faults. A separate NVS key/value history adapter is available for its narrowly qualified atomic-key profile.
- Host tooling for bounded decode, corruption validation, historical schema provenance and forensic CSV output. Archived wording is not fabricated from a schema fingerprint.
- **Three focused demonstration families**, each containing Arduino IDE, PlatformIO Arduino-ESP32 and PlatformIO ESP-IDF source surfaces: `FramingAndInstrumentation` (provider-free, in-memory only), `TypedManagedBatchWorker` (generated-record/worker/strict ACK) and `ConfigurableBoundedEdlsBatch` (application-selected finite batch sizing and dwell). Each demo distinguishes source availability, compile success and actual physical execution.
- Maintained V2 normative `docs/`, comprehensive repository-local source/reference `wiki/` and a conditional, transitive library dependency graph.

### Corrected and clarified

- Canonical owner-boundary and dependency pins: generated Localisation and Serialisation contracts, Clock main, Memory NoWait, Threading ordinary-context bounded coordination, Portable narrow byte/resource providers and Persistence abstract FileStorage/KeyValueStorage remain in their respective owning repositories rather than being duplicated inside Logging.
- Optimized bounded immutable EDLS/strict ACK workflows through exact scanned-plan reuse and one necessary post-retirement inspection, without weakening prior CRC, generation, protected history or acknowledgement checks.
- Moved large EDLS scan/publish scratch to caller-owned, application-budgeted storage for constrained ESP32 WROOM tasks. Stack-backed helper overloads remain and require their own measured stack budget.
- Removed transient implementation/agent history from the repository README, which now serves as the public discovery/navigation entry point. Current durable architecture and failure contracts reside in focused `docs/` pages; internal/public/private member reference belongs in `wiki/`.

### Validation and qualifications

- Real **GCC/Clang host** test suites cover framed wire, metadata, generated schema, malformed input, bounded admission, queue transitions, producer ownership, sinks, strict ACK, protected rolling and non-destructive read-only scan/failure negatives; relevant UBSan and owner tests are maintained and run against selected exact source revisions.
- Real **RPI400 ESP32-WROOM-32** named native workloads previously sustained a bounded **30,000-record / 300-second / 100 records per second** scheduling profile with strict NVS acknowledgement and finite EDLS retention. Named GPIO17 external relay cuts and independent media CRC scans qualified **specific** protected-data durability profiles.
- The new minimal three-target framing demo and all five directly required Memory demo families have real Arduino-ESP32 and native ESP-IDF compile/link receipts. A build-only result is **not** physical runtime proof; physical and POR-classified subsets are distinguished in the [G1–G15 acceptance matrix](./docs/ACCEPTANCE-G1-G15.MD) and [qualification status](./docs/LOGGING-QUALIFICATION-STATUS.MD).

### Deliberate non-guarantees and deferred functionality

- **No indefinite retention, Flash lifespan guarantee, universal throughput guarantee or generic LittleFS/VFS FileStorage interruption atomicity.** Storage placement, bootstrapped finite capacity and external-Flash preference for high-volume production use remain application decisions.
- **Direct EDP-Memory finite/Forever blocked ObjectPool waiter provider-failure safety** remains tracked in [Memory #16](https://github.com/ESPressio-Development-Platform/EDP-Memory/issues/16). Logging uses the separately tested Memory NoWait path with Threading/Clock-owned bounded waits; this does not certify the unrelated native Memory path.
- Optional **Serial frame pumping and ESP-IDF UART concretization** have separately versioned source branches in their owning EDP-Serial repositories. They are not dependencies of Logging's default or strict-ACK local profile. EDP-Serial-Arduino remains a deferred independent provider feature.
- Network clock synchronization, arbitrary external historical text reconstruction, every MCU/profile or ISR logging sink, automatic filesystem repair and complete end-to-end application shutdown deadlines are not asserted.

See the [current README](./README.MD), [dependency graph](./DEPENDENCY_GRAPH.MD) and [V2 qualification matrix](./docs/ACCEPTANCE-G1-G15.MD) for current implementation and limits. This file must be reconciled once more against the exact final `main` head **before any release tag** is created.
