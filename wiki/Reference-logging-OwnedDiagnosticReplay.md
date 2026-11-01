# OwnedDiagnosticReplay — implementation reference

**Classification:** Internal source-compatible replay facade. **Owning header:** [src/logging/OwnedDiagnosticReplay.hpp](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/OwnedDiagnosticReplay.hpp). **Version:** 0.1.0, source branch feature/logging-v1. This page describes implementation and owner requirements; it does not replace normative ./docs contracts.

## Responsibility and boundaries

Delegates historical scalar-replay entry points to the existing exact generated-schema positional rehydration owner. It creates finite caller-signature-sized temporary scratch and maps detailed source/encoder outcomes into a typed replay result.

## Resource, failure and lifecycle invariants

The compatibility facade is not an alternate CBOR/JSON encoder; generated schema types, position counts, source kinds and widths are checked before canonical Serialisation output. Application-owned explicit scratch API is preferred for production memory budgeting.

## Source declarations and responsibilities

The following source-indexed inventory distinguishes defined types/contracts from operations. Descriptions reflect implementation documentation at the cited line; callers must also respect the preconditions, result meanings, provider choices and lifetimes described above.

- **OwnedArgumentReplayStatus** (Type or Concept, [line 14](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/OwnedDiagnosticReplay.hpp#L14)): Defines a named type/contract within OwnedDiagnosticReplay; read its source for exact conditional semantics and provider requirements.
- **OwnedArgumentReplayResult** (Type or Concept, [line 22](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/OwnedDiagnosticReplay.hpp#L22)): Defines a named type/contract within OwnedDiagnosticReplay; read its source for exact conditional semantics and provider requirements.
- **IsSuccessful** (Method or Function, [line 25](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/OwnedDiagnosticReplay.hpp#L25)): Defines a named operation within OwnedDiagnosticReplay; read its source for exact conditional semantics and provider requirements.
- **TGeneratedMessageSchema** (Type or Concept, [line 34](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/OwnedDiagnosticReplay.hpp#L34)): Defines a named type/contract within OwnedDiagnosticReplay; read its source for exact conditional semantics and provider requirements.
- **TByteOperationsProvider** (Type or Concept, [line 36](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/OwnedDiagnosticReplay.hpp#L36)): Defines a named type/contract within OwnedDiagnosticReplay; read its source for exact conditional semantics and provider requirements.
- **EncodeOwnedDiagnosticArguments** (Method or Function, [line 38](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/OwnedDiagnosticReplay.hpp#L38)): Defines a named operation within OwnedDiagnosticReplay; read its source for exact conditional semantics and provider requirements.

## Dependency and test qualification

The implementation owns no replacement EDP-Platform/Memory/Threading/Persistence abstraction. The actual include closure is determined by its opt-in header and the consuming application; the default Logging umbrella remains deliberately narrow. See the current [Dependency Graph](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/DEPENDENCY_GRAPH.MD), [Architecture](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ARCHITECTURE.MD), and [G1–G15 acceptance matrix](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ACCEPTANCE-G1-G15.MD) for exact validation and limitation claims. Never translate an unrelated historical fixture into current production proof.
