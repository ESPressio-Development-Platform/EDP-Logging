# EDP-Logging

Bounded, structured diagnostic logging for the [ESPressio Development Platform](https://github.com/ESPressio-Development-Platform).

**Version:** `0.1.0` · **Language:** C++20 · **License:** Apache-2.0

EDP-Logging provides deterministic, application-composed diagnostic record capture, bounded producer/consumer delivery, portable record framing and finite durable log archives. The application chooses resource limits, concrete providers, storage placement and retention capacity. The library does not introduce an allocator, task scheduler, filesystem or platform clock of its own.

## Contents

- [Capabilities](#capabilities)
- [Getting started](#getting-started)
- [Application composition](#application-composition)
- [Dependencies](#dependencies)
- [Durability and resource boundaries](#durability-and-resource-boundaries)
- [Documentation](#documentation)
- [Examples and testing](#examples-and-testing)
- [Compatibility and limitations](#compatibility-and-limitations)
- [Changelog](./CHANGELOG.md)

## Capabilities

| Area | Provided functionality |
| --- | --- |
| Diagnostic sites | Six emitted severity levels, compile-time threshold selection and optional evaluation-free disabled instrumentation |
| Record identity | Nonwrapping attempt sequences, typed source identities and a governed 15-field generated record model |
| Source capture | Schema-checked, bounded owned arguments, optional coherent Clock evidence and canonical CBOR/JSON adapters |
| Concurrent delivery | Finite queue admission and lease ownership, producer wake coordination, a dedicated worker and bounded sink fan-out |
| Local framing | EDLG record frames, EDLS immutable segments, strict decoding and inspection |
| Persistence | Application-configured finite logical byte capacity, sealed batch publication, protected rolling generations, strict acknowledgement through a separately qualified KeyValueStorage provider |
| Forensics | Offline decoding, chronology inspection and CSV export; explicit errors for unsupported or uncertain data |

The minimal umbrella header stays lightweight; more extensive facilities are **opt-in**, and require the application to compose their dependencies explicitly. Logging remains usable without Mesh and without a synchronized network clock.

## Getting started

### 1. Add the library

Build with **C++20**. For PlatformIO projects, use the
[PIOArduino ESP32 platform](https://github.com/pioarduino/platform-espressif32)
and add EDP-Logging to the project's libraries. For Arduino IDE, install
the library in the Arduino library search path. For a native ESP-IDF
component, add the library's `src/` headers to its include path.

The Arduino-ESP32 [framing demo manifest](./demos/FramingAndInstrumentation/PlatformIO_Arduino/platformio.ini)
shows the qualified PIOArduino revision and the C++20 flags. Copy the
`platform`, `build_flags` and `build_unflags` settings into your ESP32
environment, then run `pio run -e esp32dev` with the demo's board environment.

The default `library.json` deliberately declares no mandatory EDP library dependencies. Additional dependencies are activated by selecting individual Logging facilities; see [Dependencies](#dependencies) before importing opt-in headers.

### 2. Include the core header

```cpp
#include <ESPressio_Logging.hpp>

using namespace ESPressio::Logging;

static_assert(SeverityEnabled<SeverityThreshold::Info, Severity::Warning>);
static_assert(!SeverityEnabled<SeverityThreshold::Off, Severity::Fatal>);
```

This is a minimal **compile-time threshold example**, not a complete persistent logger. The default threshold is `Off`; an application must explicitly select its logging sites and construct its source/worker/sink composition.

### 3. Start from a complete application

Use the [complete demo index](./demos/README.MD) and choose a maintained, target-specific example rather than guessing provider wiring:

- [Framing and Instrumentation](./demos/FramingAndInstrumentation/README.MD) — provider-free core: compile-time severity gate, attempt identity, bounded EDLG frame, strict validation and corruption rejection (not a complete generated LogRecord).
- [Typed Managed Batch Worker](./demos/TypedManagedBatchWorker/README.MD) — original 15-field records, bounded source queue, generated CBOR, sealed EDLS batching, strict LittleFS/NVS acknowledgement and replay.
- [Configurable Bounded EDLS Batch](./demos/ConfigurableBoundedEdlsBatch/README.MD) — finite batch sizing and dwell configuration.

Each demo provides **Arduino IDE**, **PlatformIO Arduino-ESP32**, and **PlatformIO ESP-IDF** projects. Follow its README for build commands, generated-schema prerequisites and Flash partition provisioning. Demo configurations are examples, **not platform-wide default allocations**.

## Application composition

A full logger is assembled at application Bootstrap, rather than by invoking a global singleton:

1. Select the **generated diagnostic schema**, message/type identities, optional temporal evidence and compile-time record capacities.
2. Configure the **source pool**, transactional queue, deadline/wake primitives and one dedicated Threading coordinator, using the existing EDP-Memory / EDP-Threading / EDP-Clock owners.
3. Choose one or more **typed sinks** and supply their owned, bounded representation buffers.
4. For persistent storage, select actual **FileStorage** and a separately qualified durable **KeyValueStorage** provider, choose an immutable **finite logical byte capacity** before archive initialization, and configure sealed batch size and maximum dwell.
5. Start producers and the worker only after all providers and lifetimes are valid. At shutdown, stop admission, drain or explicitly report outstanding ownership, settle the sink, join tasks and tear down providers in the documented order.

A queue admission or sink acceptance is **not** a durable acknowledgement. A generation is acknowledged only after the required checked publication and durable watermark conditions succeed. Failure or uncertainty is reported explicitly; there is no implicit erase, format, indefinite retry or fabricated recovery.

See [Architecture](./docs/ARCHITECTURE.MD), [Record Production Ownership](./docs/RECORD-PRODUCTION-OWNERSHIP.MD), [Coordinator Sink Service](./docs/COORDINATOR-SINK-SERVICE.MD) and [Strict Acknowledged Recovery](./docs/EDLS-ACKNOWLEDGED-RECOVERY.MD) before integrating a custom sink.

## Dependencies

The **default** `#include <ESPressio_Logging.hpp>` requires the **C++20 standard library only**. The following condensed tree describes opt-in activation; conditional dependencies are *not* imposed merely by installing EDP-Logging:

```text
EDP-Logging [C++20 core; no mandatory EDP library]
├── EDP-Clock [optional: temporal evidence, deadlines or worker coordination]
│   ├── EDP-System
│   └── EDP-Platform
├── EDP-Memory [optional: finite producer/record/representation pools]
│   ├── EDP-BoundedTopology
│   ├── EDP-System
│   └── EDP-Platform
├── EDP-Threading [optional: queues, workers, wake, sink tasks]
│   ├── EDP-Memory
│   ├── EDP-Clock
│   ├── EDP-BoundedTopology
│   ├── EDP-System
│   └── EDP-Platform
├── EDP-BoundedTopology [optional: transactional queue slots]
├── EDP-Serialisation [optional: generated CBOR/JSON and strict UTF-8]
│   ├── EDP-System / EDP-BoundedTypes / EDP-BoundedTopology
│   ├── EDP-Memory / EDP-Localisation
│   └── EDP-Platform-Portable
├── EDP-Localisation [optional: generated diagnostic contracts]
├── EDP-Persistence [optional: FileStorage / KeyValueStorage archive sinks]
│   └── EDP-System
└── EDP-Platform-Portable [optional: owned-argument byte-copy provider]
```

This is a **readable activation overview**; [the complete conditional and transitive dependency graph](./DEPENDENCY_GRAPH.MD) specifies actual header-level requirements and provider ownership. Declare direct dependencies explicitly in the application. The application chooses concrete ESP-IDF/Arduino/host implementations at Bootstrap; **EDP-Serial** and **EDP-Mesh** are not required by Logging. The optional historical-schema **Python** tooling uses EDP-Localisation's Python tools, not an embedded library dependency.

## Durability and resource boundaries

- **Finite retention:** the application supplies the logical archive byte budget, including the required reserved generations and acknowledgement value, before initialization. Oldest **eligible** generations roll out; still-protected records are not retired before a verified, acknowledged successor exists.
- **Power-loss semantics:** an acknowledged EDLS generation uses checked immutable segments and a separately qualified durable NVS/KeyValueStorage watermark. This does **not** make arbitrary filesystem operations or every FileStorage provider interruption-atomic.
- **Memory and scheduling:** all source/queue/worker/representation limits are bounded and configured for the application. Admission deadlines and drain latency depend on the selected providers, workload and hardware.
- **Flash endurance:** greater application-chosen persistent storage capacity generally reduces reuse frequency and wear, but the library makes **no device lifespan guarantee**. Prefer separate storage for high-volume production diagnostics.
- **Clock semantics:** basic local logging does not require synchronized clocks. Applications that require cross-node precision must separately qualify and supply the relevant Clock/Era evidence.

For normative detail use [Immutable Persistence Byte Capacity](./docs/IMMUTABLE-PERSISTENCE-BYTE-CAPACITY.MD), [Protected EDLS Rolling](./docs/EDLS-PROTECTED-ROLLING.MD), [PowerLoss Acknowledged Recovery](./docs/EDLS-ACKNOWLEDGED-RECOVERY.MD) and [Production Retention and Endurance](./docs/PRODUCTION-RETENTION-ENDURANCE-REVIEW.MD).

## Documentation

The documents under `./docs` define the detailed implementation contracts. Use this index to locate the relevant owner and integration rules without treating historical test receipts as API guidance.

### Architecture, record schema and producer contracts

- [Conditional Cross-Repository Dependency Baseline](./docs/DEPENDENCY-OWNER-BASELINE.MD)
- [Architecture](./docs/ARCHITECTURE.MD) · [Log Record Identity](./docs/LOG-RECORD-IDENTITY.MD) · [Log Record Source Assembly](./docs/LOG-RECORD-SOURCE-ASSEMBLY.MD)
- [Record Production Ownership](./docs/RECORD-PRODUCTION-OWNERSHIP.MD) · [G4 Generated Whole Record](./docs/G4-GENERATED-WHOLE-RECORD.MD) · [G4 Core Field Adapters](./docs/G4-CORE-FIELD-ADAPTERS.MD)
- [G4 Approved Field Wire](./docs/G4-APPROVED-FIELD-WIRE.MD) · [G4–G10 Owned Root Fan-out](./docs/G4-G10-OWNED-ROOT-FANOUT.MD)
- [Diagnostic Argument Encoding](./docs/DIAGNOSTIC-ARGUMENT-ENCODING.MD) · [Bounded Diagnostic Arguments](./docs/DIAGNOSTIC-BOUNDED-ARGUMENTS.MD) · [Owned Argument Byte Operations](./docs/OWNED-ARGUMENT-BYTE-OPERATIONS.MD)
- [Diagnostic System Identity](./docs/DIAGNOSTIC-SYSTEM-IDENTITY-ARGUMENTS.MD) · [Diagnostic Clock Era](./docs/DIAGNOSTIC-CLOCK-ERA-ARGUMENTS.MD) · [Coherent Temporal Evidence](./docs/COHERENT-TEMPORAL-EVIDENCE.MD)
- [Owned Positional Re-encoding](./docs/OWNED-DIAGNOSTIC-POSITIONAL-REENCODING.MD) · [Owned Representation Fan-out](./docs/OWNED-REPRESENTATION-FANOUT.MD)

### Admission, concurrency and sink lifecycle

- [Bounded Admission](./docs/BOUNDED-ADMISSION.MD) · [Bounded Record Admission](./docs/BOUNDED-RECORD-ADMISSION.MD) · [Bounded Queue Transitions](./docs/BOUNDED-QUEUE-TRANSITIONS.MD)
- [Admission Wake Coordinator](./docs/ADMISSION-WAKE-COORDINATOR.MD) · [Admission Lifecycle](./docs/ADMISSION-LIFECYCLE.MD) · [Atomic Lifecycle Binding](./docs/ADMISSION-LIFECYCLE-ATOMIC-BINDING.MD)
- [Tracked Admission Leases](./docs/TRACKED-ADMISSION-LEASES.MD) · [Transactional Record Queue](./docs/TRANSACTIONAL-RECORD-QUEUE.MD)
- [Dedicated Coordinator](./docs/DEDICATED-COORDINATOR.MD) · [Coordinator Sink Service](./docs/COORDINATOR-SINK-SERVICE.MD) · [Asynchronous Copying Task Sink](./docs/ASYNC-COPYING-TASK-SINK.MD)

### Frames, archives, retention and persistence

- [EDLG KeyValue Checkpoint](./docs/EDLG-KEYVALUE-CHECKPOINT.MD) · [EDLS Archive Inspection](./docs/EDLS-ARCHIVE-INSPECTION.MD) · [EDLS Archive Append](./docs/EDLS-ARCHIVE-APPEND.MD)
- [EDLS Safe Successor](./docs/EDLS-SAFE-SUCCESSOR.MD) · [EDLS Archive Rotation](./docs/EDLS-ARCHIVE-ROTATION.MD) · [EDLS Protected Rolling](./docs/EDLS-PROTECTED-ROLLING.MD)
- [EDLS Acknowledged Recovery](./docs/EDLS-ACKNOWLEDGED-RECOVERY.MD) · [PowerLoss EDLS Journal](./docs/POWERLOSS-EDLS-JOURNAL.MD) · [PowerLoss Snapshot Profile](./docs/POWERLOSS-EDLS-SNAPSHOT-PROFILE.MD)
- [Bounded EDLS Batching](./docs/BOUNDED-EDLS-BATCHING.MD) · [Typed EDLS CBOR Archive](./docs/TYPED-EDLS-CBOR-ARCHIVE.MD) · [Immutable Persistence Capacity](./docs/IMMUTABLE-PERSISTENCE-BYTE-CAPACITY.MD)
- [Strict ACK Same-Transaction Plan Reuse](./docs/EDLS-STRICT-ACK-SAME-TRANSACTION-PLAN-REUSE-2026-10-28.MD) · [Strict ACK Exclusive-Writer Scan Reuse](./docs/EDLS-STRICT-ACK-EXCLUSIVE-WRITER-SCAN-REUSE-2026-10-28.MD) · [Post-Retirement Scan Optimisation](./docs/EDLS-POSTRETIRE-SCAN-OPTIMISATION-2026-10-29.MD)

### Host tools, diagnostics and archival analysis

- [Optional Serial Output and G12 Provider Integration](./docs/SERIAL-OUTPUT-INTEGRATION.MD)
- [Host Decoder](./docs/HOST-DECODER.MD) · [Forensic CSV Export](./docs/FORENSIC-CSV-EXPORT.MD) · [Historical Diagnostic Schema Bridge](./docs/HISTORICAL-DIAGNOSTIC-SCHEMA-BRIDGE.MD)

### Qualification and evidence

- [G1–G15 Acceptance Matrix](./docs/ACCEPTANCE-G1-G15.MD) · [Logging Qualification Status](./docs/LOGGING-QUALIFICATION-STATUS.MD) · [Long Managed Workload Qualification](./docs/LONG-MANAGED-WORKLOAD-QUALIFICATION.MD)
- [Managed Batch Capacity-Cut Qualification](./docs/MANAGED-BATCH-CAPACITY-CUT-QUALIFICATION.MD) · [Managed Batch Write-Cut Qualification](./docs/MANAGED-BATCH-WRITE-CUT-QUALIFICATION.MD)
- [Sealed Batch Retirement-Cut Qualification](./docs/SEALED-BATCH-RETIREMENT-CUT-QUALIFICATION.MD) · [Sealed Batch Write-Activity Qualification](./docs/SEALED-BATCH-WRITE-ACTIVITY-QUALIFICATION.MD)
- [ESP32-WROOM Resource Qualification](./docs/WROOM-RESOURCE-QUALIFICATION.MD) · [256-Record Provider Latency](./docs/EDLS-PUBLISH-256-RECORD-PROVIDER-LATENCY-2026-10-28.MD) · [Production Retention Endurance Review](./docs/PRODUCTION-RETENTION-ENDURANCE-REVIEW.MD)

See also [Dependency Graph](./DEPENDENCY_GRAPH.MD) and the [internal developer Wiki](./wiki/Home.md) for source-level extension and reference material.

## Examples and testing

Buildable applications are indexed in [demos/README.MD](./demos/README.MD), with all three target source layouts documented in their own READMEs. The provider-free [Framing and Instrumentation](./demos/FramingAndInstrumentation/README.MD) core example has independently passed GCC/Clang execution and both PlatformIO Arduino-ESP32 and ESP-IDF compile/link builds on the approved RPI400; it does not constitute generated-record or persistence qualification. The repository's `tests/` tree contains GCC/Clang host validations, generated-schema and CBOR tests, bounded queue and recovery negatives, and separate physical ESP32 HIL fixtures.

A typical host prerequisite is to provide a **pinned sibling source dependency root** for the opt-in tests:

```sh
export EDP_LOGGING_DEPENDENCY_ROOT=/path/to/pinned/EDP-repositories
CXX=g++ bash tests/run-edls-protected-rolling-tests.sh g++
CXX=clang++ bash tests/run-edls-protected-rolling-tests.sh clang++
```

Physical HIL tests require explicit appliance provisioning, actual connected hardware and their target-specific runners; do not treat a host test pass as physical PowerLoss evidence. The [qualification status](./docs/LOGGING-QUALIFICATION-STATUS.MD) and [acceptance matrix](./docs/ACCEPTANCE-G1-G15.MD) record tested scopes and remaining qualifications.

## Compatibility and limitations

EDP-Logging is at **`0.1.0`** on `main`. Select the matching owner revisions from the [dependency baseline](./docs/DEPENDENCY-OWNER-BASELINE.MD) for the optional APIs you import. The core library is C++20, with integration examples targeting Arduino-ESP32 and ESP-IDF. Optional providers may be unavailable for a particular target and must be qualified independently. The implementation intentionally does **not** promise indefinite log retention, universal 100-records/sec performance, generic power-atomic FileStorage, network clock synchronization, native-memory record dumping or Flash lifespan. Where evidence is conditional or a feature is deferred, the [acceptance matrix](./docs/ACCEPTANCE-G1-G15.MD) is authoritative.

Licensed under [Apache-2.0](./LICENSE).
