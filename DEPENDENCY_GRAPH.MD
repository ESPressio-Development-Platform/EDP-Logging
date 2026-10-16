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

### Additional optional G2 coherent-clock evidence qualifier

`#include <logging/LogTemporalEvidence.hpp>` activates a direct dependency on **EDP-Clock abstraction only**, with EDP-Clock's own existing transitive EDP-System and EDP-Platform dependencies. It introduces no Logging→Mesh, no new platform concrete and no reverse edge. The application-selected coherent revision-atomic capture provider is **not** supplied by EDP-Logging and must be Bootstrap-composed and qualified by the Clock/coordinator owner. The default Logging umbrella remains dependency-free.

## Planned but not yet activated by Logging

- EDP-System — governed Logging root TypeIdentifier, generated schemas and Composition
- EDP-BoundedTopology — own FIFO queue and compact queue identities
- EDP-Localisation — historical schema/pack contracts
- EDP-Serialisation — bounded typed CBOR/JSON encoding
- EDP-Persistence — archive sink provider

EDP-Serial remains a prospective **downstream** optional consumer of EDP-Logging. EDP-Logging **must not depend on EDP-Serial** or on the deprecated `ESPressio-*` libraries.

## Acyclicity and conditional graph audit

This optional activation has no new foundational reverse edge or cycle. EDP-Memory remains independent of EDP-Threading, and EDP-Serialisation remains independent of EDP-Logging. The future application Composition graph, active concrete providers and target build graph need separate requalification before any integration is labelled production-ready.
