# Reference — logging/LogRecordSourceAssembly.hpp

**Public opt-in API:** `AssembleLogRecordSource<GeneratedMessageSchema>(record, severity, device, incarnation, attempt,
qualifiedClock, args...)` returns `LogRecordAssemblyStatus`.

The generated message signature itself owns the Domain/SubDomain/String
identity and strict count/ordered scalar-kind qualification. The
generated family contract owns the independent 16-byte diagnostic
schema fingerprint. Caller-supplied Clock, System identity, attempt
ticket and scalar args are staged into existing finite record storage.
The routine does not read Clock, allocate, block, publish to a queue,
run a worker or use a custom CBOR/JSON encoder.

**Source-side outcomes:** Prepared, OutputOccupied,
InvalidTemporalEvidence, InvalidSourceRecord, ArgumentCountExceeded,
ArgumentBytesExceeded and InvalidOwnedArgument. All failures preserve
the destination; no external argument storage is retained.

**Trust and wire exclusions:** Clock samples must already be genuinely
coherent; a qualified sample object does not confer authentication.
Unissued Field 11/14 bit layouts are zero-only source placeholders.
This is structural source assembly, NOT a Serialisation::SerialisableType
or a complete canonical 15-field Typed Envelope. Bounded text/bytes and System TypeIdentifier, FieldIdentifier and
DeviceIdentifier use exact native-kind owned source capture. Other
strong types such as ClockEra and RuntimeIncarnationId remain
unsupported here until their source storage is owner-certified.

Documentation: [LogRecord Source Assembly](../docs/LOG-RECORD-SOURCE-ASSEMBLY.MD).
Validation: `tests/run-diagnostic-generated-contract-tests.sh`,
real generated Localisation schema, GCC and Clang.

## Owner-certified strong positional argument source (2026-10-22)

`AssembleLogRecordSource` now admits an exact generated Localisation schema position of System RuntimeIncarnationId or opted-in ClockEra, capturing their native values into source-owned fixed capacity. The source record remains unchanged after a count/byte failure. Root incarnation and Era validation is separate from the possibility of recording zero sentinels as diagnostic facts. Neither native storage nor `OwnedArgumentKind` numbers have a published wire meaning; the typed EDP-Serialisation positional encoders own CBOR and JSON. Native GCC/Clang generated-schema tests and compile-fail contract tests exercise these distinctions; complete live emitter-to-archive re-encoding remains unqualified.
