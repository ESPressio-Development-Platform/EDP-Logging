# G4 optional canonical scalar-field adapters

Status: implementation candidate on `feature/logging-v1` — individual
value serialization, not a complete universal LogRecord envelope.

`src/logging/LogRecordCoreFieldSerialisation.hpp` is an opt-in,
application-composed translation between approved strongly typed Logging/
EDP-Clock source values and the **existing** EDP-Serialisation canonical
representations. Conversion remains EDP-BoundedTypes owner-adapted,
without Logging writing a separate CBOR/JSON backend or assigning new
TypeIdentifiers.

| Parent Field | Source | Canonical representation |
|---|---|---|
| 0 | generated Localisation message ID components | ordered `[uint32,uint32,uint32]` |
| 1 | `Severity` | certified native `uint8` enum |
| 4 | `AttemptSequence<Bits16/Bits32/Bits64>` | corresponding nonzero `uint16/uint32/uint64` |
| 6 | `Clock::MonotonicTimestamp` | `uint64` nanoseconds |
| 7 | optional `Clock::SynchronizedTimestamp` | optional `uint64` nanoseconds; zero is valid when present |
| 8 | `Clock::SynchronizationState` | certified owner `uint8` enum |
| 9 | `Clock::ClockEra` | existing EDP-Serialisation ClockEra owner adapter |
| 10 | `Clock::SynchronizationUncertainty` | `uint32` nanoseconds |

Fields 2/3 use existing owner System identity adapters. Field 5 is already
a plain unsigned eight-bit width discriminator; Fields 11/14 use the
previously approved G4 pair/zero-only adapters and Field 12 is the
fixed-size generated schema fingerprint. None of those mappings creates
a new parent FieldId.

LogMessageIdentifier's string component less than 3 is reserved;
attempt-sequence value zero is invalid and is rejected in both directions.
Enum traits certify the *underlying width*, **not** all semantic enum
values; full record validation must still reject invalid Severity and
SynchronizationState values. ClockEra and synchronization state are
semantically validated in the complete source record against synchronized
coordinate presence, era, and uncertainty.

Field 13 is still the unresolved complete-envelope gate: native
`OwnedDiagnosticArguments` has private source-memory kind/offset
descriptors, not an issued numeric wire kind registry. Its actual canonical
payload must be generated-schema-governed heterogeneous positional values
issued by EDP-Localisation and encoded by EDP-Serialisation. **Do not**
serialize the native arena layout, invent kind numbers, or assert a complete
15-field TypeId-bearing envelope until this path is implemented and
historical decoding is qualified.

Native test program: `tests/G4CoreFieldWireTests.cpp`, runner
`tests/run-g4-core-field-wire-tests.sh`. It validates CBOR and JSON
round trips across all supported attempt widths, owner time Types, strong
message IDs, zero synchronized timestamp and invalid identifiers/tickets.

This feature is opt-in to avoid introducing mandatory owner dependencies
to the EDP-Logging foundation. Its platform source is portable C++20;
actual ESP32 HIL validation is a distinct evidence gate.

## Physical qualification — 2026-10-20 G4 core fields

Before this work, `LogRecordCoreFieldSerialisation.hpp` was an
unpublished incomplete source file last edited 09:04 CEST; the
EDP-BoundedTypes bidirectional conversion adapter registrations
were absent. At 09:28:31 CEST the successor agent replaced the old
stale RPI400 lock with `gpt6-logging-20261020-g4core-r77`.
The owner mapping and typed conversions were completed in the
isolated worktree. No new FieldIds/TypeIdentifier or arbitrary wire
tags were issued.

- **Native strict host GCC and Clang**: full scalar canonical CBOR/JSON
  round-trip test `20261020T093142-0ab877c4` PASS exit0.
  Expanded suite with both-codec malformed/reserved message ID and
  zero attempt-ticket rejection `20261020T093523-3c930b16`
  PASS exit0. Static core header compile `20261020T093029-536cff12`
  PASS exit0. No emitted wire is trusted without semantic validation.
- **Fresh whole ESP32-D0WD/WROOM PioArduino ESP-IDF build**:
  detached `20261020T093345-92df80fc` PASS exit0,
  full factory image SHA256
  `1720fa055c3b3fe21e06d8db9acc8adbf68b4a0a6a2470d2704f33dc4d6f636c`.
  Actual physical 0x0 image flash plus separate on-chip verify
  `20261020T094009-6e856c53` PASS exit0.
  Esptool produced informational crystal-frequency detection warnings
  during stub interaction but independent on-chip digest verification
  succeeded and physical native execution was verified separately.
- **Three new physical UART + intentional NVS-restart boots**:
  detached `20261020T094056-c9586d7a` PASS exit0
  (finished 09:41:08 CEST). Every run returned original
  `markers=18/18|fail=0|binary_edlg_verified=True|`
  `resource_snapshot_verified=True|g11_site_verified=True`,
  now including opt-in owner CBOR/JSON core-field verification inside
  `GOVERNED_LOG_RECORD_VALIDATION`. Private raw captures under
  `/srv/espressio/scratch/gpt6-logging-20261019-g11/g4-core-field-captures-20261020`;
  capture SHA manifest digest
  `d24dc6ecdfd58ef802371b7a4ed4d62404a0080e9553dd950ebc00481b86859e`.
  Do not publish raw UART captures or proprietary firmware images.

The complete source record still **fails** `SerialisableType` because
Field13 needs an issued schema-qualified heterogeneous positional
representation. These tests prove each supported **core child field**
is portable and reversible through owner codecs, not a complete
15-parent-field Typed Envelope, historian end-to-end producer identity,
actual unexpected PowerLoss or completed G1–G15 production gate.
