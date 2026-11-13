# G3 governed diagnostic argument encoding

The optional [diagnostic argument adapter](../src/logging/DiagnosticArgumentEncoding.hpp)
uses the EDP-Localisation-generated `DiagnosticMessageSchema<D,S,I>`.
`IsDiagnosticSchemaMatching<Schema, Types...>()` checks declaration, argument
count and exact kind order. A C++ type outside the supported set fails at
compile time. The supported set includes fixed-width scalars, [bounded strings,
bytes and strong identifiers](DIAGNOSTIC-BOUNDED-ARGUMENTS.MD), and ClockEra
when the caller imports the optional ClockEra adapter.

`EncodeDiagnosticArgumentsCbor<Schema>` and `EncodeDiagnosticArgumentsJson<Schema>`
require a matching schema and owner-certified EDP-Serialisation support for
every value. They encode the Field13 positional value into a caller-owned
span and return the owner result. The [generated wire projection](../wiki/Reference-logging-GeneratedLogRecordWireProjection.md)
covers the full typed LogRecord. The optional argument adapter adds an
EDP-Serialisation dependency.

`tests/run-diagnostic-argument-tests.sh` covers a generated-descriptor-shaped
fixture and golden codec output. `tests/run-diagnostic-generated-contract-tests.sh`
uses the actual EDP-Localisation generator and covers successful and
compile-time-rejected signatures, both codecs and rehydration from owned
source arguments. These tests use fixture catalogues.

## 2026-10-22 — re-encoding already-owned heterogeneous source values

The schema-qualified scalar/strong writer also supports **reconstructed** values from `OwnedDiagnosticArguments` via [Owned Diagnostic Positional Re-encoding](./OWNED-DIAGNOSTIC-POSITIONAL-REENCODING.MD). The generated message's exact parameter Types select each private source descriptor and owner conversion; private Kind/Offset/Size bytes are checked but never emitted as wire discriminators. Caller-owned `OwnedPositionalScratch<...>` is completely cleared even when the source count/kind/width is wrong or the canonical writer rejects an undersized destination. The writer remains EDP-Serialisation-owned, and no type identifier or wire tag is newly allocated.


## 1 November 2026 - strict UTF-8 source consistency

The optional OwnedDiagnosticArguments::AddUtf8 uses the sole EDP-Serialisation strict ValidateUtf8 implementation. It rejects embedded U+0000 before committing an argument slot, preserving count/bytes on refusal and retaining previous canonical CBOR/JSON semantics and wire bytes. Arbitrary NUL-containing data remains available using AddBytes only where the generated diagnostic schema declares byte-valued parameters.
