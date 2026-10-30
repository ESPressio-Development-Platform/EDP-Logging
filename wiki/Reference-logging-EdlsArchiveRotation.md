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

## Real relay power cut with partial successor — 2026-10-23

An additional dedicated ESP-IDF fixture now covers crash recovery
after deliberately publishing a torn oldest-slot successor and then
electrically cutting RPI400-integrated ESP32 power using relay GPIO17.
Run `20261023T113223-4de652e4` PASS: the protected generation 2
remained valid, generation 3 was `DamagedTail`, and rotation failed
closed. The cut was between separate logical writes, **not inside**
the partial native ReplaceFile; the VFS/FileStorage interruption
capabilities remain conservative and unchanged. See
`tests/hil/g7-torn-slot-espidf/README.MD`.


## 2026-10-26 — finite protected N+1 immutable rolling APIs

**Public optional API:** `PublishEdlsProtectedRollingSuccessor<TPhysicalSlots,TProtectedSlots,TPayloadMaximum,TSlotMaximum,TStorage>` and `FinishEdlsProtectedRollingRetirement<TPhysicalSlots,TProtectedSlots,TPayloadMaximum,TStorage>`. Both are located in the **existing** `EdlsArchiveRotation.hpp`; there is no additional archive class, SDK binding or allocator. The older `PublishEdlsOldestEligibleSuccessor` continues to exist for its weaker best-effort profile but **must not** be used to overwrite still-protected EDLS history on LittleFS.

**Template contracts:** `TPhysicalSlots` is the total fixed Bootstrap physical names; `TProtectedSlots` the exact retained capacity, constrained `TPhysicalSlots = TProtectedSlots+1`, N≥2, total ≤64. `TPayloadMaximum` is the maximum single EDLG payload (1..4096); `TSlotMaximum` must hold at least a 40-byte EDLS header plus the complete maximum EDLG frame; `TStorage` is the caller's exclusive abstract FileStorage provider. Publication takes those fixed names, the canonical 16-byte DeviceIdentifier, the runtime incarnation and a borrowed complete canonical EDLG frame. Retirement takes the storage and exact path array; it recomputes provenance rather than relying on prior process state.

**Public enum:** `EdlsProtectedRollingStatus` has separate `PublishedWithinCapacity`, `PublishedAndRetired`, `RecoveredPendingRetirement`, `NoPendingRetirement`, `RecoveryRequired`, `ArchiveAmbiguous`, `NoncontiguousHistory`, `IdentityMismatch`, `InvalidFrame`, `GenerationExhausted`, `NoPhysicalReserve`, `ProviderRejected`, `ReadbackUnconfirmed`, `ProtectedHistoryChanged`, `RetirementRejected` and `RetirementUnconfirmed`. No value upgrades underlying provider interruption atomicity.

**Public result struct:** `EdlsProtectedRollingOutcome<TPhysicalSlots>` is value-only. `Status` is the exact semantic outcome. `NativeWrite` and `NativeRemove` preserve original EDP-Persistence FileReplace/Remove statuses when applicable. `PublishedSlot` and `RetiredSlot` are selected physical slot indices, otherwise sentinel TPhysicalSlots. `Generation` is the proposed/resumed canonical EDLS monotonic generation, otherwise zero.

**Internal helpers:** `Detail::EdlsProtectedRollingPlan<TPhysicalSlots,TProtectedSlots,TPayloadMaximum,TStorage>` stores bounded `Status`, `Inspected` full existing archive scan, `Occupied` count, `OldestSlot`, `OldestGeneration` and `NewestGeneration`. `Detail::InspectProtectedRollingPlan` verifies physically **absent** spare names, CRC-clean complete frame prefixes, unique contiguous no-wrap generation numbering and consistent identity; zero-length existing files or unidentifiable candidates fail closed. `Detail::ConfirmProtectedRollingPredecessors` independently rechecks each preexisting protected slot using the existing verified-prefix comparison helper. Both helpers are read-only.

**Lifecycle and safety:** Publish a verified immutable successor into the physically unused spare path, independently verify all previous protected files, *then* physically remove only the logically retired oldest name. After reboot, a separately complete N+1 archive is eligible for verified retirement recovery; torn or unknown candidates must NOT be silently reclaimed. Physical partition scratch/GC reserve is an additional application responsibility. The actual LittleFS provider remains Restart/None/MayModify; no generic PowerLoss guarantee is inferred. Reference the [normative protected rolling contract](../docs/EDLS-PROTECTED-ROLLING.MD) and focused `tests/EdlsProtectedRollingTests.cpp` / `tests/RealVfsProtectedRollingTests.cpp`. EDP versions remain 0.1.0.

## 2026-10-26 typed LogRecord CBOR archive integration

`GeneratedLogRecordEdlsArchive.hpp` adds the opt-in **public worker-side** `PublishGeneratedLogRecordToProtectedEdls` with `GeneratedEdlsPublishStatus` and `GeneratedEdlsPublishOutcome`. It composes *exact generated EDP-Localisation positional types* and existing fifteen approved System FieldIds via `ProjectGeneratedLogRecord`, canonical EDP-Serialisation CBOR `TypedEnvelope`, existing EDLG CRC32C framing, then `PublishEdlsProtectedRollingSuccessor`. Caller owns projection, positional scratch, separate bounded CBOR and EDLG buffers, exclusive FileStorage, static physical names and source-consistent DeviceIdentifier/runtime incarnation. `Published` requires complete independently verified immutable EDLS successor and safeguarded predecessors. Other explicit statuses include source identity mismatch, projection rejection, CBOR measurement/encoding failure, CBOR capacity exceeded, EDLG framing failure, and propagated archive refusal with exact nested status. This does **not** imply full PowerLoss provider guarantee or indefinite history; N retained+one vacant physical successor is mandatory. See [normative document](../docs/TYPED-EDLS-CBOR-ARCHIVE.MD) and `tests/GeneratedEdlsArchiveIntegrationTests.cpp`. Full native typed-payload interruption/worker integration is still pending.

## 2026-10-26 — typed CBOR worker delivery adapters

`GeneratedEdlsTaskAdapters.hpp` declares optional `GeneratedLogRecordCborEncoder<TMessageSchema,TWidth,TMaximumArguments,TArgumentBytes,TParameters...>` as the no-allocation EDP-Memory fanout encoder, and `TypedCborProtectedEdlsTaskDelivery<TMessageSchema,TWidth,TPhysicalSlots,TProtectedSlots,TMaximumPayload,TMaximumSegment,TStorage,TParameters...>` as the **existing Task-owned** synchronous delivery callback. Use only after copying CBOR into an independent EDP-Memory lease via existing `AsyncCopyingTaskSink`; Task acceptance does not imply durability. The delivery validates full canonical typed CBOR and matching generated message/diagnostic fingerprint, source device and runtime incarnation, then frames EDLG and publishes the protected N+1 EDLS successor. `RecordSinkSubmission::Accepted` is returned only after checked archive completion; inspect `LastCompletion` and nested `LastArchive` only after Task terminal with application-provided synchronization. `TypedEdlsTaskCompletion` is a finite value enum `NotInvoked`, `Archived`, `InvalidCodec`, `InvalidTypedRecord`, `SourceIdentityMismatch`, `FramingFailure`, `ArchiveFailure`. A new worker, Flash owner or codec is NOT created. Refer to [typed contract](../docs/TYPED-EDLS-CBOR-ARCHIVE.MD).

## 2026-10-26 WROOM large-payload stack-scratch variants

`InspectEdlsSlotWithScratch`, `InspectEdlsArchiveWithScratch`,
`PublishEdlsSuccessorIntoEmptySlotWithScratch`,
`FinishEdlsProtectedRollingRetirementWithScratch`,
`PublishEdlsProtectedRollingSuccessorWithScratch` and
`PublishGeneratedLogRecordToProtectedEdlsWithScratch` use the **same existing
semantic owner**, requiring one caller-owned writable span with at least
`EdlsHeaderBytes + TPayloadMaximum + 20` bytes. The legacy APIs delegate to
the same logic with local scratch; **do not select those high-payload
stack-backed wrappers on ESP32-WROOM without a separately measured stack
budget**. The reusable scratch must not alias the caller's live EDLG source
span. This fixes a real physically observed panic (4 KiB payload in native
HIL) without relaxing frame/header validation, finite N+1 retention,
PowerLoss constraints or EDP version 0.1.0.


## Post-retirement CRC scan reuse

The exclusive retirement owner retains complete pre-delete CRC proofs and one full post-delete CRC/contiguous-generation inspection. It omits only the immediately repeated second post-delete scan, with no intervening storage mutation. Strict NVS ACK-before-retirement ordering remains unchanged. See [measured qualification](../docs/EDLS-POSTRETIRE-SCAN-OPTIMISATION-2026-10-29.MD).
