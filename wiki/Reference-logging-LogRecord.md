# LogRecord — permanent System semantic type identity (G4)

Opt-in `logging/LogRecord.hpp` declares a direct immutable compile-time
`System::TypeIdentifier`:
`00 00 01 00 00 00 00 01`.
The 24-bit Authority `0x000001` belongs to ESPressio Development
Platform Core; the 40-bit local value `0x0000000001` belongs
permanently and exclusively to `Logging::LogRecord`. This allocation
was authorised 2026-10-17 and published in the owner
[System registry](https://github.com/ESPressio-Development-Platform/EDP-System/blob/main/docs/TYPE-IDENTITY-REGISTRY.MD).

`System::IdentifiedType<Logging::LogRecord>` is true, and
`System::TypeIdentifierOf<Logging::LogRecord>` reads identical
eight-byte storage. The current class is deliberately NON-INSTANTIABLE
and does not yet declare `Fields`: only the semantic identity is
implemented; the approved canonical fields 0..14 and governed
Localisation/Serialisation CBOR/JSON source/runtime follow separately.
In particular `System::SchemaType<Logging::LogRecord>` is not yet true.

Identity persistence is independent of library version (all remain
0.1.0), message StringIdentifier, transport or Mesh participation.
