# Fail-closed EDLS successor publication into a physically empty slot

Optional header: src/logging/EdlsArchiveSuccessor.hpp.

The new PublishEdlsSuccessorIntoEmptySlot routine consumes the existing
EDP-Persistence **abstract FileStorage** owner and canonical EDLS and
EDLG encoder/decoder. The caller supplies its bounded, exactly
configured FilePathView slot table, 16-byte DeviceIdentifier, nonzero
incarnation, and one already-framed canonical EDLG message.

Within application/Bootstrap-controlled **exclusive mutation ownership**
the routine verifies the frame, scans every slot with the existing
read-only historical inspector, rejects ANY ambiguity or exhausted
generation, and refuses a caller DeviceIdentifier that disagrees with
existing history. It chooses an **EMPTY** physical slot after the
newest observed slot, never overwrites any occupied generation. It
checks GetFileSize again before mutation, assembles the exact EDLS
header plus EDLG frame in bounded scratch, calls EDP-Persistence
FileStorage ReplaceFile once, then opens and checks the actual published
bytes afresh: header CRC, slot identity, DeviceIdentifier, incarnation,
generation, frame CRC and exact size. It compares the framed range to the
input using the selected EDP-Memory ByteOperations provider. Both single-frame
and sealed multi-frame publication use the verified whole-file capture already
in caller scratch. The sealed inspection includes the completion marker and
whole-segment CRC.

Statuses: PublishedAndVerified, InvalidIdentityOrFrame,
ArchiveAmbiguous, NoEmptySlot, SlotCapacityExceeded,
ChangedDuringPreflight, ProviderRejected and ReadbackUnconfirmed.
Result also exposes exact native FileReplaceStatus, selected slot,
and proposed generation. If native replacement reports failure after
partially modifying the file, the older generations remain physically
untouched but the next full scan must treat a torn target as ambiguous.
If all fixed slots are occupied, NoEmptySlot is an intentional
**safe-capacity stop**, NOT an alleged implemented rolling/wear
rotation. This API never silently erases old historical records.

**Limits and semantics**: VFS FileStorage mutation can advertise
FailurePreservation::MayModify and InterruptionAtomicity::None;
a native ReplaceFile success followed by readback is not a
power-loss-safe publication or future retention guarantee. The
application must own the exclusive writer lock throughout planning
and publication; size recheck alone is not a mutex. An optional
durable generation reservation/commit journal, approved retained
history eviction policy, official signed schema manifest, and
external power-interruption testing remain necessary for a
power-cut-certified rotating archive.

Native GCC/Clang tests cover initial and second successor,
no-empty-slot protection, mismatched identity, bad CRC,
generation wrap, unreadable prior header, rejected native replace,
partial provider mutation, corrupted successful readback and substitution
of a different same-size frame with valid CRCs.
Separate host integration tests bind the **real EDP-Persistence-ESP-IDF
VfsFileStorage** provider to an isolated disposable native filesystem
directory. That proves two generations persist across creation of a
second concrete provider object, rejects further writes at capacity
and rejects deliberate file tampering without destroying the oldest
clean generation. None of these tests fakes a powered-off device.
The ESP32 HIL binds the same VFS provider to a disposable SPIFFS
test-only partition and verifies two generations and fail-closed
corrupted second-slot handling on actual device, where observed;
see HIL VALIDATION.MD for exact results and limitations.
