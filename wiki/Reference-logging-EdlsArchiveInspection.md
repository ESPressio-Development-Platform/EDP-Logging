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

## 12 November 2026 — full per-declaration classification, native reads and failure contract

**Exact source:** [`src/logging/EdlsArchiveInspection.hpp`](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/8d2b307a8a1db1aa5596abc904cd0da1f3c032cd/src/logging/EdlsArchiveInspection.hpp). This public **read-only** optional API composes the abstract EDP-Persistence FileStorage owner and the existing Logging strict EDLS/EDLG readers. No independent provider, allocator, file writer, recovery mutation, task or volatile evidence cache is created.

**Public enum `EdlsSlotCondition : uint8_t`:** **Empty (0)** means FileStorage reports no physical file; **Clean (1)** means exact header, path/slot, complete frame and optional EDLC seal were independently verified; **DamagedTail (2)** means a previously valid prefix is followed by an incomplete/invalid frame or seal and **must not** be treated as appendable; **InvalidHeader (3)** means the 40-byte EDLS header is absent, incompatible or CRC-invalid; **SlotMismatch (4)** means an otherwise parseable header claims another physical slot ordinal; **ProviderFailure (5)** means a native file operation/read is refused or indeterminate. Only the complete Clean result is eligible for subsequent guarded archive preflight, and even Clean is not a write authorization.

**Public value `EdlsSlotInspection`:** **Condition** is the above status (defaults ProviderFailure); **Header : EdlsHeader** is the parsed identity/generation/profile only when **HasHeader** is true; **HasHeader : bool** explicitly separates a verified header fact from an unsupported guess; **VerifiedFrames : size_t** is the number of contiguous complete EDLG frames whose CRC has been checked; **VerifiedPrefixBytes : uint64_t** is the first byte after that contiguous verified prefix, never permission to append after corruption. None of the fields owns file memory or an open handle.

**Internal result `Detail::EdlsExactReadResult : uint8_t`:** **Succeeded (0)** means the EDP-Persistence FileStorage read returned success **and** exactly all requested bytes; **ProviderFailure (1)** means native ReadFileAt failed or returned a non-success status; **IncompleteRead (2)** means native read claimed success but returned a shorter transfer than required. A short read is **not** success or a clean truncated record.

**Internal helper `Detail::ReadExactEdlsBytes<TStorage>(const storage,path,offset,destination) noexcept`** borrows an existing FileStorage, immutable physical FilePathView, exact absolute byte offset and caller-owned output span; invokes native `ReadFileAt` once, checks typed status and exact transferred length, and returns one of the three values above. `TStorage` is the selected EDP-Persistence concrete. The helper does not retry indefinitely, allocate a second buffer or infer that a prior ReplaceFile was power-loss atomic.

**Public `InspectEdlsSlotWithScratch<TMaximumPayloadBytes,TStorage>(storage,path,expectedSlot,frameScratch)`** bounds one scratch payload to **1..4096** EDLG bytes plus frame overhead, validates provider/file size/header, verifies every complete frame and exact optional EDLC seal, and returns the typed immutable `EdlsSlotInspection`. `TMaximumPayloadBytes` constrains scratch and admissible record size; `TStorage` is a borrowed, application-owned read-capable EDP-Persistence provider. If the complete physical file fits the caller's scratch, one bounded read can supply all validation; otherwise a bounded streaming read path is used. If the complete bulk read fails, the scanner may reread **only** the independent 40-byte header to retain safe identity/provenance evidence, **not** to promote a partial frame to Clean. FileState is not mutated and no scan evidence survives the invocation.

**Public `InspectEdlsSlot<TMaximumPayloadBytes,TStorage>(storage,path,expectedSlot)`** uses the same exact algorithm with local statically bounded scratch for the convenience overload; high-payload WROOM workers should use caller-owned WithScratch after stack measurements. Neither overload permits forensic resynchronization or appending behind damaged data.

**Public `EdlsArchiveCondition : uint8_t`:** **Unambiguous (0)** means the fixed physical slots form a uniquely determined contiguous archive/next generation under the checked owner rules, **Ambiguous (1)** means duplicate, foreign, torn or otherwise unprovable physical history, and **GenerationExhausted (2)** means a safe next generation cannot be chosen without nonwrapping width violation. These are read-only observational classifications, **not** a generation lease.

**Public `EdlsArchiveInspection<TSlots>`** stores **Condition** as above, a bounded **Slots : array<EdlsSlotInspection,TSlots>** of independently checked per-path facts, **NewestSlot : size_t** pointing to the newest verified generation (or **TSlots** empty sentinel), and **NextGeneration : uint64_t** (zero when not safely available). The template `TSlots` is fixed at Bootstrap and must fall within 2..64; it defines the actual memory footprint and path count rather than unbounded dynamic history.

**Public `InspectEdlsArchiveWithScratch<TSlots,TMaximumPayloadBytes,TStorage>(storage,paths,scratch)`** inspects every caller-supplied canonical physical path with the source-led slot inspector, then verifies exact generation ordering/uniqueness, matching DeviceIdentifier and bounded nonwrapping successor, returning unambiguous only when every required condition is met. `paths` is the fixed lifetime-stable caller-owned array, and `scratch` is borrowed and reused **sequentially**, not retained for later publications. **`InspectEdlsArchive<TSlots,TMaximumPayloadBytes,TStorage>`** is the stack-scratch convenience wrapper and shares the same logic. All accesses must be serialized against concurrent mutation by the app's actual writer owner; otherwise a pair of read-only snapshots cannot establish a single transaction. Reinspection is needed after each publication/retirement. Source semantics and real failure-injection tests are in [EDLS inspection](../docs/EDLS-ARCHIVE-INSPECTION.MD), [strict ACK](Reference-logging-EdlsPowerLossAcknowledgement), and [G1–G15 evidence](../docs/ACCEPTANCE-G1-G15.MD). No source-level inspection guarantees physical powercut survival or Flash lifespan.

The private exact-read helper borrows the selected FileStorage provider for one positional read and requires the full destination span to be filled. It distinguishes provider rejection from a successful short transfer.
