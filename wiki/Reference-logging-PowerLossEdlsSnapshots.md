# PowerLossEdlsSnapshots — bounded latest-generation profile

**Source:** [PowerLossEdlsSnapshots.hpp](../src/logging/PowerLossEdlsSnapshots.hpp)

This optional Logging adapter uses existing EDP-Persistence
`KeyValueStorage` keys with mandatory compile-time `PowerLoss`
retention **and** interruption atomicity. The ESP-IDF NVS provider has
both properties but limits each complete value to 512 bytes.

The adapter maintains two to eight CRC-checked EDLS generations,
avoids overwriting the newest, and only acknowledges writes after a
successful provider commit and byte-for-byte readback. It explicitly
retains only the **newest acknowledged generation**, not all historical
records. A corrupted inactive key degrades recovery and blocks further
writes; it does not silently trigger destructive repair.

This is the first small-record durability profile, **not** a drop-in
replacement for rolling multi-record SPIFFS archives. The complete
guarantees and HIL limits are in
[POWERLOSS-EDLS-SNAPSHOT-PROFILE.MD](../docs/POWERLOSS-EDLS-SNAPSHOT-PROFILE.MD).


## Real ESP32 verification

The native ESP-IDF NVS-backed two-key profile passed a genuine
GPIO17-controlled supply interruption during repeated atomic-key
commits. After reboot with `ESP_RST_POWERON`, recovered generation
29 was CRC clean and at least as new as the host-visible last
acknowledgement 5. Durable run `20261023T122318-b1b4e020`
PASS exit0. See the HIL README for phase boundaries, private evidence,
and strict caveats about the single cut and 512-byte/latest-only
limits.


## Implemented types, ownership and result vocabulary

This section indexes the actual current C++20 reference surface, including the
private helpers whose fail-closed behaviour is relevant when extending the
implementation. It is not a generic durability guarantee for other providers.

**`DurableEdlsStatus`** is a strongly typed `std::uint8_t` outcome. Its
explicit ordinals are: `Empty=0` (no observed generation),
`Clean=1` (the checked slot/observation is valid), `Degraded=2`
(a corrupt slot exists alongside usable history), `Ambiguous=3`
(no safe unique interpretation), `ProviderFailure=4` (native read
contract not satisfied), `InvalidRecord=5` (invalid source frame,
identity or requested generation), `NoCapacity=6` (finite output
geometry exceeded), `StorageRejected=7` (native StoreValue refused),
`ReadbackUnconfirmed=8` (committed/read value could not be verified),
`GenerationExhausted=9` (no safe next generation), and
`Committed=10` (native store plus exact readback succeeded).
`Degraded` is observable without granting permission to overwrite
or erase the damaged slot.

**`DurableEdlsObservation<TSlots>`** returns six related facts:
`Status` classifies the scan; `NewestSlot` identifies the selected
physical index or the sentinel `TSlots`; `Generation` is the
largest verified generation; `Identity` contains its 16-byte
device identity; `SlotStatus` classifies every physical key and
`SlotGenerations` stores verified per-key ordinals. A caller must
not interpret the numeric index or generation as valid merely
because the object exists; first inspect `Status`.

**`DurableEdlsCommitOutcome<TSlots>`** exposes `Status`,
the actual native `Persistence::KeyValueStoreStatus` in `Native`,
the chosen physical `Slot` (initial sentinel `TSlots`), and the
attempted `Generation`. An attempted slot/generation is not an
acknowledgement; only `Committed` establishes this operation's
readback-verified durable outcome.

**`DurableEdlsFrameReadOutcome`** contains `Status` and
`FrameBytes`. The latter reports the exact verified EDLG bytes
copied to caller storage on a clean specific-generation retrieval.
The returned frame excludes the 40-byte EDLS slot header.

## PowerLossEdlsSnapshots instance and private implementation

The template parameters are `TSlots` (2–8 distinct keys),
`TMaximumValueBytes` (at least the EDLS header plus one EDLG
frame, and at most 512), `TMaximumFramePayloadBytes` (1–4096),
and `TStorage` (application-owned concrete owner implementing
EDP-Persistence KeyValueStorage). Compile-time
`CompositionOffers::PropertiesFor<KeyValueStorage>` rejects
non-read-write providers, storage keys lacking actual PowerLoss
retention or PowerLoss interruption atomicity, and providers whose
declared maximum value is too small. Selecting a provider is
the **application's** responsibility, not a hidden Logging default.

The private `Capability` and `Properties` aliases express those
existing owner contracts. The private `ByteOperationsProvider` alias selects
the stateless Portable implementation of the EDP-Memory ByteOperations
contract. It compares equally sized physical key names and verifies the
exact value read back after commit. It copies complete frames between caller
spans and disjoint operation-local snapshots. `storage_` is a nonowning pointer to the
caller-owned provider, and `keys_` is a fixed-size collection of
nonowning key views; the caller must maintain both for the complete
adapter lifetime. The private `Slot` value groups its parsed
`Status`, `Header` (the parsed EDLS header), and exact `TotalBytes` for one
bounded key read.

The private method `IsKeySetValid()` rejects null, empty or
byte-for-byte duplicate physical keys before allowing any read or
mutation. `ReadSlot(slot,buffer)` asks the native provider to read
into caller-supplied bounded scratch; it requires a known exact
data size, a complete CRC-checked EDLS header, a matching physical
slot index, and a complete CRC-checked EDLG frame. An absent key
is `Empty`; corrupt/truncated content is `Degraded`;
incomplete native read contracts are `ProviderFailure`.
It never silently repairs, creates or deletes a key.

The adapter owns no native lock, heap allocator, timer, thread or
filesystem driver. Its method-local fixed-size arrays and any
provider-internal buffers are separate resource obligations; a
caller must provide exclusive admission/writer access, adequate
task stack, and keys that cannot be changed by other services.

## Public method contracts and failure boundaries

- **`PowerLossEdlsSnapshots(storage,keys)`:** Binds the already-live
  provider and keys without taking native ownership or opening a
  namespace. It does not check keys until the first operation.
- **`Inspect()`:** Scans each key read-only, recording every
  slot status and generation. It chooses the newest complete
  generation and rejects duplicate generations, inconsistent
  identities and unsafe key mappings as `Ambiguous`. A damaged
  inactive key can yield `Degraded` while retaining the clean
  newest record; a native read failure stays `ProviderFailure`.
  There is no automatic provenance-unsafe cleanup.
- **`RecoverNewest(destination,used)`:** Sets `used=0` first,
  invokes `Inspect`, re-reads the selected clean key and checks
  generation and identity again before copying EDLG frame bytes
  into sufficiently large caller storage. A `Degraded` scan may
  still yield a verified newest frame for observation, but must
  not be treated as authorising another `Commit`.
- **`RecoverGeneration(generation,destination)`:** Requires a
  nonzero requested generation, matches a clean physical slot
  observed by `Inspect`, independently revalidates its source
  identity/header/frame and the bounded destination capacity,
  then returns `Clean` with `FrameBytes`. A generation that
  has been overwritten or was never present produces an
  explicit failure; historical selection never grants retention
  entitlement or a new durable ACK.
- **`Commit(identity,incarnation,frame)`:** Validates finite
  capacity, complete canonical EDLG framing and usable EDLS
  identity; fails closed if the existing state is ambiguous or
  degraded. It reserves an inactive/empty key, never the
  latest acknowledged key. It assigns a nonwrapping generation,
  writes the complete header and frame through native
  `StoreValue`, then independently calls `ReadSlot` and
  compares every physical byte. Only when both phases succeed
  is `Committed` returned. Interrupted or rejected writes may
  leave an unknown inactive slot; the next observation must
  classify it conservatively, not infer that all key operations
  were atomic unless the selected owner truly advertises
  PowerLoss atomicity.

These guarantees are narrower than an application-level file archive
with a separate NVS ACK watermark: this adapter keeps the latest
readback-verified small key-value generation, not an arbitrary
rolling multi-record history. See the bounded archive and strict
acknowledgement references for those other profiles.
