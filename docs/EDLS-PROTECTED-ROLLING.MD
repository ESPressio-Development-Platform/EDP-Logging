# EDLS finite protected immutable rolling archive (2026-10-26)

**Implementation:** `src/logging/EdlsArchiveRotation.hpp`, opt-in API. **Issue:** Logging #12 / parent #1. **Version:** exactly 0.1.0.

## Capacity and ownership

The application Bootstrap configures a fixed table of N+1 unique `EDP-Persistence::FilePathView` paths, comprising N retained complete EDLS generations and exactly **one vacant successor filename**. `TProtectedSlots >= 2`, `TPhysicalSlots == TProtectedSlots + 1`, `TPhysicalSlots <= 64`. The application also supplies the canonical 16-byte DeviceIdentifier, nonzero RuntimeIncarnation, complete validated EDLG frame, bounded `TPayloadMaximum` (1..4096) and bounded `TSlotMaximum`. Exactly N complete generations remain after normal rolling. **Indefinite log retention is not a design goal.**

This is a logical capacity budget, NOT a promise of physical LittleFS scratch/metadata/GC reserve. The application must allocate sufficient real partition slack for immutable successor creation and filesystem metadata, or refuse new history without deleting protected entries.

## Publish and recovery

`PublishEdlsProtectedRollingSuccessor` checks all paths via the existing `InspectEdlsArchive`. Every occupied segment must be CRC-clean and possess unique, contiguous monotonic generations and matching DeviceIdentifier. A zero-byte *existing* file is NOT free; any unidentifiable or torn candidate fails closed. Unknown/corrupt slots must not be deleted by speculative recovery.

The existing `PublishEdlsSuccessorIntoEmptySlot` creates a **new immutable physical filename** using the existing EDLS header and CRC-bearing EDLG frame. It independently re-reads the complete new segment, its identity, generation, incarnation, frame and physical size. The protected-rolling owner then independently requalifies **every** preexisting complete segment before any retirement. Below N retained, publication succeeds without retirement. At N retained, the newly committed N+1 generation makes the physical oldest logically eligible for retirement.

`FinishEdlsProtectedRollingRetirement` is separately callable after a new boot or failed removal. It accepts only N+1 independently CRC-clean, contiguous, same-device generations and rechecks the latest N before calling the existing abstract `FileStorage::RemoveFile` on only the oldest eligible file. It independently re-inspects exactly N new generations after removal. A full N+1 archive requires this explicit recovery step before another publish. A torn/unidentifiable candidate is *not* eligible for this operation.

Neither API overwrites a still-protected filename. Both require caller-exclusive FileStorage writer rights across scan, mutation and recheck. No new memory allocator, worker, Clock, EDP component, parallel Flash owner, or platform SDK import is introduced.

## Public result types and error truth

`EdlsProtectedRollingStatus` separates:
- successful `PublishedWithinCapacity`, `PublishedAndRetired`, `RecoveredPendingRetirement`, and `NoPendingRetirement`;
- `RecoveryRequired` when the pending complete N+1 archive must be reduced to N;
- `ArchiveAmbiguous`, `NoncontiguousHistory`, `IdentityMismatch`, `InvalidFrame`, `GenerationExhausted` and `NoPhysicalReserve`;
- `ProviderRejected`, `ReadbackUnconfirmed`, `ProtectedHistoryChanged`, `RetirementRejected` and `RetirementUnconfirmed`.

`EdlsProtectedRollingOutcome<TPhysicalSlots>` retains `Status`, original `FileReplaceStatus NativeWrite`, original `FileRemoveStatus NativeRemove`, `PublishedSlot`, `RetiredSlot` and proposed `Generation`. Non-success does not imply the media is unchanged: an interrupted but complete successor can exist and must be re-inspected before any next operation.

## Tested versus unqualified

New RPI400 GCC/Clang fake-FileStorage tests cover 42 generations, finite rollover, incarnation changes, failure before new write, torn candidate, noncontiguous or corrupt history, limit exhaustion, collateral protected-record change and interrupted retirement/recovery. Real host `VfsFileStorage` tests use separate reopened provider instances through 24 generations, an interrupted complete-but-not-retired successor, and a preexisting zero-byte candidate collision. GCC UBSan also passes; see the contemporaneous Policy/main living handoff for exact IDs.

**Unqualified:** power-loss survival of this exact EDLS API on native ESP32 LittleFS, arbitrary internal garbage collection or flash instruction interruption, complete LogRecord→EDP-Serialisation strongly-typed CBOR projection, torn-candidate provenance and safe cleanup, physical reserve/wear/lifetime budgets and other Logging G1–G15 release gates. Provider traits remain Restart/None/MayModify. The prior synthetic two-record hard-power-cut tests are relevant positive evidence, **not** a general EDLS production guarantee. All versions remain 0.1.0.
