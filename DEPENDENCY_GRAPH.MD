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
