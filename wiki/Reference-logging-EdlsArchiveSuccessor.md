# EdlsArchiveSuccessor — guarded empty-slot EDLS publication

Source src/logging/EdlsArchiveSuccessor.hpp is an opt-in generic
EDP-Persistence::FileStorage consumer. It never allocates a new
backend, erases occupied slots, or depends on Mesh or Serial.

PublishEdlsSuccessorIntoEmptySlot<TSlots,TPayloadMaximum,TSlotMaximum>
(storage,paths,deviceIdentifier,incarnation,completeEdlg)
requires 2..64 static paths, 1..4096 payload bytes, valid EDLG frame
and stable exclusive app-side writer lifetime. It reads all existing
slots, enforces one device and unique no-wrap generation, chooses a
physically EMPTY slot, emits one canonical EDLS header and frame via
native ReplaceFile, then verifies readback through the existing
read-only scanner.

EdlsSuccessorStatus: PublishedAndVerified, InvalidIdentityOrFrame,
ArchiveAmbiguous, NoEmptySlot, SlotCapacityExceeded,
ChangedDuringPreflight, ProviderRejected, ReadbackUnconfirmed.
EdlsSuccessorOutcome<TSlots> carries Status, Native
EDP-Persistence::FileReplaceStatus, Slot/TSlots invalid marker, and
Generation. A success asserts checked media readback at that instant,
not future power-loss durability. The ordinary ESP-IDF VFS provider
publishes no FileReplace power-loss atomicity guarantee, so a
failed/interrupted replace may leave a damaged newer slot; the
next inspection must fail closed until authenticated repair.

NoEmptySlot is expected once all bounded slots hold earlier retained
history. Automatic destructive circular rotation remains deliberately
absent pending certified provider atomicity and retention policy.


## Sealed bounded frame sequences

The same successor owner adds trailing template TMaximumFrames (default1, maximum64). Multi-frame opt-in validates all frames, writes profile1 plus EDLC count/length/whole CRC and independently compares complete attempted frame bytes. Caller scratch fits header+body+seal. One-frame defaults preserve profile0. See [wire/ownership contract](../docs/BOUNDED-EDLS-BATCHING.MD).
