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
Field 11 carries the issued Clock projection and quality values. Field 14 is zero for this version's reserved record facts.
This is structural source assembly, NOT a Serialisation::SerialisableType
or a complete canonical 15-field Typed Envelope. Bounded text/bytes and System TypeIdentifier, FieldIdentifier and
DeviceIdentifier use exact native-kind owned source capture. The owner-certified `RuntimeIncarnationId` and explicitly selected `ClockEra` are **supported**; other unissued strong Types remain rejected at compilation.

Documentation: [LogRecord Source Assembly](../docs/LOG-RECORD-SOURCE-ASSEMBLY.MD).
Validation: `tests/run-diagnostic-generated-contract-tests.sh`,
real generated Localisation schema, GCC and Clang.

## Owner-certified strong positional argument source (2026-10-22)

`AssembleLogRecordSource` now admits an exact generated Localisation schema position of System RuntimeIncarnationId or opted-in ClockEra, capturing their native values into source-owned fixed capacity. The source record remains unchanged after a count/byte failure. Root incarnation and Era validation is separate from the possibility of recording zero sentinels as diagnostic facts. Neither native storage nor `OwnedArgumentKind` numbers have a published wire meaning; the typed EDP-Serialisation positional encoders own CBOR and JSON. Native GCC/Clang generated-schema tests and compile-fail contract tests exercise these distinctions; complete generated-record-to-archive re-encoding is separately qualified by the later bounded WROOM managed-worker/strict ACK and test source profiles, rather than by this source assembly helper alone.

## 12 November 2026 — exact output status and complete source capture semantics

**Exact reviewed header:** [`LogRecordSourceAssembly.hpp`](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/8d2b307a8a1db1aa5596abc904cd0da1f3c032cd/src/logging/LogRecordSourceAssembly.hpp). The source is an **optional PUBLIC pre-admission** helper with **INTERNAL** source capture traits, importing sibling Logging diagnostic schema, BasicLogRecord and qualified temporal evidence. No native EDP-Memory pool, Threading task, Clock reading, codec implementation, queue or filesystem is allocated or owned.

**`LogRecordAssemblyStatus : uint8_t`** specifies: **Prepared (0)** means one complete schema-checked, caller-owned structural BasicLogRecord was built; **OutputOccupied (1)** rejects a destination whose attempt, device or owned arguments already contain a live value; **InvalidTemporalEvidence (2)** rejects internally inconsistent synchronized/holdover/never-synchronized Clock facts; **InvalidSourceRecord (3)** rejects completed candidate source field validation; **ArgumentCountExceeded (4)** and **ArgumentBytesExceeded (5)** are distinct finite arena constraints; **InvalidOwnedArgument (6)** preserves the exact failure of unsupported or malformed source types/UTF-8. All failures keep `output` unchanged; Prepared itself is **not** queue admission, serialization or persistent acknowledgement.

**INTERNAL `Detail::IsOwnedSourceCaptureType<TValue>() consteval`** approves exact-width primitive scalars, EDP-BoundedTypes String/Bytes and the existing owner-issued strong EDP-System Type/Field/Device/RuntimeIncarnation and optional exact EDP-Clock ClockEra, rejecting arbitrary coercions. `TValue` is the concrete generated argument Type, not a dynamic tag. **`Detail::CaptureOwnedSourceArgument<TOwned,TValue>(owned,value)`** binds **`TOwned`** to the caller's exact finite `OwnedDiagnosticArguments` arena and **`TValue`** to the exact source value, then chooses `AddUtf8`, `AddBytes`, `AddTypeIdentifier`, `AddFieldIdentifier`, `AddDeviceIdentifier`, `AddRuntimeIncarnationId`, `AddClockEra` or `AddScalar` under the owner-certified kind. It retains no source borrows after synchronous copying, does not allocate, and returns `OwnedArgumentResult` rather than generic Boolean.

**PUBLIC `AssembleLogRecordSource<TMessageSchema,TWidth,TMaximumArguments,TArgumentBytes,TValues...>`** takes the generated EDP-Localisation exact `TMessageSchema` (issued Domain/SubDomain/String, declared ordered argument kinds and 16-byte diagnostic fingerprint), strong attempt width `TWidth`, fixed source argument count and byte arena capacities, and exact Types `TValues...`. Runtime arguments are the caller's initially empty **`output`** BasicLogRecord, emitted `severity`, issued `device`, `incarnation`, nonzero per-incarnation `attempt`, one **previously coherent captured** `temporal` EDP-Clock sample and borrowed source values. Static assertions enforce the generated message signature, parameter count/kinds and recognized exact-width Types before code runs.

The operation first refuses occupied output and incoherent/contradictory Clock facts. It stages a complete `BasicLogRecord` in **caller stack scratch**, copies generated message/fingerprint, severity/Device/Incarnation/Attempt and Clock coordinate/quality from the same qualified sample, records disabled/unavailable synchronization conservatively and sets owner-reserved facts to zero. Each argument is copied into the new candidate's bounded arena; **only after every capture and `ValidateLogRecord(candidate)` succeed does it assign the completed semantic record to the caller's output**. The caller owns synchronization for that destination. Any count/byte/UTF8/identity failure leaves the destination unchanged. A zero ClockEra in a diagnostic parameter can be a valid observation while a zero root era/invalid source identity remains a different field-level rejection. Field13 private native source-kinds are not EDP-Serialisation's CBOR tags, Field11 Clock quality is never invented, and full typed wire/durable archiving belongs to subsequent generated projection + strict ACK worker owners.

**Resource and lifecycle:** this is a synchronous ordinary-context stage with fixed stack candidate proportional to `TMaximumArguments`/`TArgumentBytes`, no hidden Heap, Task, mutex or dynamic queue; Bootstrap must provision sufficient WROOM task stack and ensure the issuing Clock/identity providers outlive capture. It is not ISR-qualified or itself a complete error-free producer pipeline. GCC/Clang generated contract positives, compile-fail mismatches and whole-record native worker cases are separately evidenced in [source assembly](../docs/LOG-RECORD-SOURCE-ASSEMBLY.MD), [record production](Reference-logging-RecordProduction) and [G1–G15 matrix](../docs/ACCEPTANCE-G1-G15.MD).
