# EdlsArchiveInspection — bounded read-only historical slot assessment

Header: src/logging/EdlsArchiveInspection.hpp. All services and paths
come from Bootstrap-composed EDP-Persistence FileStorage; no concrete
ESP-IDF, Mesh, Serial, native SDK, file ownership or write is introduced.

EdlsSlotCondition: Empty, Clean, DamagedTail, InvalidHeader,
SlotMismatch, ProviderFailure. EdlsSlotInspection: Condition, Header,
HasHeader, VerifiedFrames, VerifiedPrefixBytes. InspectEdlsSlot<TMaxPayload,
TStorage>(storage,path,slot) reads and verifies the exact 40-byte EDLS
header and each successive canonical EDLG frame CRC, stopping at first
corruption without forensic resynchronisation.

EdlsArchiveCondition: Unambiguous, Ambiguous, GenerationExhausted.
EdlsArchiveInspection<TSlots>: Condition, ordered Slots, NewestSlot or
TSlots empty sentinel, NextGeneration or zero on ambiguity.
InspectEdlsArchive<TSlots,TMaxPayload,TStorage>(storage,paths) checks a
2..64 fixed-slot FilePathView topology for unreadable/header-invalid
slots, duplicate generations, mixed device identities and no-wrap.

All results are read-only candidates. NextGeneration does NOT reserve
generation, authorize writes, guarantee retention, prove a complete
immutable history or certify power interruption. The caller prevents
concurrent mutation; provider-specific concurrency/retention applies.


Profile1 additionally requires its exact EOF seal, count, framed length and whole CRC after individual frame checks. Missing complete-record suffix is DamagedTail, with no repair/resynchronisation. See [sealed batch contract](../docs/BOUNDED-EDLS-BATCHING.MD).

InspectEdlsSlotWithScratch and InspectEdlsArchiveWithScratch reuse caller-owned
scratch. If a complete physical file fits, one full ReadFileAt supplies readonly
views for the same header, frame and EOF-seal validation. A larger file keeps
the existing bounded streaming path. Neither data nor integrity evidence is
cached across calls. Every protected-history check still rereads actual media.

A failed or short bulk read never verifies frames. The inspector attempts only
an independent 40-byte header read to retain available valid identity, generation
and slot provenance, then returns failure. This prevents strict recovery from
treating a readable foreign segment as an unknown removable candidate. Corrupt
or mismatched headers keep their existing classifications; no repair occurs.
