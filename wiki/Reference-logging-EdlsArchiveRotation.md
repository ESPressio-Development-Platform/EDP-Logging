# Reference — EDLS oldest-eligible rotation (G6/G7)

The optional, application-selected
`logging/EdlsArchiveRotation.hpp` implements
`PublishEdlsOldestEligibleSuccessor<Slots,PayloadMaximum,SlotMaximum>()`.
It requires the existing abstract EDP-Persistence FileStorage provider,
static 2–64 slot paths, exclusive caller-owned write access, a
canonical DeviceIdentifier, incarnation and validated complete EDLG
frame. It introduces no new provider, queue, worker or allocator.

When empty slots exist it delegates to the established
`EdlsArchiveSuccessor` no-overwrite routine. Once full, a rotation
may replace **only** the oldest CRC-clean candidate, preserving the
newest currently certified generation and failing closed if any slot
is corrupted, ambiguous, or a generation limit is reached.

`EdlsRollingStatus` separates PublishedInEmptySlot,
PublishedAfterOldestReclaim, InvalidIdentityOrFrame,
ArchiveAmbiguous, SlotCapacityExceeded, ChangedDuringPreflight,
ProviderRejected, ReadbackUnconfirmed, PriorNewestUnconfirmed and
NoReclaimableSlot. `EdlsRollingOutcome` retains selected slot,
protected newest slot, proposed generation, raw native
`FileReplaceStatus` and whether readback requalified the previous
newest generation. The oldest slot's replacement bytes are also
checked independently against the original expected frame.

A successful checked replacement is **not** power-cut durability:
VFS failure may alter old data, and a CRC does not prove physical
retention. This module refuses automatic replay over torn tails; a
separate owner-approved recovery/PowerLoss policy is required.
See [normative EDLS rotation contract](
../docs/EDLS-ARCHIVE-ROTATION.MD), host tests
`EdlsArchiveInspectionTests.cpp`, `RealVfsArchiveSuccessorTests.cpp`,
and the native HIL `tests/hil/g1-espidf/VALIDATION.MD`.


## Physical 20/20 checks (2026-10-23)

Three independent RPI400 ESP32-WROOM cycles passed the new native
SPIFFS oldest-eligible rotation, protected newest readback and
corrupted-tail refusal, plus all existing lifecycle/EDLG/G11 markers.
Detached physical run `20261023T083422-cc50dcba` PASS exit0, exact
factory image SHA256
`354e98347121bbce2a4c006b50b6ca1cab342547b9770eb4110b263d8ea155b8`.
No power-cut result or production retention guarantee follows.

## Physical power-loss HIL (2026-10-23)

The RPI400-integrated ESP32 now has a genuine switched 3.3V feed via
GPIO17 relay; the user confirmed that its wiring prevents signal
back-powering. The first destructive hardware campaign is tracked by
`20261023T110910-d66bc60e`: 2 independently verified EDLS generations
survived an actual power interruption of a concurrent SPIFFS
FileStorage::ReplaceFile, recovered after an `ESP_RST_POWERON` reset.
The test uses a private two-MiB flash partition map and retains full
pre-flash backup/evidence. **This is a measured retention result, not a
new provider-level interruption-atomic contract.** Remaining G7/#12
work includes torn EDLS slot and rollover-cut failure cases. Full
fixture and repeatability instructions:
`tests/hil/g7-power-cut-espidf/README.MD`.
