# Owned diagnostic positions → canonical EDP-Serialisation

## Scope and owners

`logging/OwnedDiagnosticPositionalReencoding.hpp` is an **optional, synchronous application composition** adapter between two existing contracts: (1) `OwnedDiagnosticArguments<MaximumArguments, MaximumBytes>`, a bounded, privately native source snapshot whose descriptors and kind enum have **no public wire meaning**, and (2) the **generated** EDP-Localisation `DiagnosticMessageSchema` and existing `SerialisePositionalCbor/Json` implementation in EDP-Serialisation.

The generated message signature must be supplied at compile time together with its exact argument Types in position order. The bridge checks the source count, each descriptor's *private* kind, each stored byte width, and owner-level value validity. It reconstructs typed arguments into **caller-provided** `OwnedPositionalScratch<TValues...>` (a fixed tuple of optional typed values), then forwards those values to `EncodeDiagnosticArgumentsCbor/Json`. **Neither native argument bytes nor private kind/offset descriptors are serialized.** This does not add another CBOR encoder, numeric tag registry, dynamic value variant or unmanaged allocation.

## Resource and lifecycle contract

The caller retains the source arena, the output span, and all fixed scratch until the synchronous call completes. Scratch capacity and stack/heap location are explicitly chosen by Bootstrap; the bridge allocates no heap or task and always resets all optional values, even when count/kind/width, owner-value validation or destination/encoder admission fails. The source owner remains authoritative and can be reclaimed after all its borrowers complete; the successfully encoded output follows the EDP-Memory-owned downstream sink lease lifecycle. The output span is not a retained reference to the source. No asynchronous buffer sharing or durable Persistence acceptance is implied.

`OwnedPositionalReencodingOutcome` reports a typed `Status`, first failed parameter index for source validation, and the **unchanged** EDP-Serialisation `SerialisationResult` (including size/failure data). `CountMismatch` and `KindMismatch` reject a source not matching its generated schema. `SourceUnavailable`, `ValueWidthMismatch` and `OwnerValueRejected` reject malformed/badly typed source facts. `EncoderRejected` preserves a downstream destination/codec failure, without pretending any bytes were durably delivered.

Supported existing owner Types include exact-width scalar bool/integer/floating parameters; bounded EDP-BoundedTypes String/Bytes; the complete System TypeIdentifier, DeviceIdentifier, and FieldIdentifier; RuntimeIncarnationId (including a zero *diagnostic observation*); and optional EDP-Clock ClockEra (including zero *unavailable* observation). Bool requires one native byte of 0/1. Strings are rehydrated through EDP-BoundedTypes' own `Assign` contract (which prohibits embedded NUL); bytes preserve their exact width and binary contents; owner semantic identifiers preserve their Type validity. Unknown kinds, mismatched widths, invalid identities and unsupported semantic Types fail closed. A zero diagnostic fact does **not** make an emitting record root identity valid.

## Qualification and limits

On RPI400, generated Localisation fixtures exercise the source-to-typed-to-canonical path with (a) RuntimeIncarnationId and optional ClockEra, (b) bounded UTF-8, binary bytes, TypeIdentifier and FieldIdentifier, and (c) DeviceIdentifier, bool, u16, f32, f64. Host GCC and Clang tests check identical CBOR and JSON outputs before/after original caller inputs are modified, empty scratch on every path, strict kind/count/size, transactional source capacity failure, destination rejection, compile-time schema mismatches, and zero diagnostic sentinels. The separate UBSan runs use both compilers. The root fifteen-field LogRecord Typed Envelope is **not yet emitted** by this adapter, and it is not a substitute for physical WROOM, G1/G10 queue/sink-lifetime or EDLS PowerLoss tests.

## 2026-10-23 — G3/G5 generated owner replay runner repair

The generated Localisation diagnostic integration runner previously
referenced `tests/OwnedDiagnosticReplayTests.cpp`, which did **not**
exist in the checked-in source. This meant one required build/test
surface failed before exercising the source-compatible
`logging/OwnedDiagnosticReplay.hpp` facade. The runner also compiled,
but did not execute, its heterogeneous generated DeviceIdentifier /
bool / u16 / f32 / f64 source-to-CBOR/JSON rehydration test.

The newly checked-in `OwnedDiagnosticReplayTests.cpp` uses the real
EDP-Localisation fixture's generated message schema (1/0/3), stores
owned u16/bool arguments, and verifies canonical CBOR and JSON
positional output, count mismatch, descriptor-kind mismatch and
bounded encoder refusal through the public compatibility facade.
The runner now **executes** both this replay test and the existing
heterogeneous generated owner-reencoding test, in addition to its
existing direct generated-contract, strong Clock/System arguments,
and compile-fail width-mismatch checks.

These tests verify owner-qualified source rehydration and canonical
representation only. They do **not** issue new wire field identities,
change D1–D13, establish historical schema authenticity, or prove
product Persistence durability. All versions stay 0.1.0.
