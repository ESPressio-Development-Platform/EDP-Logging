# G3: generated bounded UTF-8, Bytes and System Identifier argument encoding

**Package 0.1.0**, opt-in `logging/DiagnosticArgumentEncoding.hpp`, branch `feature/logging-v1`. No complete LogRecord Typed Envelope/TypeIdentifier is claimed. Existing scalar call signatures are backward compatible.

The authoritative **EDP-Localisation generator** already declares canonical `DiagnosticParameterKind::{Utf8,Bytes,TypeIdentifier,FieldIdentifier}`, and emits each governed message's exact `DiagnosticMessageSchema<D,S,I>::ArgumentKinds`. The EDP-Logging adapter now introduces `DiagnosticSchemaMatches<TGeneratedMessageSchema,TValues...>()`, a C++20 compile-time check of the generated numeric identity, argument count, exact kind, positional order and explicit opt-in `Declared`. The pre-existing `ScalarDiagnosticSchemaMatches` remains as a source-compatible delegating alias. The mapping extends the exact old fixed-width scalar support to these **owner-certified** C++ types:

| Generated kind | Permitted application value |
|---|---|
| `Utf8` | `EDP-BoundedTypes::Bounded::String<compile-time capacity, byte operations provider>` |
| `Bytes` | `EDP-BoundedTypes::Bounded::Bytes<compile-time capacity, byte operations provider>` |
| `TypeIdentifier` | `EDP-System::TypeIdentifier` (canonical exact 8-byte storage, already certified by EDP-Serialisation) |
| `FieldIdentifier` | `EDP-System::FieldIdentifier` (0..255 one-byte identity, already certified by EDP-Serialisation) |

The Logging layer **does not** implement any String encoding, JSON escaping, CBOR major type, byte copy, strong identity conversion or runtime variant: these remain the responsibility of **existing EDP-Serialisation** `SerialisePositionalCbor/Json` and `SerialisableType` (with EDP-BoundedTypes provider operations). Both encoder functions preserve strict `static_assert` of governed argument kinds AND `SerialisableType`, pre-measure source validity/buffer capacity through the underlying owner, and return the exact underlying typed result. A buffer shorter than the measured encoded sequence remains entirely unchanged.

**Important unimplemented kinds:** `Localisation::StringIdentifier`, `System::Identity::DeviceIdentifier`, `System::Identity::RuntimeIncarnationId`, and `Clock::ClockEra` have distinct current owner Types, but are **not** automatically coerced to scalar or raw bytes. Their owner-certified reversible `EDP-Serialisation::CanonicalRepresentation` adapter and/or application ContractFamily width specification must be established first. C++ custom enums/strong types likewise require explicit governed kind-to-serialiser mapping; no guessed IDs/casts or transient type strings may be encoded as production records. The governed root `LogRecord::TypeIdentifier` remains BLOCKED under EDP-System#11 (do NOT invent one). The bounded diagnostic array itself is **Field13 only**, never a complete typed log record.

**Tests:** `tests/run-diagnostic-bounded-generated-tests.sh` uses the ACTUAL Localisation Python semantic compiler and generated C++ descriptor, types `Utf8,Bytes,TypeIdentifier,FieldIdentifier`; `DiagnosticBoundedGeneratedContractTests.cpp` verifies exact static argument matching, swapped kinds/arity rejection, correct CBOR/JSON encoding using genuine EDP-Serialisation owner, and transactional undersized-buffer refusal. `CompileFailDiagnosticBoundedWrongKind.cpp` must fail to compile at the Logging-generated-schema `static_assert` (not for missing include). `DiagnosticBoundedXtensaSmoke.cpp` instantiates the same bounded/strong template path under Xtensa ESP32 compile-only. Existing scalar generated-contract tests, G1/G2 and full G3 regressions are mandatory. No physical target/Flash/serial delivery is proven.

## Exact source-owned FieldIdentifier (2026-10-17)

The PRIVATE `OwnedDiagnosticArguments` capture arena now supports
`AddFieldIdentifier(System::FieldIdentifier)`, adding one exact byte
under `OwnedArgumentKind::FieldIdentifier = 15`. This is **not**
a public on-wire tag allocation; zero is a valid FieldIdentifier,
and source-backed typed FieldIdentifier can never be misclassified as
a generic UInt8 argument. Its new kind is intentionally appended
rather than altering existing internal kind values. The generated
schema and EDP-Serialisation remain the authorities for on-wire
FieldIdentifier semantics. No new byte allocator or copy facility.
The opt-in `LogRecordSourceAssembly.hpp` also reuses the existing
bounded UTF-8/Bytes and strong TypeIdentifier/DeviceIdentifier
source-capture methods with exact generated-kind validation.

Regression: generated four-parameter bounded/strong fixture in
`tests/run-diagnostic-bounded-generated-tests.sh`, GCC, Clang,
UBSan. It verifies byte-ownership persistence after the source
Bounded::String/Bytes changes, finite-capacity rollback, invalid
strong identity rejection and exact FieldIdentifier source kind.
