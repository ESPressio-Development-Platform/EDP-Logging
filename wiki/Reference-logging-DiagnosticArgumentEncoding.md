# Logging DiagnosticArgumentEncoding — bounded governed arguments

**Opt-in public G3 extension**, version `0.1.0`. `logging/DiagnosticArgumentEncoding.hpp` consumes actual **EDP-Localisation generated** `DiagnosticMessageSchema`, **EDP-Serialisation** canonical positional CBOR/JSON and **EDP-BoundedTypes** values.

`DiagnosticSchemaMatches<Schema,ActualTypes...>()` compiles only where the generated message declares the exact positional kinds. Currently supported values: all prior fixed-width scalars, compile-time `Bounded::String<N,Provider>` for `Utf8`, `Bounded::Bytes<N,Provider>` for `Bytes`, owner-certified `System::TypeIdentifier` and `System::FieldIdentifier`. The original `ScalarDiagnosticSchemaMatches` alias is retained. `EncodeDiagnosticArgumentsCbor/Json` enforce the same generated contracts and require actual EDP-Serialisation `SerialisableType`, returning typed results and preflight buffer correctness.

**No guessed conversions** for Localisation StringIdentifier, DeviceIdentifier, RuntimeIncarnationId, ClockEra or bespoke enums. No governed G4 LogRecord root TypeIdentifier, typed envelope, archive, transport or durability is created by this adapter. See [G3 bounded arguments](../docs/DIAGNOSTIC-BOUNDED-ARGUMENTS.MD), [Dependencies](../DEPENDENCY_GRAPH.MD).

## Exact generated strong owner types (2026-10-22)

The generated signature now accepts `RuntimeIncarnationId` and explicitly certified ClockEra without casting either to primitive integer call-site arguments. `EncodeDiagnosticArgumentsCbor` and `EncodeDiagnosticArgumentsJson` delegate to EDP-Serialisation, enforcing the generated Localisation argument-kind list at compile time. Source capture uses a separate bounded native arena whose descriptors must never be treated as wire tag numbers. Real owner-generated fixtures with both strong values, golden CBOR/JSON positionals and GCC/Clang negative type mismatch tests pass. No current full 15-field LogRecord wire result is inferred.
