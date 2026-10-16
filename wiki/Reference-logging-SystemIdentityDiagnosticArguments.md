# Generated System identity diagnostic arguments

New G3 support inside `logging/DiagnosticArgumentEncoding.hpp` (0.1.0). EDP-Localisation generated kinds `DeviceIdentifier` and `RuntimeIncarnationId` accept only `System::Identity::DeviceIdentifier` and `System::Identity::RuntimeIncarnationId` respectively, in exact declared positions. Typed CBOR/JSON values are emitted by EDP-Serialisation's reversible canonical SystemIdentityAdapters.

The 16-octet DeviceIdentifier is CBOR **byte string** / JSON **Base64**. RuntimeIncarnationId uses an owner-defined `uint32`. Invalid-zero markers do not become valid identities by successful serialization. Existing generated schema static assertions reject raw arrays/uint32 substituting for strong values. No EDP-Clock era assignment or G4 governed LogRecord root invented.

See [full G3 contract](../docs/DIAGNOSTIC-SYSTEM-IDENTITY-ARGUMENTS.MD) and [tests](../tests/run-diagnostic-system-identity-tests.sh).
