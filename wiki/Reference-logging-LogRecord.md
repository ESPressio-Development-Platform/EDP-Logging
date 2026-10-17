# LogRecord — permanent System semantic type identity (G4)

Opt-in `logging/LogRecord.hpp` declares a direct immutable compile-time
`System::TypeIdentifier`:
`00 00 01 00 00 00 00 01`.
The 24-bit Authority `0x000001` belongs to ESPressio Development
Platform Core; the 40-bit local value `0x0000000001` belongs
permanently and exclusively to `Logging::LogRecord`. This allocation
was authorised 2026-10-17 and published in the owner
[System registry](https://github.com/ESPressio-Development-Platform/EDP-System/blob/main/docs/TYPE-IDENTITY-REGISTRY.MD).

`System::IdentifiedType<Logging::LogRecord>` is true, and
`System::TypeIdentifierOf<Logging::LogRecord>` reads identical
eight-byte storage. The actual `LogRecord` now supplies 15
compile-time `System::FieldBinding` members, one per approved
FieldId 0..14; `System::SchemaType<Logging::LogRecord>` is true.
`BasicLogRecord<AttemptSequenceWidth,MaxArguments,ArgumentBytes>`
selects a 16/32/64-bit attempt width and bounded owned in-memory
arguments without changing semantic Type identity. Field 11 temporal
provenance and Field 14 facts are still **opaque placeholders** and have
no issued wire bit layout. The record is NOT YET an owner-certified
`Serialisation::SerialisableType`, typed CBOR/JSON envelope,
complete historical schema, or production worker/sink lifecycle.

Identity persistence is independent of library version (all remain
0.1.0), message StringIdentifier, transport or Mesh participation.

## Fail-closed pre-wire validation (2026-10-17)

`ValidateLogRecord` now rejects nonzero source-only Field 11 or Field 14
opaque placeholders with `UnapprovedTemporalProvenance` or
`UnapprovedRecordFacts`, rather than silently assigning them persistent
wire meanings. In the absence of a synchronized coordinate, the
`EDP-Clock::SynchronizationUncertainty` must remain saturated/unknown;
zero uncertainty falsely implies precision and is rejected as
`InvalidSyncEvidence`. Legal monotonic-only records remain supported.
These checks do not allocate an on-wire vocabulary, provide a coherent
upstream Clock capture, or make the root `SerialisableType`.
The host `LogRecordIdentityTests` covers the new failure outcomes.
