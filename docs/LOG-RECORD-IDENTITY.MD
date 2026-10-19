# EDP-Logging::LogRecord — permanently issued universal identity

ESPressio Development Platform's authorised Core TypeAuthority (24-bit
`00 00 01`) formally issued the 40-bit TypeLocalIdentifier
`00 00 00 00 01` on **2026-10-17** for the single semantic
`ESPressio::Logging::LogRecord` Type.

- Canonical retained 8-byte identity:
  `00 00 01 00 00 00 00 01`
- Compact presentation (not a native-endian scalar):
  `0x0000010000000001`
- Authoritative registry:
  [EDP-System main TYPE-IDENTITY-REGISTRY.MD](https://github.com/ESPressio-Development-Platform/EDP-System/blob/main/docs/TYPE-IDENTITY-REGISTRY.MD)
- Owner governance issue: [EDP-System#11](https://github.com/ESPressio-Development-Platform/EDP-System/issues/11).
- Approved Logging field IDs `0–14` remain fixed and distinct from the
  LogRecord Type identifier. Schema revisions retain the same Type identity
  when the semantic type is unchanged; retired identifiers never recycle.

The opt-in `src/logging/LogRecord.hpp` owns the only semantic C++
root declaration and directly exposes its
`inline static constexpr System::TypeIdentifier Identifier`.
There is **no runtime identifier registry or allocator**. It is
designed for use through `System::IdentifiedType<LogRecord>` and
`TypeIdentifierOf<LogRecord>`. The source now publishes the **structural 15-field `System::FieldSet`**
and satisfies `System::SchemaType<LogRecord>`. Exactly approved
Field IDs 0..14 bind explicit C++ owned members, with compile-time
sequence width selection (16/32/64 bits) and bounded argument-count
and byte capacities. `ValidateLogRecord()` rejects invalid source
or attempt identity, severity, reserved String identifiers, mismatched
width, or contradictory synchronized-time/era evidence. In particular,
no synchronized coordinate requires **saturated/unknown upstream Clock
uncertainty**. Field 11's approved two EDP-Clock enum values must belong
to the issued numeric domains; Field 14 is exactly a zero-only uint8.
Their invalidity is diagnosed as `InvalidTemporalProvenance` or
`ReservedRecordFacts`. These checks do **not** automatically make the
full `LogRecord` a `SerialisableType` or certify captured history.

**CRITICAL: a `System::SchemaType` is only Type identity and C++
Field binding; it is NOT automatically `Serialisation::SerialisableType`.**
Field 11's owner-issued Clock enum pair and Field 14's zero-only uint8
have now been specifically approved for wire v1. These two reversible
child adapters are independently tested in CBOR and JSON; see
[G4 field wire definitions](./G4-APPROVED-FIELD-WIRE.MD).
The **complete** Typed CBOR/JSON LogRecord envelope, authenticated
schema history, runtime sink and task lifetimes remain OPEN.
No native C++ object is itself a wire format.

The LogRecord identity is unrelated to the EDP-Localisation
StringIdentifier used for individual diagnostic messages, and Logging
never acquires a mandatory dependency on EDP-Mesh.
Test: `tests/run-log-record-identity-tests.sh` uses real
EDP-System/Clock/Localisation source Types, exact Field 0..14 and
24/40 identifier bytes, invalid provider evidence and finite owned
argument tests with GCC/Clang. Captures are in-memory only.
All package versions remain `0.1.0`.

**Collision audit:** the same 8-byte literal previously appeared in
non-governed Serialisation and Primitives tests/demos. These unregistered
fixture uses have been moved to isolated test/demo-only identifiers,
not reissued as new permanent Type identities. The broader inventory
of third-party and application-generated identities remains a
governance audit concern; this issuance is authoritative within EDP
Core, not a claim of searching unknown external systems.



The separate [LogRecord Source Assembly](./LOG-RECORD-SOURCE-ASSEMBLY.MD)
describes the generated diagnostic source-capture contract, which is
**not yet an approved complete typed wire record**.
