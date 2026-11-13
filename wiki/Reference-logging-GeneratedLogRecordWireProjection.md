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

## 12 November 2026 — complete generated value conversion and projection failures

**Current source:** [`GeneratedLogRecordWireProjection.hpp`](../src/logging/GeneratedLogRecordWireProjection.hpp). The internal `Detail::GeneratedValueConversionResult : uint8_t` separates **Succeeded (0)**, an EDP-BoundedTypes/Serialisation owner-approved semantic → canonical value conversion succeeded, from **OwnerRejected (1)**, semantic input could not be converted. It never falls back to an arbitrary static_cast or builds a separate wire Type registry.

`GeneratedLogRecordProjectionStatus : uint8_t` has: **Succeeded (0)**, complete schema-bound structural projection; **InvalidSource (1)**, the source `ValidateLogRecord` rejected its permanent fields; **GeneratedMessageMismatch (2)**, Domain/SubDomain/String differs from the exact generated message signature; **DiagnosticFingerprintMismatch (3)**, the source diagnostic schema does not match the generated owner fingerprint; **ArgumentCountMismatch (4)**, source-owned Field13 argument count differs from the generated ordered `TParameters...` count; **ArgumentRejected (5)**, a specific source argument cannot be rehydrated/converted under the owner's exact Type and wire semantics. `GeneratedLogRecordProjectionOutcome` preserves `Status` (default InvalidSource) and `FirstFailedArgument`, which carries the actual source count on ArgumentCountMismatch or the zero-based argument position on ArgumentRejected. `IsSuccessful()` reports exactly Succeeded, not native storage acceptance.

`Detail::HasOwnerCanonicalRepresentation<TValue>` identifies whether EDP-Serialisation declares a canonical wire value Type for a source semantic Type. `Detail::GeneratedPositionalWireType<TValue,THasCanonical>` yields either the original Type or the existing `Serialisation::CanonicalRepresentation<TValue>::Type`, and its alias `GeneratedPositionalWireTypeOf<TValue>` selects that value without introducing another owner. `Detail::ConvertGeneratedValue<TValue>(semantic,wire)` invokes the existing EDP-BoundedTypes `TypeConversionAdapter` when certified, checks its strongly typed result and leaves semantic wire meaning in that owner, never inventing TypeId/FieldId numbers.

`GeneratedLogRecordWireProjection<TWidth,TParameters...>` is the exact EDP-System `SchemaType` 15-field, type-governed **temporary wire projection** for one existing LogRecord; `TWidth` preserves its attempt ticket width and `TParameters...` fixes the diagnostic positional child values. It carries the same owner-issued message, severity, device/incarnation, attempt, Clock state, optional synchronized coordinate, era/uncertainty, provenance, reserved facts and generated fingerprint as the source, but Field13 becomes the strongly typed ordered positional child tuple instead of the private `OwnedDiagnosticArguments` descriptor arena. It does not borrow a source record's transient argument spans after successful construction.

`GeneratedLogRecordWireProjection::Fields` is the public schema alias used
by EDP-System and EDP-Serialisation. Its ordered `System::FieldSet` binds
each projection member to its issued FieldId from 0 to 14. The field set
keeps the permanent root identity and uses the generated positional tuple
for Field13. Changing the binding order or reassigning a FieldId would
change the wire contract.

`ProjectGeneratedLogRecord<TMessageSchema,TWidth,TParameters...,TMaximumArguments,TArgumentBytes>(source,output,scratch)` requires a generated exact message descriptor, validates all source fields, fingerprint and argument count, rehydrates Field13 through the existing owner-certified positional scratch, then publishes the **complete** projection only after all conversions succeed. On failure it preserves a typed stage/argument index and clears/invalidates output as the existing source algorithm specifies; it is not a portable wire serializer or an asynchronous delivery API. Caller scratch is borrowed only for the operation; canonical bytes are later produced by EDP-Serialisation TypedEnvelope CBOR/JSON. No implicit heap or Clock/Memory provider is instantiated.
