# Reference — `src/logging/LogRecordApprovedFieldsSerialisation.hpp`

**Classification:** internal cross-library canonical adapters. [Reviewed source](../src/logging/LogRecordApprovedFieldsSerialisation.hpp).

The opt-in adapters supply the issued [Field 11 and Field 14 child representations](../docs/G4-APPROVED-FIELD-WIRE.MD). EDP-Serialisation owns the canonical representation contract, EDP-BoundedTypes owns the conversion interface and EDP-Clock owns the temporal status enumerators.

| `ApprovedLogFieldConversion` | Meaning |
| --- | --- |
| `Succeeded` (0) | Conversion assigned the complete output. |
| `UnknownClockValue` (1) | The source projection or quality is outside its issued EDP-Clock enum range. |
| `ReservedRecordFacts` (2) | A v1 record fact bit is asserted. This revision only permits zero. |

`IsApprovedProvenance(p)` compares the underlying projection and quality values with `SynchronizedCoordinateOverflow` and `Reliable`, the issued upper bounds. The predicate establishes range membership. The Clock source and complete record validation establish availability and consistency.

## Canonical representation and all conversion specializations

| Adapter specialization | `Convert(src, dst)` |
| --- | --- |
| `LogTemporalProvenance → array<uint8_t, 2>` | Checks both enum ranges, then assigns Projection followed by Quality. |
| `array<uint8_t, 2> → LogTemporalProvenance` | Decodes the ordered enum pair into a temporary. Assigns the output after both values pass the same range check. |
| `LogRecordFacts → uint8_t` | Checks `src.Value == 0`, then assigns the canonical zero byte. |
| `uint8_t → LogRecordFacts` | Checks `src == 0`, then assigns zero to the output's `Value` field. |

The two `Serialisation::CanonicalRepresentation` specializations expose `Type` as `array<uint8_t, 2>` for `LogTemporalProvenance` and `uint8_t` for `LogRecordFacts`. The four `Bounded::TypeConversionAdapter` specializations each expose `ResultType = ApprovedLogFieldConversion`, `IsAvailable = true` and `IsNoexcept = true`. `IsSuccessful(r)` returns true exactly for `Succeeded`. Compile-time checks verify that both semantic values satisfy `SerialisableType`.

All four operations are constexpr, synchronous and allocation-free. The caller owns the source and output and each refusal preserves the output. Neither adapter reads a clock, selects a provider or retains a reference. The [normative contract](../docs/G4-APPROVED-FIELD-WIRE.MD) links the host and native qualification of the child values.
