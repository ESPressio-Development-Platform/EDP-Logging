# G4 approved LogRecord Field 11/14 child wire definitions

The child-field contracts were approved on 19 October 2026. These adapters
implement the Field 11 and Field 14 values consumed by the separate
[generated fifteen-field projection](G4-GENERATED-WHOLE-RECORD.MD).

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

The child tests qualify the two conversions. The generated argument array,
root envelope, historical decoding, Clock evidence and end-to-end sink
behaviour have separate acceptance evidence in the [G1–G15 ledger](ACCEPTANCE-G1-G15.MD).
