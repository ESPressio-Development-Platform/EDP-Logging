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
width, or contradictory synchronized-time/era evidence.

**CRITICAL: a `System::SchemaType` is only Type identity and C++
Field binding; it is NOT automatically `Serialisation::SerialisableType`.**
Field 11 temporal provenance and Field 14 record-validity-fact bit
meanings are **not registered**. Their current zero-initialized
in-memory opaque placeholders make NO claim about approved wire bit
meaning, width, or historical schema stability. Typed CBOR/JSON
LogRecord envelope, authenticated schema history, runtime sink and
task lifetimes remain OPEN. No native C++ object is ever a wire format.

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
