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
or a complete canonical 15-field Typed Envelope. Strong/bounded
arguments beyond exact arithmetic types are currently unsupported
here, even where the independent positional G3 encoder has them.

Documentation: [LogRecord Source Assembly](../docs/LOG-RECORD-SOURCE-ASSEMBLY.MD).
Validation: `tests/run-diagnostic-generated-contract-tests.sh`,
real generated Localisation schema, GCC and Clang.
