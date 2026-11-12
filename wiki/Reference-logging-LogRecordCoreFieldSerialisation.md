# Reference — `src/logging/LogRecordCoreFieldSerialisation.hpp`

**Classification:** internal scalar traits and cross-library canonical field adapters. [Reviewed source](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/73e39775eb0d4bd5398e479ae931aa42c47ab22a/src/logging/LogRecordCoreFieldSerialisation.hpp).

The header implements the [G4 scalar child mappings](../docs/G4-CORE-FIELD-ADAPTERS.MD). It consumes EDP-BoundedTypes conversion contracts and EDP-Serialisation canonical traits, including the existing ClockEra adapter. These mappings operate on individual values. The generated projection owns root record encoding and Field 13 positional values.

## Conversion results and private mappings

`LogRecordCoreFieldConversion::Succeeded` (0) means the output contains the mapped value. `InvalidSemanticValue` (1) means a reserved message component, zero ticket or unissued enum value was refused. The private primary `Detail::LogRecordScalarRepresentation<TSemantic, TWire>` is declared without a definition. `TSemantic` selects the exact strong type and `TWire` selects its approved canonical representation. Only the following pairs are defined.

| Semantic type | Wire type | `To(source, wire)` and `From(wire, source)` |
| --- | --- | --- |
| `LogMessageIdentifier` | `array<uint32_t, 3>` | Maps Domain, SubDomain and String in that order. Both directions reject String values below 3 before assigning output. |
| `Severity` | `uint8_t` | `To` writes the ordinal and reports failure above Fatal. `From` validates the ordinal before assigning the enum. |
| `AttemptSequence<TWidth>` | `Storage` | `Storage` names the ticket's unsigned storage type. `TWidth` chooses Bits16, Bits32 or Bits64 and therefore the exact width. `To` writes the ticket value and reports failure for zero. `From` rejects zero before invoking `FromValidatedValue`. |
| `Clock::MonotonicTimestamp` | `uint64_t` | Transfers the owner nanosecond value with `Nanoseconds()` and `FromNanoseconds()`. The conversion does not acquire a clock sample. |
| `Clock::SynchronizedTimestamp` | `uint64_t` | Transfers the owner nanosecond value, including zero. The root record owns synchronized-coordinate presence. |
| `Clock::SynchronizationState` | `uint8_t` | `To` writes the ordinal and reports failure above LostSynchronization. `From` validates before assigning the enum. |
| `Clock::SynchronizationUncertainty` | `uint32_t` | Transfers the owner nanosecond value in both directions. The enclosing record validates how it relates to the selected coordinate. |

Callers use a scalar output only after `Succeeded`. The forward Severity, SynchronizationState and AttemptSequence mappings assign their temporary scalar before testing it. Rejected reverse mappings leave the destination unchanged.

## Cross-library registrations

- `Serialisation::EnumSerialisationTraits<Severity>` and `<Clock::SynchronizationState>` each expose `UnderlyingType = uint8_t`. They certify the encoded width. Complete record validation enforces the accepted enum and temporal combinations.
- The `Serialisation::CanonicalRepresentation` specializations for `LogMessageIdentifier`, `AttemptSequence<TWidth>`, both timestamp types and `SynchronizationUncertainty` each expose `Type` equal to the corresponding wire type in the table. ClockEra's canonical mapping comes from the existing EDP-Serialisation owner header.
- `Detail::ToCoreWire<TSemantic, TWire>` and `Detail::FromCoreWire<TWire, TSemantic>` adapt the private mappings to BoundedTypes. `ResultType` is the conversion enum, `IsAvailable` and `IsNoexcept` are true, `IsSuccessful(r)` tests `Succeeded` and `Convert` delegates to the corresponding `To` or `From` operation. The supplied references remain caller-owned.
- `Bounded::TypeConversionAdapter` specializes both directions of the five canonical-representation pairs above, inheriting the matching bridge. The two enums use the separate Serialisation traits. Static assertions check all declared serialisable types, including all three ticket widths and the imported ClockEra adapter.

These constexpr mappings retain no state and use no provider I/O or dynamic storage. The [core-field adapter contract](../docs/G4-CORE-FIELD-ADAPTERS.MD) identifies the CBOR/JSON and target fixtures.
