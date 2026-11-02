# EDP-Logging — Developer Wiki

This is the **internal implementation and extension reference** for EDP-Logging (`0.1.0`). For a consuming developer's introduction, installation, examples and feature navigation, start with the repository [README](../README.MD). For normative public contracts, use [`./docs`](../docs/ARCHITECTURE.MD); the Wiki expands on implementation details but does not redefine those contracts.

## What Logging owns

Logging owns finite diagnostic source capture, attempt sequences, record production and queue admission, dedicated coordinator/sink delivery, EDLG/EDLS framing, sealed batch publication, finite protected rolling and strict acknowledged recovery. It composes existing EDP owners rather than implementing its own allocator, thread scheduler, timer, generic filesystem or CBOR encoder.

Bootstrap selects the existing **Clock**, **Memory**, **Threading**, **System**, **Serialisation**, **Localisation**, **Persistence** and appropriate concrete platform providers when the selected optional feature requires them. Network Mesh and a synchronized remote Clock are **not required** for basic local Logging.

## Implementation map

- [Architecture](Architecture) — ownership and source-level boundaries.
- [Dependency Contracts](Dependency-Contracts) — conditional imports, provider lifetimes and Bootstrap requirements.
- [Public API](Public-API) — supported consuming interfaces.
- [Internal API](Internal-API) and [Private Implementation](Private-Implementation) — helper types, invariants and extension guidance.
- [Resources, Lifecycle and Concurrency](Resources-Lifecycle-Concurrency) — finite source ownership, safe shutdown, worker coordination and persistent acknowledgement.
- [Compiler Definitions](Compiler-Definitions) — optional feature and provider-selection configuration.
- [Build, Test and Source](Build-Test-and-Source) — build procedures, test fixtures, generated records and physical qualification.
- [Reference Index](Reference-Index) — source-oriented public/internal/private declarations and maintained modules.
- [Tooling Reference](Tooling-Reference) — host decoder, schema and forensic tooling.

## Important components

- [Severity](Reference-logging-Severity), [Attempt Sequence](Reference-logging-AttemptSequence) and [Instrumentation](Reference-logging-Instrumentation) form the lightweight C++20 umbrella.
- [Transactional Record Queue](Reference-logging-TransactionalRecordQueue), [Record Production](Reference-logging-RecordProduction) and [Bounded Admission](Reference-logging-BoundedRecordAdmission) use application-owned Memory, Threading and Clock capabilities.
- [LogRecord Source Assembly](Reference-logging-LogRecordSourceAssembly), [Generated Whole-Record Projection](Reference-logging-GeneratedLogRecordWireProjection) and [Owned Representation Fan-out](Reference-logging-OwnedRepresentationFanout) compose generated identities, canonical field encoding and independently owned sinks.
- [Bounded EDLS Batching](Reference-logging-BoundedEdlsBatch) and [Coordinator Sink Service](Reference-logging-CoordinatorSinkService) implement finite flushing and acknowledged delivery.
- [PowerLoss EDLS Journal](Reference-logging-PowerLossEdlsJournal) and [Archive Rotation](Reference-logging-EdlsArchiveRotation) document protected history and recovery.
- [Host Decoder](Host-Decoder) and [Historical Diagnostic Tool](Tooling-edp-logging-historical-diagnostic) cover optional offline processing.

## Qualification boundaries

The core has **finite, configuration-specific** native ESP32-WROOM-32 throughput, resource and relay-controlled PowerLoss evidence, and GCC/Clang regression coverage. It is not a claim that every Flash provider is interruption-atomic, that all hardware sustains the same rate indefinitely, or that any Flash lifespan is guaranteed.

For the authoritative tested configurations, failures, and unfinished optional coverage, consult [G1–G15 Acceptance](../docs/ACCEPTANCE-G1-G15.MD) and [Qualification Status](../docs/LOGGING-QUALIFICATION-STATUS.MD). The historical execution narrative belongs in the governed Policy living handoff, not the Wiki landing page.
