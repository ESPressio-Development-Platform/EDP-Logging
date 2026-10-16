# logging/DiagnosticArgumentEncoding.hpp — G3 scalar diagnostic argument adapter

**Status:** optional, partially qualified. Requires EDP-Serialisation `feature/logging-v1` positional codecs and compile-time generated EDP-Localisation DiagnosticMessageSchema descriptor. No Logging default umbrella import or Serialisation reverse edge. Package version `0.1.0`.

## Public/internal declarations

- `Detail::ScalarDiagnosticKind<Kind,TValue>() consteval`: private compile-time mapper from exact C++ bool/int8/int16/int32/int64/uint8/uint16/uint32/uint64/float/double to the enum **supplied by the EDP-Localisation-generated schema Type**. Does not define a Logging enum. Refuses unsupported values with compile-time error.
- `ScalarDiagnosticSchemaMatches<TGeneratedMessageSchema,TValues...>() consteval`: compares `Declared`, `ArgumentCount` and exact ordered `ArgumentKinds` against compile-time scalar pack. The descriptor is the EDP-Localisation `DiagnosticMessageSchema<D,S,I>` source-generated specialization. False on absent schema, incorrect arity or wrong type width/order.
- `EncodeDiagnosticArgumentsCbor<TGeneratedMessageSchema>(std::span<std::byte>,values...) noexcept`: `static_assert` exact signature and forward to EDP-Serialisation's `SerialisePositionalCbor`; returns its typed result including exact RequiredBytes/BytesWritten.
- `EncodeDiagnosticArgumentsJson<TGeneratedMessageSchema>(std::span<std::byte>,values...) noexcept`: matching canonical JSON positional array using `SerialisePositionalJson` on the exact same governed scalar signature.

No new clock, dynamic allocation, runtime table, language translation or raw byte encoder. The result is only **positional argument bytes**; it is **not** a D13 LogRecord Typed Envelope. Caller source must remain stable through Serialisation's preflight+write traversal.

## Gaps and host evidence

Generated non-scalar UTF-8, Bytes and strong identifier kinds still need owner-aligned semantic C++ adapters. No production LogRecord TypeIdentifier, argument Field13 container / schema fingerprint envelope, full Localisation generator integration, HIL or physical sink. Native GCC+Clang tests against a descriptor-shape fixture passed RPI400 detached run `20261016T075420-4cf97c7e`. The additional owner compiler integration script `tests/run-diagnostic-generated-contract-tests.sh` builds a **real** EDP-Localisation-generated fixture and compiles its governed `DiagnosticMessageSchema<1,0,3>` into the Logging/Serialisation adapter. Real-generated C++/CBOR/JSON interoperability first passed GCC+Clang RPI400 detached run `20261016T075614-ff87d5fb`. This still does not establish the application production catalogue or complete LogRecord envelope. See [G3 implementation notes](../docs/DIAGNOSTIC-ARGUMENT-ENCODING.MD), [Dependency Graph](../DEPENDENCY_GRAPH.MD).
