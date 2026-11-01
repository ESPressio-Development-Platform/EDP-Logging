# CoordinatorQuantum — implementation reference

**Classification:** Internal coordination primitive. **Owning header:** [src/logging/CoordinatorQuantum.hpp](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/CoordinatorQuantum.hpp). **Version:** 0.1.0, source branch feature/logging-v1. This page describes implementation and owner requirements; it does not replace normative ./docs contracts.

## Responsibility and boundaries

Runs one bounded coordinator unit of producer-queue polling and sink dispatch. It distinguishes bounded progress, quiescence and failure outcomes instead of implying that a source accepted into RAM is durable.

## Resource, failure and lifecycle invariants

Caller owns queue and sink lifetimes and supplies deadline/capacity. Only the owning coordinated worker may retire or reclaim record leases.

## Source declarations and responsibilities

The following source-indexed inventory distinguishes defined types/contracts from operations. Descriptions reflect implementation documentation at the cited line; callers must also respect the preconditions, result meanings, provider choices and lifetimes described above.

- **CoordinatorQuantumStatus** (Type or Concept, [line 14](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/CoordinatorQuantum.hpp#L14)): One bounded, synchronous coordinator execution quantum, NOT a Thread. The application-composed EDP-Threading dedicated worker owns wake/stop/join.
- **CoordinatorQuantumOutcome** (Type or Concept, [line 26](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/CoordinatorQuantum.hpp#L26)): Exact per-sink submission and reclamation evidence from one bounded pass. No Accepted result here denotes Persistence durability or Serial receipt.
- **TQueue** (Type or Concept, [line 61](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/CoordinatorQuantum.hpp#L61)): The single quantum algorithm, selected with an explicit queue mutex policy.
- **TQueue** (Type or Concept, [line 132](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/CoordinatorQuantum.hpp#L132)): Original no-wait quantum; explicit bounded queue locking reuses the same implementation.

## Dependency and test qualification

The implementation owns no replacement EDP-Platform/Memory/Threading/Persistence abstraction. The actual include closure is determined by its opt-in header and the consuming application; the default Logging umbrella remains deliberately narrow. See the current [Dependency Graph](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/DEPENDENCY_GRAPH.MD), [Architecture](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ARCHITECTURE.MD), and [G1–G15 acceptance matrix](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ACCEPTANCE-G1-G15.MD) for exact validation and limitation claims. Never translate an unrelated historical fixture into current production proof.
