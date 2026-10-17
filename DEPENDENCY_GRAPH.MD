# EDP-Logging Dependency Graph

**Branch:** `feature/logging-v1`. **Package:** `0.1.0`. Revalidated 2026-10-16.

## Core/default source graph

The default `<ESPressio_Logging.hpp>` Severity, AttemptSequence, binary EDLG/EDLS framing, and instrumentation foundation currently require only **C++20 standard library headers**. `tools/edp_logging` is a pure Python standard-library host toolchain. The package's default PlatformIO dependency list is intentionally empty; no platform-specific concrete implementation is selected or linked by default.

## Optional G1 admission-bridge activation

**Only when the application explicitly imports** `<logging/BoundedAdmission.hpp>`, it must provide the following direct abstract EDP dependencies as explicit build/library references:

```text
EDP-Logging [BoundedAdmission optional]
├── EDP-Clock [direct]
│   ├── EDP-System
│   └── EDP-Platform
│       └── EDP-System
├── EDP-Memory [direct]
│   ├── EDP-System
│   ├── EDP-Platform
│   │   └── EDP-System
│   └── EDP-BoundedTopology
├── EDP-Threading [direct]
│   ├── EDP-System
│   ├── EDP-Platform
│   ├── EDP-Clock
│   ├── EDP-BoundedTopology
│   └── EDP-Memory
└── EDP-Platform [direct abstract native NoWait policy]
    └── EDP-System
```

These are **abstract domain libraries**, not concrete provider dependencies. Bootstrap must supply appropriate target-platform Mutex/Signal/Clock/MemoryResource implementations via Composition. No dependency on EDP-Platform-Portable, EDP-Platform-ESP-IDF, EDP-Platform-Arduino or other concrete provider is introduced into EDP-Logging.

**Activation:** the source includes canonical `ESPressio_Clock.hpp`, `ESPressio_Memory.hpp`, `ESPressio_Threading.hpp`, and `ESPressio_Platform.hpp` directly. The feature is *not* automatically imported from `ESPressio_Logging.hpp`. Explicit application build configuration must list all four direct EDP dependencies; no implicit reliance on EDP-Threading's transitive includes is permitted.

The optional `BoundedAdmission` source is a partial reusable algorithm with fake-provider GCC/Clang host tests only, **not** a complete producer or qualified G1 deadline implementation.

### Additional optional fixed-slot wake coordinator

`#include <logging/AdmissionWakeCoordinator.hpp>` activates the **same** optional abstract EDP-Clock / EDP-Memory / EDP-Threading / EDP-Platform dependency set above, without new direct or transitive edges. It composes the existing EDP-Threading `BoundedWaitWakeProvider` capability from Bootstrap and uses a fixed compile-time number of uniquely assigned producer slots. The tests reuse genuine EDP-Memory host runtime providers as declared sibling fixtures. No EDP-Memory or EDP-Threading implementation is copied into the runtime source. This feature is not imported by the default Logging umbrella and no other EDP repo consumed Logging in the pinned 2026-10-16 source scan.

### Additional optional G1 admission shutdown and drain

`#include <logging/AdmissionLifecycleCoordinator.hpp>` activates exactly the **same** Clock, Memory, Threading, Platform abstract EDP dependency set as the prior bounded-admission/wake coordinator and adds **only** standard C++20 lock-free atomics. The concrete `TWaitWakeProvider` must be chosen through Bootstrap and supply one independent extra EDP-Threading drain slot (capacity at least producer-count + 1). No new direct/transitive EDP edge, concrete implementation import, Memory finite acquisition or circular dependency exists. The coordinator remains excluded from `ESPressio_Logging.hpp`. No old EDP-* source was modified by this branch.

### Additional optional G1 tracked RAII lease ownership

`#include <logging/TrackedAdmissionCoordinator.hpp>` activates exactly the SAME abstract **EDP-Clock / EDP-Memory / EDP-Threading / EDP-Platform** dependency graph as the opt-in G1 lifecycle. It introduces only standard C++20 lock-free atomic counters and private owned EDP-Memory lease storage, while delegating all Memory capacity release and wake publication to already-composed G1/Threading abstractions. Neither Logging default umbrella nor any foundational dependency acquires a new reverse edge, and there is no EDP-Memory native finite Acquire call. Bootstrap chooses concrete providers; the tracked guard cannot make a raw lease outside its owner safe or cancel unknown external tasks.

### Additional optional G1 transactional record queue (implemented partial)

`#include <logging/TransactionalRecordQueue.hpp>` activates a **direct optional EDP-BoundedTopology dependency**, in addition to the existing EDP-Threading/Clock/Memory/Platform abstractions via the included `AdmissionWakeCoordinator.hpp`. Its state consists of the canonical bounded slot-occupancy bitmap, the canonical non-owning intrusive FIFO endpoints and each queue record's compact Next link, plus one move-only Memory lease, generation token and retirement coordination bit/state. All object allocation, destruction and active lease backing remain in **EDP-Memory**; the caller/Bootstrap selects the Threading ordinary mutex and bounded wait/wake providers. No platform-specific provider, EDP-Serial, EDP-Mesh, or Logging→Clock implementation dependency is introduced. This opt-in code is NOT imported by the core Logging umbrella.

No reverse EDP-BoundedTopology→Logging/Memory dependency exists. Queue ‘Commit’ FIFO is relative to successful commits, not producer arrival or instrumentation ticket order. See `docs/TRANSACTIONAL-RECORD-QUEUE.MD` for unimplemented RAII/end-to-end producer limitations and the exact host + real EDP-Memory tests.

### Additional optional G2 coherent-clock evidence qualifier

`#include <logging/LogTemporalEvidence.hpp>` activates a direct dependency on **EDP-Clock abstraction only**, with EDP-Clock's own existing transitive EDP-System and EDP-Platform dependencies. It introduces no Logging→Mesh, no new platform concrete and no reverse edge. The application-selected coherent revision-atomic capture provider is **not** supplied by EDP-Logging and must be Bootstrap-composed and qualified by the Clock/coordinator owner. The default Logging umbrella remains dependency-free.

### Additional opt-in G3 generated diagnostic argument adapter

**2026-10-16 widened kinds, no graph change:** the same optional EDP-Serialisation + generated EDP-Localisation owner profile now accepts bounded String/Bytes from EDP-BoundedTypes and System Type/Field identifiers already supplied by the **existing** Serialisation/System dependency tree. No new required library, SDK/Clock/Mesh/provider edge, no change to Logging default umbrella.

`logging/DiagnosticArgumentEncoding.hpp` activates a direct abstract dependency on `EDP-Serialisation` at its **`feature/logging-v1` positional CBOR/JSON API**, and on the generated **EDP-Localisation** diagnostic message schema descriptor Type provided by an application. The header itself imports the EDP-Serialisation umbrella; the pre-existing Serialisation mandatory transitive dependencies are EDP-System, EDP-BoundedTypes, EDP-BoundedTopology, EDP-Memory, EDP-Platform-Portable and EDP-Localisation with EDP-Platform/System underneath. This is a purely **opt-in** G3 profile, never imported by default. No reverse dependency Serialisation→Logging and no new platform-concrete library chosen by Logging. Generated diagnostic schema objects must be provided as compile-time descriptor Types and are *not* runtime registries.

## Planned but not yet activated by Logging

- EDP-System — governed Logging root TypeIdentifier, generated schemas and Composition
- EDP-Localisation — historical schema/pack contracts
- EDP-Serialisation — bounded typed CBOR/JSON encoding
- EDP-Persistence — archive sink provider

EDP-Serial remains a prospective **downstream** optional consumer of EDP-Logging. EDP-Logging **must not depend on EDP-Serial** or on the deprecated `ESPressio-*` libraries.

## Acyclicity and conditional graph audit

This optional activation has no new foundational reverse edge or cycle. EDP-Memory remains independent of EDP-Threading, and EDP-Serialisation remains independent of EDP-Logging. The future application Composition graph, active concrete providers and target build graph need separate requalification before any integration is labelled production-ready.

### Optional G5 Python host-only archival resolver (additional, no C++ graph edge)

`edp_logging.historical_schema` lazily imports **EDP-Localisation's existing Python archival verifier/GeneratedContractFamily** only when an offline CLI caller opts into `--historical-archive`. All embedded `src/logging` code remains unchanged and does not import Python, EDP-Serial or platform concretes. The archive library retains owner responsibility for EDPL/manifest verification; EDP-Logging checks the selected diagnostic schema using its own current owner tools. This conditional tooling edge is **EDP-Logging host tooling → EDP-Localisation tooling**, not a compiled dependency cycle. See `docs/HISTORICAL-DIAGNOSTIC-SCHEMA-BRIDGE.MD`.

### G3 generated System strong identities (conditional existing DAG)

The existing opt-in Logging -> EDP-Serialisation -> EDP-System & EDP-BoundedTypes graph now admits two more owner-certified types: `System::Identity::DeviceIdentifier` and `System::Identity::RuntimeIncarnationId`. No new Clock, Mesh, EDP-Serial-ESP-IDF or platform concrete dependency introduced; the C++ default Logging umbrella is unchanged.

### Optional ClockEra diagnostic profile (activated only by exact header)

`<logging/DiagnosticClockEraArguments.hpp>` activates optional **Logging → EDP-Serialisation** and **Logging → EDP-Clock** direct abstract owner edges, together with `EDP-Serialisation → EDP-Clock` only for its own optional `ClockEraAdapters.hpp`. All edges point toward Clock/System/Platform foundations; **no reverse Clock→Logging/Serialisation, no Mesh import and no platform SDK import**. The default Logging diagnostic header retains its previous dependency profile and only carries an inactive owner-type certification trait. The consuming application declares EDP-Clock when opting into this header.

## Optional G1 bounded record admission activation (new)

The opt-in header logging/BoundedRecordAdmission.hpp uses exactly the preexisting direct abstract EDP-Clock, EDP-Threading, EDP-Platform and EDP-BoundedTopology edges of TransactionalRecordQueue and AdmissionLifecycleCoordinator plus the supplied EDP-Memory pool/lease contract. No new dependency, reverse edge, platform-specific SDK, circular dependency, or private allocation/wake implementation is introduced. Application Bootstrap selects every concrete implementation. The default Logging umbrella remains dependency-free. See [contract](docs/BOUNDED-RECORD-ADMISSION.MD).


## 2026-10-16 — conditional source producer/consumer dependency audit

The opt-in source logging/RecordProduction.hpp includes existing optional
BoundedRecordAdmission.hpp, AttemptSequence.hpp and LogTemporalEvidence.hpp.
Consequently its direct abstract owner requirements are EDP-Clock,
EDP-Threading, EDP-Platform and EDP-BoundedTopology. EDP-Memory
is used through Bootstrap-selected ObjectPool/Lease template contracts.
No new platform-specific implementation or ad hoc Memory/Clock facility is
created. Default ESPressio_Logging.hpp stays standard-C++ only.

The opt-in physical HIL APPLICATION separately includes EDP-Serialisation
and EDP-Localisation for test-only CBOR argument encoding, and
EDP-Platform-FreeRTOS/Portable for Bootstrap composition. These are not
mandatory Logging library edges.

**There is no EDP-Logging -> EDP-Mesh dependency.** Monotonic-only
CaptureCoherent is an accepted configuration and full basic Logging
functionality must remain usable in the complete absence of Mesh.
When Mesh happens to feed a synchronized Clock in another application,
revision-coherent root/Era acquisition is the responsibility of that
application's Clock/Mesh integration, not a universal prerequisite for
EDP-Logging. No reverse Clock/Memory/Serialisation dependency on Logging.

The G4 full canonical LogRecord typed root remains separate and blocked
by EDP-System-governed permanent TypeIdentifier issuance.

## Optional EDLG KeyValueStorage checkpoint (2026-10-16, independent)

- EDP-Logging [only logging/EdlgCheckpoint.hpp]
  - EDP-Persistence [DIRECT abstract KeyValueStorage]
    - EDP-System [TRANSITIVE Composition]
  - EDP-Logging own canonical EdlgFrameReader and EdlgFrame writer [INTERNAL]
- Application ESP32 HIL Bootstrap [separately, NOT a mandatory Logging edge]
  - EDP-Persistence-ESP-IDF [concrete NVS selected by Bootstrap]
  - EDP-Serial [abstract bounded frame queue selected by Bootstrap]
  - EDP-Serial-ESP-IDF [concrete unbuffered native UART FIFO writer]
  - EDP-Platform-Portable [EDP-Memory ByteOperations concrete used by NVS]
  - EDP-Threading [exclusive fixed FIFO writer arbitration]
  - EDP-Clock, EDP-Memory, EDP-Serialisation, EDP-Localisation
    [already activated for prior independent HIL source/encoding tests]

This conditional graph introduces NO Logging-to-Serial, Logging-to-Mesh,
Logging-to-ESP-IDF or non-abstraction reverse edge. KeyValueStorage snapshot
is explicitly NOT a substitute for persistent EDLS rolling archive.

## Optional embedded EDLS inspection

- EDP-Logging [logging/EdlsArchiveInspection.hpp only]
  - EDP-Persistence [direct abstract FileStorage and validated views]
    - EDP-System [transitive Composition]
  - Logging/EdlsSegment, Logging/EdlgFrameReader [internal canonical readers]

EDP-Logging has no mandatory VFS, Mesh or Serial dependency.

## Optional EDLS checked append

- EDP-Logging [logging/EdlsArchiveAppend.hpp only]
  - EDP-Persistence [direct abstract FileStorage::AppendFile/GetFileSize]
    - EDP-System [transitive Composition]
  - Logging/EdlsArchiveInspection and Logging/EdlgFrameReader
    [internal existing owner validation]

No concrete filesystem, Mesh, Serial or supplementary allocator.


## Optional EDLS successor publication

- EDP-Logging [logging/EdlsArchiveSuccessor.hpp only]
  - EDP-Persistence [abstract FileStorage::ReplaceFile/GetFileSize/ReadFileAt]
    - EDP-System [transitive Composition]
  - Logging/EdlsArchiveInspection, EdlsSegment, EdlgFrameReader
    [internal previously approved source]
- Application-specific ESP32 HIL Bootstrap separately chooses
  EDP-Persistence-ESP-IDF::VfsFileStorage with SPIFFS mounted by ESP-IDF,
  EDP-Platform-Portable::Memory::ByteOperationsProvider. This is NOT
  a Logging source dependency on any native ESP-IDF or concrete VFS.


## Optional governed LogRecord semantic identity

- EDP-Logging [logging/LogRecord.hpp and
  logging/OwnedDiagnosticArguments.hpp, deliberately opt-in]
  - EDP-System [TypeIdentifier, FieldSet and source strong identity]
  - EDP-Clock [strong monotonic/synchronized/era/uncertainty values]
  - EDP-Localisation [generated owner strong StringIdentifier inputs,
    widened ONLY in the in-memory source representation]
  - EDP-Logging AttemptSequence/Severity [existing internal Types]
- EDP-System/main/docs/TYPE-IDENTITY-REGISTRY.MD is the offline
  authoritative allocation record, NOT a code runtime dependency.
- This addition does not import any concrete platform provider,
  EDP-Mesh, native file adapter or run-time identity allocation.

## Optional generated-source LogRecord assembly (2026-10-17)

Only applications importing `logging/LogRecordSourceAssembly.hpp` activate the
combined existing abstract edges to EDP-System (strong identities, schema),
EDP-Clock (one already-qualified monotonic/synchronized sample),
EDP-Localisation **generated** `DiagnosticMessageSchema` and
`OwningContract::DiagnosticSchemaFingerprint`, and EDP-Serialisation
(through the already-existing opt-in `DiagnosticArgumentEncoding.hpp`
for compile-time schema kind certification). Bootstrap provides the concrete
Clock source and memory/task resources independently. No reverse dependencies,
EDP-Mesh, native SDK, redundant encoder or runtime catalogue registry
are introduced. The root remains structural, **not** a canonical
15-field `SerialisableType`. Applications must use at least the
Localisation `feature/logging-v1` generated descriptor owner checkpoint
with `SourceDomain`/`SourceSubDomain`/`SourceString` and
`OwningContract`, not an older generated fixture. Package version 0.1.0.
