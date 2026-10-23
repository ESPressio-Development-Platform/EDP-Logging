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
