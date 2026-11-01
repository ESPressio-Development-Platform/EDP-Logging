# DedicatedCoordinator — implementation reference

**Classification:** Public optional composition and internal task orchestration. **Owning header:** [src/logging/DedicatedCoordinator.hpp](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/DedicatedCoordinator.hpp). **Version:** 0.1.0, source branch feature/logging-v1. This page describes implementation and owner requirements; it does not replace normative ./docs contracts.

## Responsibility and boundaries

Composes one dedicated Threading worker around an existing queue, service function, wake/deadline observation, and lifecycle state. A worker success status is not a persistence acknowledgement unless the selected sink expressly reports that condition.

## Resource, failure and lifecycle invariants

Bootstrap owns selected Clock/Threading/Memory providers and the worker life cycle; shutdown must join outstanding Tasks and avoid destroying a queue while leases remain.

## Source declarations and responsibilities

The following source-indexed inventory distinguishes defined types/contracts from operations. Descriptions reflect implementation documentation at the cited line; callers must also respect the preconditions, result meanings, provider choices and lifetimes described above.

- **DedicatedCoordinatorExit** (Type or Concept, [line 19](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/DedicatedCoordinator.hpp#L19)): Terminal reason published by one EDP-Threading Dedicated Thread activation.
- **DedicatedCoordinatorReport** (Type or Concept, [line 31](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/DedicatedCoordinator.hpp#L31)): Shared read-only-from-Bootstrap operational report. Only the Dedicated Thread mutates these fields while it is running.
- **CoordinatorQueueLockWait** (Type or Concept, [line 54](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/DedicatedCoordinator.hpp#L54)): Explicit compile-time opt-in to finite locking on the SAME queue and ordinary mutex. guarantee.
- **TQueue** (Type or Concept, [line 97](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/DedicatedCoordinator.hpp#L97)):  Stop is cooperatively requested via Thread::RequestStop() and followed by finite JoinUntil/JoinFor under Bootstrap. This callable exits on stop without silently draining indefinitely. Bootstrap must reject new producers, then choose an explicit bounded shutdown/retention policy. When a native claim cannot be
- **DedicatedCoordinator** (Type or Concept, [line 99](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/DedicatedCoordinator.hpp#L99)): Defines a named type/contract within DedicatedCoordinator; read its source for exact conditional semantics and provider requirements.
- **ServiceSinks** (Method or Function, [line 134](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/DedicatedCoordinator.hpp#L134)): Polls only compile-time opted-in callbacks in the existing single owner. Absolute deadlines cannot be extended by later callbacks or arrivals.
- **operator** (Method or Function, [line 207](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/DedicatedCoordinator.hpp#L207)): Runs finite quanta and optional service inside the one managed Threading context.

## Dependency and test qualification

The implementation owns no replacement EDP-Platform/Memory/Threading/Persistence abstraction. The actual include closure is determined by its opt-in header and the consuming application; the default Logging umbrella remains deliberately narrow. See the current [Dependency Graph](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/DEPENDENCY_GRAPH.MD), [Architecture](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ARCHITECTURE.MD), and [G1–G15 acceptance matrix](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ACCEPTANCE-G1-G15.MD) for exact validation and limitation claims. Never translate an unrelated historical fixture into current production proof.
