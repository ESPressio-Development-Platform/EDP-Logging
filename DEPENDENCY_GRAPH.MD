# EDP-Logging Dependency Graph

**Implementation baseline:** current source on this branch; publication will promote this exact source to `main` only after qualification. **Package:** 0.1.0. The dependency graph is conditional on the headers and Bootstrap capabilities selected by the application. The graph retains dated audits as historical implementation provenance. The current implementation surface is defined by [architecture](docs/ARCHITECTURE.MD), the [G1–G15 owner matrix](docs/ACCEPTANCE-G1-G15.MD), and the current activation summary below. The conditional package graph below must be verified against the actual header profile and published owner revisions. The complete cross-repository manifest pin scan remains a separate, explicitly scoped acceptance check; historical notes alone do not certify all optional profiles.

A pinned [current G14 owner and manifest-branch baseline](docs/DEPENDENCY-OWNER-BASELINE.MD) documents directly relevant repository heads, source-import owners, branch mismatches and the exact limits of cross-repository coverage. [Optional G12 Serial output](docs/SERIAL-OUTPUT-INTEGRATION.MD) is a downstream Serial-to-Logging feature, not a default Logging dependency.

## Published-main dependency baseline — 12 November 2026

The **current** implementation's application-selected optional owner source contracts are publicly promoted on their respective **`main`** branches (all versions `0.1.0`): `EDP-Clock 9a27b12d5938dbd1cb26af541a489531a848ba25`, `EDP-Localisation 8b826f4c9becfa34eb3a885361f70033f8ff3f22`, `EDP-Serialisation f36e615720ce093a03482ae710afe129d031a551`, `EDP-Platform-Portable 267411bf319a269883d2273928ad19fc05e2f324`, `EDP-Memory 9c5a994eb1e101b80a6fb0e1357c0add0e3a38c4`, and `EDP-Threading 1c5ff530c7051d0ccb9480c9a46fbb3cd9424750`. These are independently read GitHub main heads, **not** pinned changes to every existing application's manifest. Post-Threading-main 10 November RPI400 detached `20261110T144522-0b07e433` **81/81 PASS** includes full selected Logging GCC/Clang host tests and all three Logging demo families' native PlatformIO Arduino/ESP-IDF compile/link. Actual physical relay and finite-throughput evidence predates these docs-only commits.

The **default C++20 `ESPressio_Logging.hpp` remains dependency-free**; all owner imports above are conditional upon explicit opt-in headers and must be selected by the application and its real provider graph. `EDP-Serial` and `EDP-Serial-ESP-IDF` are **downstream optional consumers** (source still on `feature/logging-v1`), not dependencies that block the core Logging graph. `EDP-Platform-ESP-IDF/main` remains the tested specific concrete profile; `mesh_v1` contains separate Mesh/Radio support and is not silently pulled into Logging. The direct native EDP-Memory finite/Forever blocked waiter is still **not** certified under failed native relock ([Memory #16](https://github.com/ESPressio-Development-Platform/EDP-Memory/issues/16)); the selected producer uses `NoWait` and Threading/Clock owned bounded coordination, not that unsafe waiting path. Historical feature-branch references and 2 November failed older-main profiles elsewhere in this graph retain only their dated provenance.

## Current conditional dependency activation

The root [README Dependencies tree](README.MD#dependencies) is the consuming-developer overview. The manifest has no mandatory EDP library dependencies; the default C++20 umbrella contains severity, attempt sequences, EDLG frame/reader, EDLS segment and instrumentation.

| Feature imported by the application | Conditional direct dependencies and owning contracts |
| --- | --- |
| Admission, queues, lifecycle, worker and sink fan-out | EDP-Clock, EDP-Threading, EDP-Memory, EDP-Platform and EDP-BoundedTopology as selected by each exact header |
| Optional temporal Clock/Era evidence | EDP-Clock; synchronized Clock qualification is an application responsibility and does not require Mesh imports |
| Diagnostic arguments, generated records and codecs | EDP-Serialisation; EDP-Localisation generated schemas; EDP-System identity and EDP-BoundedTypes |
| Owned diagnostic UTF-8 and runtime copying | EDP-Serialisation validated UTF-8, existing EDP-Memory byte operations and optional EDP-Platform-Portable copy provider |
| Durable EDLS/EDLG persistence | EDP-Persistence abstract FileStorage/KeyValueStorage and independently qualified application-selected concrete implementations |
| Optional Python historical diagnostic catalogue | EDP-Localisation host tools only for historical-resolution mode; no embedded dependency |
| Complete HIL application and demos | Explicit host, ESP-IDF or Arduino platform providers are application/test dependencies only |

Transitive closure of relevant existing owners: EDP-Clock reaches EDP-System and EDP-Platform; EDP-Memory reaches EDP-System, EDP-Platform and EDP-BoundedTopology; EDP-Threading reaches Clock, Memory, Platform, System and BoundedTopology; EDP-Serialisation reaches System, BoundedTypes, BoundedTopology, Memory, Localisation and Platform-Portable for its chosen codec profile; EDP-Persistence reaches System. The application explicitly lists its direct dependencies and chooses concrete platform providers at Bootstrap. EDP-Serial and EDP-Mesh are not required by Logging.

The dated profiles below describe earlier implementation discoveries. Statements about what was *then* unfinished are historical; current source and linked architecture/acceptance documents own the actual contracts. This README/documentation edit does not introduce any implementation or dependency edge.

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

`#include <logging/AdmissionLifecycleCoordinator.hpp>` activates exactly the **same** Clock, Memory, Threading, Platform abstract EDP dependency set as the prior bounded-admission/wake coordinator and selects either the original always-lock-free standard C++20 words or an explicitly supplied existing Platform AtomicWord32 provider through its public abstract contract. The concrete `TWaitWakeProvider` must be chosen through Bootstrap and supply one independent extra EDP-Threading drain slot (capacity at least producer-count + 1). No new direct/transitive EDP edge, concrete implementation import, Memory finite acquisition or circular dependency exists. The coordinator remains excluded from `ESPressio_Logging.hpp`. No old EDP-* source was modified by this branch.

### Additional optional G1 tracked RAII lease ownership

`#include <logging/TrackedAdmissionCoordinator.hpp>` activates exactly the SAME abstract **EDP-Clock / EDP-Memory / EDP-Threading / EDP-Platform** dependency graph as the opt-in G1 lifecycle. It introduces only standard C++20 lock-free atomic counters and private owned EDP-Memory lease storage, while delegating all Memory capacity release and wake publication to already-composed G1/Threading abstractions. Neither Logging default umbrella nor any foundational dependency acquires a new reverse edge, and there is no EDP-Memory native finite Acquire call. Bootstrap chooses concrete providers; the tracked guard cannot make a raw lease outside its owner safe or cancel unknown external tasks.

### Additional optional G1 transactional record queue (implemented partial)

`#include <logging/TransactionalRecordQueue.hpp>` activates a **direct optional EDP-BoundedTopology dependency**, in addition to the existing EDP-Threading/Clock/Memory/Platform abstractions via the included `AdmissionWakeCoordinator.hpp`. Its state consists of the canonical bounded slot-occupancy bitmap, the canonical non-owning intrusive FIFO endpoints and each queue record's compact Next link, plus one move-only Memory lease, generation token and retirement coordination bit/state. All object allocation, destruction and active lease backing remain in **EDP-Memory**; the caller/Bootstrap selects the Threading ordinary mutex and bounded wait/wake providers. No platform-specific provider, EDP-Serial, EDP-Mesh, or Logging→Clock implementation dependency is introduced. This opt-in code is NOT imported by the core Logging umbrella.

No reverse EDP-BoundedTopology→Logging/Memory dependency exists. Queue ‘Commit’ FIFO is relative to successful commits, not producer arrival or instrumentation ticket order. See `docs/TRANSACTIONAL-RECORD-QUEUE.MD` for unimplemented RAII/end-to-end producer limitations and the exact host + real EDP-Memory tests.

### Additional optional G2 coherent-clock evidence qualifier

`#include <logging/LogTemporalEvidence.hpp>` activates a direct dependency on **EDP-Clock abstraction only**, with EDP-Clock's own existing transitive EDP-System and EDP-Platform dependencies. It introduces no Logging→Mesh, no new platform concrete and no reverse edge. The application-selected coherent revision-atomic capture provider is **not** supplied by EDP-Logging and must be Bootstrap-composed and qualified by the Clock/coordinator owner. The default Logging umbrella remains dependency-free.

### Additional opt-in G3 generated diagnostic argument adapter

**2026-10-16 widened kinds, no graph change:** the same optional EDP-Serialisation + generated EDP-Localisation owner profile now accepts bounded String/Bytes from EDP-BoundedTypes and System Type/Field identifiers already supplied by the **existing** Serialisation/System dependency tree. No new required library, SDK/Clock/Mesh/provider edge, no change to Logging default umbrella.

`logging/DiagnosticArgumentEncoding.hpp` activates a direct abstract dependency on `EDP-Serialisation` at its **published `main` positional CBOR/JSON API**, and on the generated **EDP-Localisation** diagnostic message schema descriptor Type provided by an application. The header itself imports the EDP-Serialisation umbrella; the pre-existing Serialisation mandatory transitive dependencies are EDP-System, EDP-BoundedTypes, EDP-BoundedTopology, EDP-Memory, EDP-Platform-Portable and EDP-Localisation with EDP-Platform/System underneath. This is a purely **opt-in** G3 profile, never imported by default. No reverse dependency Serialisation→Logging and no new platform-concrete library chosen by Logging. Generated diagnostic schema objects must be provided as compile-time descriptor Types and are *not* runtime registries.

## Historical planning baseline (superseded by later conditional implementations)

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
  - EDP-Serialisation [strict UTF-8 validation in opt-in owned arguments]
  - EDP-Memory [ByteOperations trait and owner-approved CopyBytes]
  - EDP-Platform-Portable [stateless C++ ByteOperations concrete DEFAULT,
    selected only in this opt-in header; application can supply another
    stateless EDP-Memory-compliant provider at compile time]
  - EDP-Logging AttemptSequence/Severity [existing internal Types]
- EDP-System/main/docs/TYPE-IDENTITY-REGISTRY.MD is the offline
  authoritative allocation record, NOT a code runtime dependency.
- This feature imports the expressly selected wholly portable Memory
  byte-copy default (just as EDP-BoundedTypes does), but no native SDK,
  EDP-Mesh, native file adapter or run-time identity allocation. Its default
  C++20 umbrella remains independent of all these optional imports.
  Callers must declare EDP-Memory and EDP-Platform-Portable when importing
  this source; choosing an alternative provider is an application-level
  stateless compile-time policy decision, not a runtime pointer/heap owner.

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


## 2026-10-17 bounded Logging coordinator quantum

The opt-in `logging/CoordinatorQuantum.hpp` imports only
`RecordProduction.hpp` and directly reuses
`DrainOneRecordToSinks` over the already selected
EDP-Memory/Threading/BoundedTopology queue composition.
No new edges, platform concrete import, or additional scheduler.
The Dedicated Thread and latched wake provider are still
application Bootstrap obligations, not provided here.
The #10 correction admits 1..8 static sink callbacks (rather than 2..8)
through direct drain, bounded quantum and dedicated coordinator without
changing any dependency, ownership, queue, signal or worker edge.

## 2026-10-17 opt-in EDP-Threading Dedicated Coordinator

`logging/DedicatedCoordinator.hpp` imports ONLY the approved public
`ESPressio_Threading.hpp` plus `CoordinatorQuantum.hpp` and
references the application-owned static resource topology. No
EDP-Mesh, third-party scheduler, SDK-specific native task or dynamic
provider. The physical ESP-IDF HIL separately selects
EDP-Platform-ESP-IDF execution and SpinLock concrete owners at
Bootstrap, demonstrating the intended architectural separation.
See `docs/DEDICATED-COORDINATOR.MD`.

## G10 native encoded buffer (2026-10-17)

`logging/OwnedRepresentationFanout.hpp` composes
`EDP-Memory::ObjectPool` (capacity and movable lease),
EDP-Platform finite NoWait policy, and the existing Logging
source/sink status/EDLG codec enums; no direct EDP-Serial or
Persistence provider dependency. Application tests select real
`EDP-Serial::BoundedEdlgFrameQueue` copy-on-enqueue, whose
owning mutex is selected from EDP-Threading. Full G4 canonical
TypedEnvelope implementation remains with EDP-Serialisation.

## G11 WROOM fixture metrics (2026-10-18)

Only the ESP-IDF physical HIL application selects the concrete
ESP-IDF heap-capability metrics, FreeRTOS task-stack watermark,
and esp_timer monotonic provider. Portable Logging has no added
platform resource-provider dependency; measured readings cover the
complete test application, not Logging's own isolated overhead.


## G11 whole-firmware test dependency closure

The G11 comparator is test-only. Its Bootstrap links the existing explicit EDP-Clock, EDP-Memory, EDP-Threading, EDP-Serialisation, EDP-Localisation, EDP-Persistence, EDP-Serial, EDP-Platform and System/Bounded* owner abstractions and selected concrete ESP-IDF/FreeRTOS providers through the caller-pinned dependency directory. Both builds share the same closure, SDK settings and application source. This adds no new dependency (mandatory or optional) to production EDP-Logging and no Mesh coupling or circular dependency.

## G10 single-sink composition dependency revalidation (issue #7)

Allowing one copying sink does not introduce any direct or transitive
dependency: the existing optional OwnedRepresentationFanout.hpp
still requires application-selected abstract EDP-Memory and EDP-Platform,
with the encoder and concrete Serial/Persistence adapters at Bootstrap.
No new owner, Mesh coupling or circular edge was introduced. The 1-sink
and 2-sink regression cases use the same optional EDP-Memory/Serial closure.


## G4 approved Field 11/14 opt-in conversion dependency closure

The optional `logging/LogRecordApprovedFieldsSerialisation.hpp` imports
EDP-Serialisation and EDP-BoundedTypes to adapt Logging-typed values,
and consumes EDP-Clock owner statuses through LogRecord. These are
optional application composition edges, not mandatory Logging foundation
imports. EDP-Serialisation owns both CBOR/JSON codecs, no duplication or
new Mesh dependencies, package versions remain at 0.1.0.


## 2026-10-20 optional G4 core wire adaptation

`logging/LogRecordCoreFieldSerialisation.hpp` is opt-in at
application composition and imports the existing EDP-System,
EDP-Clock, EDP-Serialisation and EDP-BoundedTypes abstractions,
with EDP-Serialisation retaining CBOR/JSON ownership. Neither the
Logging base umbrella nor dependency direction changed; no direct
Mesh/Persistence/Serial implementation was added. Enum width
certification does not bypass full record semantic validation.


## G4 optional heterogeneous generated Record projection

`GeneratedLogRecordWireProjection` is an opt-in Logging
source/Bootstrap-facing adapter that consumes EDP-System SchemaType,
EDP-Localisation generated message identity and fingerprint,
EDP-BoundedTypes canonical pairwise conversion and
EDP-Serialisation's owner-certified fixed heterogeneous Tuple
Schema Fields. No new Logging-local CBOR/JSON writer, StringIdentifier,
Clock, owner allocator or Mesh dependency is introduced.
EDP-Serialisation never imports EDP-Logging; feature branch
`feature/logging-v1` is required until the owner facility is
independently released. Library versions remain exactly 0.1.0.



### 2026-10-22 G4 whole-record stack-qualification dependency review

This fixture-only change adds FreeRTOS task-stack telemetry inside
`tests/hil/g1-espidf/src/g4_whole_record_check.hpp` and reduces the
**application HIL-only** app_main stack configuration from 16 KiB to
14 KiB. It adds **no** new mandatory or optional production EDP-Logging
dependencies, and the original EDP-Serialisation owner remains the
only CBOR/JSON writer. No reusable memory provider was introduced.

## 2026-10-22 optional G10 async copying Task callback (#11, partial)

`logging/AsyncCopyingTaskSink.hpp` adds **no new mandatory** dependency.
Opt-in applications consume exactly the existing abstract EDP-Memory,
EDP-Threading and EDP-Platform paths already present in G1/G10 source.
All bounded payload acquisition and explicit release use
EDP-Memory::ObjectPoolLease; payload copying uses the composable
EDP-Memory::ByteOperations capability, with a portable provider selected
by the application (the native host fixture uses EDP-Platform-Portable).
All async dispatch, Task handle, cancellation and terminal completion
remain in EDP-Threading's preexisting `Task` and bounded
`DedicatedWorkerLease`/TaskExecutionFacility design. No Logging-owned
Task worker, native mutex/signal, internal queue, elapsed-time source,
platform concrete, Serial/Persistence concrete or reverse EDP dependency
is introduced. Owner-cancellation/reclaim remains an explicit Bootstrap
lifecycle duty until product HIL/reliability tests complete.


## 2026-10-23 optional EDLS oldest-eligible rotation

`EdlsArchiveRotation.hpp` adds **no new mandatory or optional repository**
dependencies. It consumes the already-selected abstract EDP-Persistence
FileStorage contract, canonical EDLG writer/reader and EDLS inspector,
plus the existing guarded Empty-slot successor helper. Application Bootstrap
continues to select ESP-IDF/host concrete VFS and to hold the unique
mutation lock. No direct ESP-IDF include or EDP-Mesh coupling occurs in
Logging. Rotation cannot strengthen a provider's weak PowerLoss guarantees.


## 2026-10-23 — optional PowerLoss EDLS snapshots

`logging/PowerLossEdlsSnapshots.hpp` consumes ONLY the existing
abstract `EDP-Persistence::KeyValueStorage` and canonical existing
Logging EDLG/EDLS formatters. A compile-time guarantee gate prevents
instantiation with Restart-only/None-interruption-atomic providers.
The application Bootstrap chooses EDP-Persistence-ESP-IDF NVS or
another qualified provider; no concrete SDK import, second allocator,
queue, worker, clock or reverse dependency is introduced. This
latest-generation profile is not the multi-record EDLS FileStorage
archive; the non-atomic VFS profile remains unchanged.


## 2026-10-23 — optional strict historical EDLS journal

Optional logging/PowerLossEdlsJournal.hpp adds **no new repository dependency**: it composes the existing PowerLossEdlsSnapshots owner and the same optional abstract EDP-Persistence::KeyValueStorage contract, EDP-System composition requirements and existing EDLG/EDLS encoding. Application Bootstrap selects the already-existing PowerLoss-atomic ESP-IDF NVS provider. No new raw flash driver, worker, queue, mutex, allocator, Clock, Mesh or reverse dependency. No change to SPIFFS FileStorage guarantees or versions 0.1.0.


## 2026-10-26 — immutable EDLS protected N+1 rolling dependency revalidation

The opt-in physical spare/rolling functions are inside existing `logging/EdlsArchiveRotation.hpp`. They consume the *already-declared optional* **abstract EDP-Persistence FileStorage** contract, EDLG/EDLS existing CRC reader/writer, existing archive inspection, guarded empty-slot successor and fixed C++20 storage. No new mandatory or optional ESPressio dependency edge, direct platform SDK import, memory allocator, independent worker, Clock, Mutex, Flash adapter or reverse dependency is added. The real host VFS tests use EDP-Persistence-ESP-IDF as an **application/test-level concrete** only. Complete previously documented production graph remains unchanged, acyclic, and all versions remain 0.1.0.


## 2026-10-26 native real EDLS LittleFS fixture (test dependency only)

Optional test-only \`tests/hil/g7-littlefs-protected-rolling-espidf\` consumes
the existing Logging EDLG/EDLS headers plus abstract EDP-Persistence,
concrete EDP-Persistence-ESP-IDF LittleFsVfsMount and the already-existing
joltwallet/littlefs ESP-IDF component at the **application/test edge**.
Existing EDP-Platform-Portable EDP-Memory ByteOperations is explicitly
selected by test Bootstrap. This adds **zero production** EDP-Logging
dependencies, no new Flash owner or cross-repo cycle, and no SDK import
in the Logging domain-neutral library. All versions remain 0.1.0.

## 2026-10-26: generated typed CBOR-to-archive composition

`logging/GeneratedLogRecordEdlsArchive.hpp` is a thin optional application-worker composition of existing domain-neutral Logging generated schema-projection, existing **EDP-Serialisation** canonical TypedEnvelope CBOR encoder, existing EDLG/EDLS frame/immutable archive owners, and the already-approved optional abstract **EDP-Persistence FileStorage**. It adds no new direct SDK, Clock platform, allocation provider, threading engine, or mandatory dependency. The test selects EDP-Persistence-ESP-IDF VfsFileStorage **outside** Logging production ownership. No circular dependency or new EDP component. All versions 0.1.0.

## 2026-10-26 existing Task-worker CBOR/EDLS adapters

`logging/GeneratedEdlsTaskAdapters.hpp` optionally composes **already declared** Logging→EDP-Memory, EDP-Threading, EDP-Serialisation and abstract EDP-Persistence edges. No new component, concrete provider or second worker/pool/codec edge is introduced. Existing `AsyncCopyingTaskSink` and `EncodeOnceAndSubmitToSinks` are reused; Task delivery gets a borrowed copy only while EDP-Memory lease stays active. All versions 0.1.0.

## 2026-10-26 bounded high-payload scratch ownership

The existing Logging EDLS ArchiveInspection/Successor/Rotation and typed
CBOR/Task adapters now accept **caller-owned** reusable archive scratch
through additional WithScratch APIs, sourced from existing application
Bootstrap EDP-Memory or static owner. No new allocator, Flash provider,
queue, worker, SDK import or EDP dependency; all internal scans use the
same original EDP-Persistence abstraction. All versions remain 0.1.0.

## Oct 27 real existing Task-owner tests and typed native fixture

Additional tests exercise the already declared EDP-Memory ObjectPool, EDP-Threading Task<T>, EDP-Serialisation typed CBOR, and EDP-Persistence FileStorage paths; the optional Logging worker-side TypedCborProtectedEdlsTaskDelivery remains an adapter, not a new worker, codec, allocator or provider. Native ESP-IDF and LittleFS are selected only in test/bootstrap fixture, not Logging. Versions fixed 0.1.0.

## Strict finite EDLS acknowledgment (Oct 27)

`src/logging/EdlsPowerLossAcknowledgement.hpp` consumes only the **already declared EDP-Persistence** FileStorage and KeyValueStorage abstraction and the existing Logging EdlsArchiveRotation/EdlsSegment CRC implementation. Selection of NVS PowerLoss-atomic KeyValueStorage versus conservative LittleFS FileStorage remains exclusively Bootstrap/application composition. No SDK imports, alternate owner, dynamic allocator, task, or codec added in EDP-Logging; existing EDP-Serialisation typed CBOR still frames log records. Version 0.1.0.

## Strict NVS-acknowledged full typed LogRecord delivery

The existing `TypedCborProtectedEdlsTaskDelivery` generated 15-field TypedEnvelope adapter now supports an **optional compile-time injected** `PowerLossAcknowledgedEdls` owner. This is not a second CBOR serializer or Task/sink implementation: every code path shares the exact existing EDP-Serialisation canonical typed-envelope decode, re-encoding validation, EDLG frame writer and EDP-Threading Task callback. For the strict owner ONLY, `RecordSinkSubmission::Accepted` and `TypedEdlsTaskCompletion::DurablyAcknowledged` require a successful NVS PowerLoss-atomic acknowledgement commit/readback; the older FileStorage-only path retains its preexisting `Archived` completion and weaker durability semantics. In either mode, admission to `AsyncCopyingTaskSink` is NEVER a durability ACK. `AcknowledgedPendingRetirement` is accepted only after a committed watermark and verified successor; later publishing must successfully resume retirement first.

`tests/GeneratedEdlsStrictAckTaskIntegrationTests.cpp` reuses the actual existing EDP-Memory pool, bounded lease ownership, owner-generated Localisation schema and full 15-field canonical CBOR source, EDP-Threading deferred Task executor, POSIX genuine EDP-Persistence FileStorage and explicit PowerLoss-capability EDP-Persistence KeyValueStorage fake. It validates 17 separately Task-delivered generations, cross-boot incarnation 7->8, exact rolling physical latest two EDLS segments, no ACK on pending admission, terminal ACK only AFTER NVS commit, no false Accepted if KeyValueStorage StoreValue fails, correct failure terminal and retries after safe candidate reclamation. This is genuine C++ owner-composition testing but **not yet** proof of a deployed ESP32 scheduling, flash/worker stack, or wear budget; native HIL separately exercises strict typed NVS and EDLS on the physical chip.

Versions 0.1.0 unchanged; no new allocator, thread, codec, persistence store or SDK imports in production Logging code.


Revalidated 2026-10-30: post-retirement duplicate scan removal changes no includes, capabilities or mandatory/optional dependency edges; existing abstract Persistence ownership is retained.


## Optional bounded EDLS batching (30 October 2026)

Including logging/BoundedEdlsBatch.hpp directly consumes EDP-BoundedTypes and EDP-Clock plus existing strict-archive EDP-Persistence/System. Bytes owns the inline finite frame buffer and consumes EDP-Memory and its existing portable default provider. No mandatory dependency is added to the minimal umbrella, nor a concrete SDK/Mesh/Serial edge. Application-selected Threading scheduling remains external. Existing transitive graphs above were revalidated for these headers.

## Native sealed-batch HIL Clock binding (2026-10-30)

Only the optional tests/hil/g7-littlefs-protected-rolling-espidf sealed-batch
APPLICATION adds EDP-Platform-ESP-IDF at adc4164031016c200ad88d742fc4f05c86d66833.
Bootstrap composes its existing EspTimerMonotonicTimebaseProvider with the
canonical EDP-Clock MonotonicClockProvider; this is not a Logging timer/Clock
implementation or a neutral production dependency. The exact consumed timebase
header blob is d5b5709be51f0dd8814a743c0c68546ecdb49ad0. Existing optional
BoundedEdlsBatch abstract owner dependencies and the default umbrella are
unchanged. The native compile workflow records this additional checkout pin;
physical execution and all test coverage run on the authorised appliance.

## 2026-10-30 optional coordinator sink service

CoordinatorSinkService.hpp directly imports the existing abstract
EDP-Clock canonical MonotonicTimestamp. DedicatedCoordinator continues
to consume EDP-Threading and the existing queue/Memory closure; its
optional callback reuses ThreadContext::WaitUntil. GeneratedEdlsTaskAdapters
now includes BoundedEdlsBatch and the service outcome, reusing already
declared optional Clock/Memory/Serialisation/Persistence paths. Bootstrap
selects the one canonical origin and concrete providers. No default
umbrella import, SDK, extra worker/queue/timer/allocator, reverse dependency
or version change is introduced. Native provider bindings remain solely
application/test dependencies. All packages stay 0.1.0.

## Native managed batch worker fixture (test/bootstrap only)

The g7 esp32dev-batch-worker environment explicitly selects existing
EDP-Platform-FreeRTOS Signal/Mutex, EDP-Platform-ESP-IDF execution/spinlock,
portable Memory resource and the existing Threading topology. It reuses
all optional domain dependencies already declared for source production,
typed delivery and batching. No concrete SDK import enters production
Logging; no competing queue, worker, timer, allocator or reverse edge is
added. Native concurrent qualification is pending after a confirmed
NoWait queue Commit Busy result; versions remain0.1.0.

## Existing queue canonical bounded transitions (30 October)

TransactionalRecordQueue now explicitly includes the already-required
abstract EDP-Clock canonical API and standard optional; Clock was already
in its declared admission/wake closure. The existing remaining-budget
conversion moved from BoundedRecordAdmission to shared BoundedAdmission.
DedicatedCoordinator uses the same Clock only for explicit positive
CoordinatorQueueLockWait opt-in. All locking remains the original
Threading ordinary mutex; all release remains original Memory/lifecycle.
No new provider, native SDK, timer, queue, allocator, worker or reverse
edge. New per-slot notification state and per-handle stable view are finite
metadata, not Memory facilities. Versions remain0.1.0.

## Native representation topology isolation (test/bootstrap only)

The managed worker fixture configures a second existing EDP-Memory runtime
for its worker-exclusive single representation pool, reusing the already
qualified G10 Bootstrap composition pattern. Source producers/reclamation
keep their existing source runtime. Each uses existing provider instances
and explicit finite dedicated counts; no EDP-Memory source/API changed,
no allocation implementation or provider is duplicated. This avoids
unrelated NoWait coordination contention between the two pool roles.
Production dependency graph and versions0.1.0 remain unchanged.


## Optional canonical admission lifecycle atomic binding (31 October 2026)

AdmissionLifecycleCoordinator<ProducerCount, WaitWakeProvider, AtomicWordProvider>
uses the already-declared direct abstract Platform dependency for the canonical
AtomicWord32 contract. The third parameter defaults to void, retaining the
original standard-atomic always-lock-free requirement. Explicit Bootstrap
selection uses the existing supplied word, validated capability/properties and
load/store/CAS operations. No concrete SDK/provider or new dependency edge is
introduced into neutral Logging. Concrete ESP-IDF selection belongs only to
the typed demo application; Portable selection belongs only to host tests.
See docs/ADMISSION-LIFECYCLE-ATOMIC-BINDING.MD.

## 10 November 2026 dependency-first main integration update

The previously promoted, independently host- and ESP32-demo-tested owner revisions are now **`EDP-Localisation/main=8b826f4c9becfa34eb3a885361f70033f8ff3f22`**, **`EDP-Clock/main=9a27b12d5938dbd1cb26af541a489531a848ba25`**, **`EDP-Serialisation/main=f36e615720ce093a03482ae710afe129d031a551`**, **`EDP-Platform-Portable/main=267411bf319a269883d2273928ad19fc05e2f324`**, and **`EDP-Memory/main=9c5a994eb1e101b80a6fb0e1357c0add0e3a38c4`**. This is the **conditional generated-record, byte-copy, source-pool and worker ownership baseline** at the time of this documentation update, not a permanent SHA pin of every application. Application manifests should resolve those owners by their current `#main` only after each guarded main promotion and pass downstream tests.

**12 November 2026 supersession:** `EDP-Threading/main` has been independently promoted after its Memory-main qualified native target suite, with full post-Threading-main Logging host/native regression PASS 81/81 (see Policy/main 9 November handoff). Its earlier feature-only status is historical, not an unresolved branch prerequisite. The new minimal `FramingAndInstrumentation` demo uses the dependency-free C++20 Logging umbrella and does not activate any of these optional edges.

**Unqualified Memory API boundary:** [EDP-Memory #16](https://github.com/ESPressio-Development-Platform/EDP-Memory/issues/16) tracks unsafe direct native finite/Forever blocked-waiter provider-failure lifetime and lack of one end-to-end deadline. The qualified Logging bounded-admission profile exclusively uses Memory `NoWait` plus Threading/Clock-owned coordination, **not** the unqualified direct native Memory waiter. A `main` merge of Memory does not upgrade or guarantee this separate public path.


## Complete recursive package-manifest dependency closure — verified 12 November 2026

This **source-checked graph is the actual transitive tree** implied by the eleven opt-in direct owner packages and their current GitHub **`main` `library.json` manifests**. Each repeated node records another real dependency edge, **not** a circular dependency or an instruction to instantiate two provider objects. The root default `ESPressio_Logging.hpp` declares **no mandatory EDP package**: each root edge activates only when the selected source header, generated typed record, worker/service, archive or application Bootstrap imports that owner's capability. The recursive edges inside an activated optional branch are the corresponding owner's **mandatory package-manifest** dependencies; application-specific concrete backends (ESP-IDF, FreeRTOS, etc.) must additionally be selected explicitly.

```text
EDP-Logging [default umbrella: C++20 only]
├── EDP-System [optional direct: LogRecord root, generated identity and composition]
├── EDP-Platform [optional direct: native wait, mutex and producer provider contract]
│   └── EDP-System [mandatory transitive]
├── EDP-Clock [optional direct: qualified temporal fields and absolute deadlines]
│   ├── EDP-System [mandatory transitive]
│   └── EDP-Platform [mandatory transitive]
│       └── EDP-System [mandatory transitive]
├── EDP-Memory [optional direct: finite source, owned representations and record capacity]
│   ├── EDP-System [mandatory transitive]
│   ├── EDP-Platform [mandatory transitive]
│   │   └── EDP-System [mandatory transitive]
│   └── EDP-BoundedTopology [mandatory transitive]
├── EDP-Threading [optional direct: producer wake, bounded service and dedicated worker]
│   ├── EDP-System [mandatory transitive]
│   ├── EDP-Platform [mandatory transitive]
│   │   └── EDP-System [mandatory transitive]
│   ├── EDP-Clock [mandatory transitive]
│   │   ├── EDP-System [mandatory transitive]
│   │   └── EDP-Platform [mandatory transitive]
│   │       └── EDP-System [mandatory transitive]
│   ├── EDP-BoundedTopology [mandatory transitive]
│   └── EDP-Memory [mandatory transitive]
│       ├── EDP-System [mandatory transitive]
│       ├── EDP-Platform [mandatory transitive]
│       │   └── EDP-System [mandatory transitive]
│       └── EDP-BoundedTopology [mandatory transitive]
├── EDP-BoundedTopology [optional direct: finite queue topology and strong indexes]
├── EDP-BoundedTypes [optional direct: bounded text/bytes and typed canonical values]
│   ├── EDP-Memory [mandatory transitive]
│   │   ├── EDP-System [mandatory transitive]
│   │   ├── EDP-Platform [mandatory transitive]
│   │   │   └── EDP-System [mandatory transitive]
│   │   └── EDP-BoundedTopology [mandatory transitive]
│   └── EDP-Platform-Portable [mandatory transitive]
│       ├── EDP-Platform [mandatory transitive]
│       │   └── EDP-System [mandatory transitive]
│       └── EDP-Memory [mandatory transitive]
│           ├── EDP-System [mandatory transitive]
│           ├── EDP-Platform [mandatory transitive]
│           │   └── EDP-System [mandatory transitive]
│           └── EDP-BoundedTopology [mandatory transitive]
├── EDP-Localisation [optional direct: generated diagnostic contract and schemas]
│   ├── EDP-System [mandatory transitive]
│   └── EDP-Memory [mandatory transitive]
│       ├── EDP-System [mandatory transitive]
│       ├── EDP-Platform [mandatory transitive]
│       │   └── EDP-System [mandatory transitive]
│       └── EDP-BoundedTopology [mandatory transitive]
├── EDP-Serialisation [optional direct: canonical CBOR/JSON, UTF-8, root envelope]
│   ├── EDP-System [mandatory transitive]
│   ├── EDP-BoundedTypes [mandatory transitive]
│   │   ├── EDP-Memory [mandatory transitive]
│   │   │   ├── EDP-System [mandatory transitive]
│   │   │   ├── EDP-Platform [mandatory transitive]
│   │   │   │   └── EDP-System [mandatory transitive]
│   │   │   └── EDP-BoundedTopology [mandatory transitive]
│   │   └── EDP-Platform-Portable [mandatory transitive]
│   │       ├── EDP-Platform [mandatory transitive]
│   │       │   └── EDP-System [mandatory transitive]
│   │       └── EDP-Memory [mandatory transitive]
│   │           ├── EDP-System [mandatory transitive]
│   │           ├── EDP-Platform [mandatory transitive]
│   │           │   └── EDP-System [mandatory transitive]
│   │           └── EDP-BoundedTopology [mandatory transitive]
│   ├── EDP-BoundedTopology [mandatory transitive]
│   ├── EDP-Memory [mandatory transitive]
│   │   ├── EDP-System [mandatory transitive]
│   │   ├── EDP-Platform [mandatory transitive]
│   │   │   └── EDP-System [mandatory transitive]
│   │   └── EDP-BoundedTopology [mandatory transitive]
│   ├── EDP-Platform-Portable [mandatory transitive]
│   │   ├── EDP-Platform [mandatory transitive]
│   │   │   └── EDP-System [mandatory transitive]
│   │   └── EDP-Memory [mandatory transitive]
│   │       ├── EDP-System [mandatory transitive]
│   │       ├── EDP-Platform [mandatory transitive]
│   │       │   └── EDP-System [mandatory transitive]
│   │       └── EDP-BoundedTopology [mandatory transitive]
│   └── EDP-Localisation [mandatory transitive]
│       ├── EDP-System [mandatory transitive]
│       └── EDP-Memory [mandatory transitive]
│           ├── EDP-System [mandatory transitive]
│           ├── EDP-Platform [mandatory transitive]
│           │   └── EDP-System [mandatory transitive]
│           └── EDP-BoundedTopology [mandatory transitive]
├── EDP-Persistence [optional direct: abstract FileStorage/KeyValueStorage strict archive]
│   └── EDP-System [mandatory transitive]
└── EDP-Platform-Portable [optional direct: optional stateless byte-copy resources]
    ├── EDP-Platform [mandatory transitive]
    │   └── EDP-System [mandatory transitive]
    └── EDP-Memory [mandatory transitive]
        ├── EDP-System [mandatory transitive]
        ├── EDP-Platform [mandatory transitive]
        │   └── EDP-System [mandatory transitive]
        └── EDP-BoundedTopology [mandatory transitive]
```

**Provenance and pin safety:** Verified individual `main` manifests, all version `0.1.0`, for `EDP-System`, `EDP-Platform`, `EDP-Clock`, `EDP-Memory`, `EDP-BoundedTopology`, `EDP-BoundedTypes`, `EDP-Localisation`, `EDP-Serialisation`, `EDP-Threading`, `EDP-Persistence` and `EDP-Platform-Portable`. Each actual transitive package-manifest edge uses its owner's published `#main`, including Serialisation→Localisation/Memory/Platform-Portable, BoundedTypes→Memory/Platform-Portable, Localisation→Memory, and Portable→Memory/Platform. **This is the complete manifest-level closure for those selected abstract owners, not an assertion that every concrete embedded provider is compatible on every board.** The optional Serial (output) and Mesh (network Clock orchestration) libraries are downstream application choices, **not mandatory imports of the Logging core or its local strict EDLS profile**; see [optional Serial](docs/SERIAL-OUTPUT-INTEGRATION.MD).

The main-source SHAs of Clock, Localisation, Serialisation, Memory, Threading and Portable providers are captured in [owner baseline](docs/DEPENDENCY-OWNER-BASELINE.MD), and independent post-main native/host regressions are captured in the [acceptance matrix](docs/ACCEPTANCE-G1-G15.MD). Explicitly retain **EDP-Memory #16** unresolved direct finite/Forever blocked-waiter failed-relock safety; the configured Logging producer uses the qualified NoWait/wake path. Never read a package manifest pin as certification of a particular source-method timeout, NVS/Flash interruption atomicity, wear/endurance or historical generated schema.
