# Generated LogRecord Wire Projection (optional G4)

**Owner:** EDP-Logging `feature/logging-v1`, C++20.

**Source:** [`src/logging/GeneratedLogRecordWireProjection.hpp`](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/src/logging/GeneratedLogRecordWireProjection.hpp); optional projection, not a replacement root semantic type.

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


## 2026-10-22 — 14 KiB WROOM application fixture proof

Three newly reflashed ESP32-WROOM HIL runs validate the generated
whole-record CBOR/JSON root with 14,336 B app_main stack and at least
3,512 B remaining. The source helper now reports high-water stack
at projection (11,208 B remaining), CBOR encode/decode (10,536 B),
and JSON encode/decode (3,512 B). A strict test-only trace validator
requires five ordered phases, monotone high-water and ≥2,048 B margin.
The JSON encoding owner path is the dominant observed stack-pressure
point; this is not a production-wide or worst-case promise.
See [G4 wire evidence](../docs/G4-GENERATED-WHOLE-RECORD.MD).

## 1 November 2026 V2 result and schema naming correction

The private generated-value adaptation operation `Detail::ConvertGeneratedValue` now returns `Detail::GeneratedValueConversionResult` with explicit `Succeeded = 0U` and `OwnerRejected = 1U`, rather than a Boolean indicating the outcome of a fallible operation. `ProjectGeneratedLogRecord` requires the typed `Succeeded` status and otherwise refuses publication with `ArgumentRejected`. Compile-time schema qualification is now named `IsDiagnosticSchemaMatching`; callers must migrate rather than rely on removed aliases. These changes do not alter canonical fifteen-field CBOR/JSON payloads or grant new hardware durability.
