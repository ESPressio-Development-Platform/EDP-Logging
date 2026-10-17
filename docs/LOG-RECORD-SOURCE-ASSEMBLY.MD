# LogRecord Source Assembly — generated-schema-bound G3/G4 prerequisite

**Status:** optional partial Logging feature on `feature/logging-v1`,
version `0.1.0`, NOT a serialisable, complete LogRecord pipeline.

## Contract and authority

`logging/LogRecordSourceAssembly.hpp` prepares a source-owned
`BasicLogRecord<Width,MaxArgs,Bytes>` on the calling thread using
the authoritative **EDP-Localisation generated**
`DiagnosticMessageSchema<Domain,SubDomain,String>` and
`DiagnosticMessageSchema::OwningContract::DiagnosticSchemaFingerprint`,
so the message and its fingerprint cannot be selected from different
generated catalogue families.
Its compile-time generated `SourceDomain`, `SourceSubDomain` and
`SourceString` constants bind Field 0 to exactly the message whose
parameter signature was certified. The full 128-bit diagnostic
fingerprint (Field 12) comes from the Localisation contract, not
from translated text and not from Logging's sequence-width profile.

The caller must already have sampled one truthful coherent
`QualifiedLogTime` using EDP-Clock/Bootstrap Composition, issued its
one per-incarnation `AttemptSequence` under external producer
sequencing, and selected valid EDP-System DeviceIdentifier and
RuntimeIncarnationId. Assembly is performed **before queue/Memory
admission waiting**. No new Clock capture, Mesh requirement, mutex,
allocator, formatter or wire serializer is introduced.

## Atomic owned capture and failure outcomes

The initial supported argument profile deliberately accepts only
exact-width bool, signed/unsigned 8/16/32/64-bit integers, float and
double with *exact C++20 compile-time agreement* to the generated
`DiagnosticParameterKind` array and matching arity. A schema
declared as absent, mismatched positional kind, excessive argument
count, or unsupported strong/bounded parameter type is rejected at
compile time, not implicitly narrowed or reinterpreted. Arbitrary
generated parameter types (including bounded UTF-8/Bytes) are
**not yet supported by this particular source assembler**, even
where a separate G3 positional encoder already supports them.

The caller-owned destination must be unoccupied. Construction uses
a complete candidate in independent bounded stack storage. Each
argument is copied once using `OwnedDiagnosticArguments::AddScalar`
(the existing Logging owned-capture facility), and `ValidateLogRecord`
runs before the destination is assigned. Invalid input
(`InvalidTemporalEvidence`, `InvalidSourceRecord`), an occupied
destination, or finite argument byte exhaustion leaves destination
unchanged. Successful `Prepared` means only a structurally valid,
fully owned source record. No heap allocation or external argument
borrow survives the call. Stack capacity scales with the configured
record capacity and requires measured application stack budgeting.

A legal monotonic-only sample preserves canonical Monotonic, records
no synchronized coordinate or Era and saturates unknown uncertainty.
Valid zero synchronized time remains distinguishable by `std::optional`.
Contradictory temporal statuses, a supposedly absent synchronized
coordinate with a nonzero ClockEra or falsely precise uncertainty,
cannot be silently accepted.

## Explicitly not implemented

Fields 11 and 14 remain **unissued bit sub-vocabularies**. Both
`OpaqueSourceFacts` placeholders remain zero-only in memory, never
historical wire semantics. No canonical CBOR/JSON LogRecord typed
envelope is emitted, no signed historical schema provenance or
trusted identity is asserted, and no runtime sink, worker or archive
is activated by this function. The existing G3 positional encoder
remains EDP-Serialisation-owned; future LogRecord representation
must use that owner rather than hand-coded CBOR or JSON.

## Validation

`tests/run-diagnostic-generated-contract-tests.sh` regenerates a
real EDP-Localisation fixture schema from authoring JSON, builds
`DiagnosticArgumentGeneratedContractTests.cpp` with real Clock,
System, Localisation, Serialisation and Logging owner headers,
and exercises valid owned record, generated numeric message,
compile-time fingerprint equality, monotonic-only evidence,
occupied output, malformed temporal evidence, invalid source
identity and insufficient argument bytes. Both GCC and Clang
are required. The script also preserves its existing negative
compile-fail test for a mismatched generated positional schema.
A successful host test does not qualify full physical G4 wire
serialization or true coherent Clock publisher.
