# GeneratedLogRecordEdlsArchive — implementation reference

**Classification:** Public optional governed-record archival adapter. **Owning header:** [src/logging/GeneratedLogRecordEdlsArchive.hpp](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/GeneratedLogRecordEdlsArchive.hpp). **Version:** 0.1.0, source branch feature/logging-v1. This page describes implementation and owner requirements; it does not replace normative ./docs contracts.

## Responsibility and boundaries

Emits the generated-schema-qualified fifteen-field record through EDP-Serialisation canonical CBOR and Logging framing into the existing immutable EDLS successor/rolling protocol. The original source coordinate and issued root TypeIdentifier are preserved.

## Resource, failure and lifecycle invariants

Do not serialize the private native owned-argument arena as a wire record. A failed conversion, full slot, or uncertain publication must remain a typed refusal and must not advance a durable acknowledgement.

## Source declarations and responsibilities

The following source-indexed inventory distinguishes defined types/contracts from operations. Descriptions reflect implementation documentation at the cited line; callers must also respect the preconditions, result meanings, provider choices and lifetimes described above.

- **GeneratedEdlsPublishStatus** (Type or Concept, [line 13](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/GeneratedLogRecordEdlsArchive.hpp#L13)): Defines a named type/contract within GeneratedLogRecordEdlsArchive; read its source for exact conditional semantics and provider requirements.
- **GeneratedEdlsPublishOutcome** (Type or Concept, [line 24](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/GeneratedLogRecordEdlsArchive.hpp#L24)): Defines a named type/contract within GeneratedLogRecordEdlsArchive; read its source for exact conditional semantics and provider requirements.
- **IsPublished** (Method or Function, [line 30](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/GeneratedLogRecordEdlsArchive.hpp#L30)): Defines a named operation within GeneratedLogRecordEdlsArchive; read its source for exact conditional semantics and provider requirements.
- **TMessageSchema** (Type or Concept, [line 40](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/GeneratedLogRecordEdlsArchive.hpp#L40)): Called only from an application-composed worker with exclusive archive ownership. All scratch is caller-supplied and capacity-bounded. A result of Published means complete EDLS/EDLG readback and predecessor checks, not blanket atomicity of the underlying FileStorage device.
- **TStorage** (Type or Concept, [line 45](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/GeneratedLogRecordEdlsArchive.hpp#L45)): Defines a named type/contract within GeneratedLogRecordEdlsArchive; read its source for exact conditional semantics and provider requirements.
- **TMessageSchema** (Type or Concept, [line 139](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/GeneratedLogRecordEdlsArchive.hpp#L139)): The old host API is retained, using stack scratch. ESP32 workers with high payload bounds must instead call WithScratch and supply one Bootstrap-owned scratch separate from their EDLG frame buffer. Once framing completes, the former CBOR buffer may be reused as archive scratch if it has the full EdlsHeaderByt
- **TStorage** (Type or Concept, [line 144](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/GeneratedLogRecordEdlsArchive.hpp#L144)): Defines a named type/contract within GeneratedLogRecordEdlsArchive; read its source for exact conditional semantics and provider requirements.

## Dependency and test qualification

The implementation owns no replacement EDP-Platform/Memory/Threading/Persistence abstraction. The actual include closure is determined by its opt-in header and the consuming application; the default Logging umbrella remains deliberately narrow. See the current [Dependency Graph](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/DEPENDENCY_GRAPH.MD), [Architecture](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ARCHITECTURE.MD), and [G1–G15 acceptance matrix](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ACCEPTANCE-G1-G15.MD) for exact validation and limitation claims. Never translate an unrelated historical fixture into current production proof.
