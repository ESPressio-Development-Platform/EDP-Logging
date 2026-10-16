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
