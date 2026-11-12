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

## 12 November 2026 — exact compatibility replay result vocabulary

**Exact source:** [`OwnedDiagnosticReplay.hpp`](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/8d2b307a8a1db1aa5596abc904cd0da1f3c032cd/src/logging/OwnedDiagnosticReplay.hpp). This optional **source-compatible convenience facade** forwards to the newer owner-qualified `OwnedDiagnosticPositionalReencoding` implementation; it is not an independent scalar decoder or unbounded replay queue.

`OwnedArgumentReplayStatus : uint8_t`: **Succeeded (0)** means complete positional bytes were encoded under the generated message schema; **CountMismatch (1)** means source-owned argument count differed from exact TValues pack; **SourceDescriptorRejected (2)** means a borrowed descriptor/span was missing, wrongly classified or outside owned capacity; **SourceScalarRejected (3)** means the exact typed owner conversion rejected the source value; **EncoderRejected (4)** means EDP-Serialisation canonical positional CBOR/JSON measure or encode refused the operation. Status names retain historical scalar terminology but apply to the full owner-qualified bounded UTF-8, Bytes and strong identity types. `OwnedArgumentReplayResult` stores `Status` (default CountMismatch) and `BytesWritten` (only authoritative upon Succeeded); `IsSuccessful() const` tests that exact status.

`EncodeOwnedDiagnosticArguments<TGeneratedMessageSchema,TCodec,TValues...,N,B,TByteOperationsProvider>(destination,source)` is the legacy signature: generated EDP-Localisation message schema and ordered `TValues...` are mandatory, `TCodec` must be precisely EDP-Serialisation Cbor or Json (compile-time assertion), `N,B` match the source argument descriptor/byte arena capacity and `TByteOperationsProvider` names its borrowed EDP-Memory byte-copy qualifier. It creates a **local fixed-size `OwnedPositionalScratch<TValues...>` stack value** and forwards to the canonical typed `EncodeOwnedDiagnosticArgumentsCbor` or `EncodeOwnedDiagnosticArgumentsJson` path, mapping all source failure states to the five result values. Nothing is persisted or copied into an independent unbounded record cache; large generated argument packs should instead use the explicit externally owned scratch overload to avoid unexpected Task stack use. This call is not ISR-qualified and must not infer text localisation or historical schema provenance from a mere fingerprint.
