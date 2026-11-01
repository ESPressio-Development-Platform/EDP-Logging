# EdlgFrameReader — implementation reference

**Classification:** Public framing reader. **Owning header:** [src/logging/EdlgFrameReader.hpp](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/EdlgFrameReader.hpp). **Version:** 0.1.0, source branch feature/logging-v1. This page describes implementation and owner requirements; it does not replace normative ./docs contracts.

## Responsibility and boundaries

Examines an EDLG-framed byte span and yields a validated borrowed frame view. It rejects malformed size, incomplete suffix, incorrect CRC and invalid framing rather than exposing unqualified payload bytes.

## Resource, failure and lifecycle invariants

Returned views borrow the caller buffer. Framing/integrity success is not proof of native durable storage, nor of the semantics of the payload CBOR.

## Source declarations and responsibilities

The following source-indexed inventory distinguishes defined types/contracts from operations. Descriptions reflect implementation documentation at the cited line; callers must also respect the preconditions, result meanings, provider choices and lifetimes described above.

- **EdlgFrameReadResult** (Type or Concept, [line 13](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/EdlgFrameReader.hpp#L13)): Strict, non-resynchronising read-only inspection of one EDLG v1 frame.
- **EdlgFrameView** (Type or Concept, [line 26](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/EdlgFrameReader.hpp#L26)): Borrowed verified frame; source must outlive all consumers of the view.
- **ReadEdlgFrame** (Method or Function, [line 42](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/EdlgFrameReader.hpp#L42)): Inspect one complete frame at offset zero in caller-owned bytes.  A valid frame consumes exactly header+payload+completion, permitting a caller to iterate a contiguous prefix. Extra source bytes are untouched. No attempt is made to salvage records after corruption; archive rotation and forensic resync are sep

## Dependency and test qualification

The implementation owns no replacement EDP-Platform/Memory/Threading/Persistence abstraction. The actual include closure is determined by its opt-in header and the consuming application; the default Logging umbrella remains deliberately narrow. See the current [Dependency Graph](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/DEPENDENCY_GRAPH.MD), [Architecture](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ARCHITECTURE.MD), and [G1–G15 acceptance matrix](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ACCEPTANCE-G1-G15.MD) for exact validation and limitation claims. Never translate an unrelated historical fixture into current production proof.
