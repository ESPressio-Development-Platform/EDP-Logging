# G4 approved LogRecord Field 11/14 child wire definitions

Status: approved by the user 2026-10-19, executable owner-composed
CBOR/JSON child-field adapter; **not** a complete 15-field
LogRecord Typed Envelope or a historical archive decoder.

## Authoritative identities and representations

The permanent System TypeIdentifier for LogRecord remains
`00 00 01 00 00 00 00 01`, with exactly 15 root FieldIdentifiers
`0..14`. These definitions govern only the payloads of existing
Field 11 and Field 14. No new semantic TypeIdentifier is minted for
either child value.

**Field 11** is the ordered two-element sequence
`[ClockCorrelationProjectionStatus, TemporalQualityAssessment]`.
Both values are exactly the existing owner-issued EDP-Clock enum
underlying `uint8_t` values. Projection:
`0=Correlated`, `1=CorrelationUnavailable`,
`2=SynchronizedCoordinateUnderflow`,
`3=SynchronizedCoordinateOverflow`.
Quality: `0=Unreliable`, `1=NotRequired`, `2=Reliable`.
Other numeric values are rejected on write **and** read.
CBOR represents the pair as an array (major type 4) of two
unsigned integers; JSON represents a two-element integer array.
The all-zero pair has canonical child CBOR `82 00 00` and
JSON `[0,0]`. Neither type substitutes for the separately
defined root SynchronizationState, uncertainty, or ClockEra.

**Field 14** is one unsigned `uint8_t` with **zero only** in v1.
Zero states that no additional record validity facts are asserted;
it does not claim correctness, authenticity or durability.
All nonzero values are reserved, and both serialization and
deserialization fail rather than normalizing or ignoring them.
Canonical CBOR zero is `00` and canonical JSON zero is `0`.

## Implementation boundary and proof

`src/logging/LogRecord.hpp` declares approved typed values and
source-semantic validation. Opt-in
`src/logging/LogRecordApprovedFieldsSerialisation.hpp` delegates
encoding/decoding to EDP-Serialisation via bidirectional
EDP-BoundedTypes conversion adapters. No hand-written alternate
codec or wire parser exists. The compiler only imports these
dependencies when this optional feature is composed.

`tests/G4ApprovedFieldWireTests.cpp` exercises all 12 valid
owner enum pairs under both codecs; source invalid enums; reverse
unknown values; malformed array lengths; zero-only Field 14 and
exact canonical samples. An additional real ESP32 HIL path
performs a runtime CBOR/JSON pair round-trip and zero-only
Field14 validation without changing the existing 18 markers.

Remaining: full generated diagnostic positional argument
representation, complete typed fifteen-field root serializer,
historical typed decoder, source-provenance connection from
EDP-Clock, concrete sink integration and actual end-to-end
product resource/power-loss testing. None of these may be
inferred from the passing child-adapter tests.
