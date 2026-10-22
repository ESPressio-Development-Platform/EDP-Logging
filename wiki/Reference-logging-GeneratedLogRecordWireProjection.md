# Generated LogRecord Wire Projection (optional G4)

**Owner:** EDP-Logging `feature/logging-v1`, C++20.

The optional `GeneratedLogRecordWireProjection<TWidth,TArgs...>`
has the same permanent LogRecord TypeIdentifier and fifteen issued
EDP-System FieldIds. Field13 is a statically heterogeneous
`std::tuple` matching the exact message Type parameters from
an EDP-Localisation generated contract. The adapter verifies
message identity/fingerprint and performs bounded, scratch-owned
source rehydration. EDP-Serialisation owns the CBOR/JSON Typed
Envelope and tuple encoding; Logging does not issue wire tags.

Exporters supply actual `TMessageSchema`, parameter Types and
optional strong ClockEra adapter at application composition.
No runtime schema registry or dynamic heap is introduced.

Native GCC/Clang full-root contract tests passed, and an actual
ESP32-WROOM physical image passed three 18/18-marker HIL runs.
The test app used 16 KiB task stack after an 8 KiB stack fault;
peak observed task usage 10,824 B. No universal product minimum
or final Logging release is implied.

See [G4 complete generated root contract](../docs/G4-GENERATED-WHOLE-RECORD.MD)
and the physically measured test entrypoint
`tests/hil/g1-espidf/src/g4_whole_record_check.hpp`.
