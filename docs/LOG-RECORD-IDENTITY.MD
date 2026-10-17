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
`TypeIdentifierOf<LogRecord>`. The present source is purposely
**non-instantiable and identity-only**; it DOES NOT yet publish a
canonical `System::FieldSet` or satisfy `System::SchemaType`.
This prevents confusing a mere valid Type identity with a full,
versioned 15-field schema or a complete CBOR/JSON logging pipeline.
Those implementation and historical provenance gates remain OPEN.

The LogRecord identity is unrelated to the EDP-Localisation
StringIdentifier used for individual diagnostic messages, and Logging
never acquires a mandatory dependency on EDP-Mesh.
Test: `tests/run-log-record-identity-tests.sh` uses real EDP-System
strong identity types and compile-time byte goldens with GCC/Clang.
All package versions remain `0.1.0`.

**Collision audit:** the same 8-byte literal previously appeared in
non-governed Serialisation and Primitives tests/demos. These unregistered
fixture uses have been moved to isolated test/demo-only identifiers,
not reissued as new permanent Type identities. The broader inventory
of third-party and application-generated identities remains a
governance audit concern; this issuance is authoritative within EDP
Core, not a claim of searching unknown external systems.
