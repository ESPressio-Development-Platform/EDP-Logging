# ESPressio_Logging — implementation reference

**Classification:** Public entry point. **Owning header:** [src/ESPressio_Logging.hpp](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/ESPressio_Logging.hpp). **Version:** 0.1.0, source branch feature/logging-v1. This page describes implementation and owner requirements; it does not replace normative ./docs contracts.

## Responsibility and boundaries

The deliberately lightweight C++20 foundation umbrella exposes severity selection, attempt sequence issuance, EDLG framing/reading, EDLS segment framing, and instrumentation without importing an allocator, runtime, radio, filesystem, or native SDK. Opt-in coordinator, generated codecs, and persistence are composed by explicit downstream includes.

## Resource, failure and lifecycle invariants

The default include must not pull EDP-Memory, Threading, Serialisation, or a target SDK; compilation should succeed on a bare C++20 toolchain.

## Source declarations and responsibilities

The following source-indexed inventory distinguishes defined types/contracts from operations. Descriptions reflect implementation documentation at the cited line; callers must also respect the preconditions, result meanings, provider choices and lifetimes described above.

This header is an aggregate of explicit includes, not an independently instantiated type or runtime.

## Dependency and test qualification

The implementation owns no replacement EDP-Platform/Memory/Threading/Persistence abstraction. The actual include closure is determined by its opt-in header and the consuming application; the default Logging umbrella remains deliberately narrow. See the current [Dependency Graph](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/DEPENDENCY_GRAPH.MD), [Architecture](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ARCHITECTURE.MD), and [G1–G15 acceptance matrix](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ACCEPTANCE-G1-G15.MD) for exact validation and limitation claims. Never translate an unrelated historical fixture into current production proof.
