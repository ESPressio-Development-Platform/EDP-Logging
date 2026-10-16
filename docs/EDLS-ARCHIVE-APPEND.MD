# Optional checked append to one clean EDLS segment (partial G7)

Source: [EdlsArchiveAppend.hpp](../src/logging/EdlsArchiveAppend.hpp).
This is deliberately a **nonrotating mutation primitive**, not a
complete historical archive coordinator.

AppendEdlgToCleanEdlsSlot<TMaxPayloadBytes,TMaximumSlotBytes>(storage,
path,physicalSlot,frame) consumes ONLY application-supplied
EDP-Persistence::FileStorage. It validates an exact complete EDLG frame
using the canonical owner frame reader, rescans the entire selected EDLS
slot using InspectEdlsSlot, rejects any damaged tail, invalid header,
wrong physical slot or unreadable backend, enforces a fixed segment
byte capacity, and independently rechecks GetFileSize before making ONE
native AppendFile call. The application must retain the exclusive
FileStorage writer lock throughout both reads and the append. A size
recheck alone cannot make concurrent mutation safe.

The return status distinguishes InvalidFrame, SegmentUnclean,
SegmentCapacityExceeded, ChangedDuringPreflight, ProviderRejected,
ProviderAccepted, and exposes native EDP-Persistence FileAppendStatus.
Native accepted means ONLY what the selected provider advertises.
**NO inherent PowerLoss commit, fsync, append atomicity or retained
history is implied**; a partial/torn append can damage a tail.
The inspector recovers only its valid prefix and a future, dedicated
rotation coordinator must choose a CLEAN successor generation. This
primitive never assumes it can append behind torn data, wraps
generations, erases another slot, duplicates the Persistence provider,
or imports a native SDK/Serial/Mesh dependency.

GCC/Clang host tests validate successful append, hard file capacity,
rejected/failed native append, tampered EDLG CRC, damaged existing tail,
and file-size change detected during preflight. Physical ESP32 testing
of this particular VFS append path requires an application-approved
mounted filesystem, explicit PowerLoss contract and dedicated test
partition. Earlier NVS checkpoint HIL is NOT such a test.
