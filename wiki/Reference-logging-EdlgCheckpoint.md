# EdlgCheckpoint — optional EDP-Persistence owner bridge

The source src/logging/EdlgCheckpoint.hpp is a single-frame,
single-key latest-value checkpoint. The provider and key originate
in application Bootstrap and use abstract EDP-Persistence::KeyValueStorage.
The payload MUST already be a valid exact EDLG frame. Store calls validate
checksum, length and terminal trailer before calling the provider.
Read calls report missing, provider failure, insufficient fixed caller
buffer, invalid EDLG frame or full verified recovery. Output EdlgFrameView
is published only on complete success and borrows caller's storage.

Types: EdlgCheckpointWriteStatus {Committed, InvalidFrame, FrameTooLarge,
ProviderRejected}; EdlgCheckpointWriteOutcome exposes Status and native
KeyValueStoreStatus. EdlgCheckpointReadStatus {Recovered, NotFound,
BufferTooSmall, CorruptFrame, ProviderFailure}; EdlgCheckpointReadOutcome
exposes Status, native KeyValueReadStatus and checked FrameBytes.
Methods: WriteEdlgCheckpoint<TStorage>(storage, key, frame, maxPayload)
and ReadEdlgCheckpoint<TStorage>(storage, key, destination, maxPayload,
borrowedOutput).

The bridge never selects NVS/ESP-IDF concrete, never writes EDLS historical
records, and does not claim power-loss retention absent a corresponding
provider capability. Caller serializes operations according to provider
Composition requirements, and borrowed views cannot outlive the bytes.

Tests exercise native compiler negative cases and explicitly qualified ESP32 NVS reboot readback where a suitable KeyValueStorage concrete is provided. Serial UART delivery is a separate optional provider and is **not a dependency** of this checkpoint. Payload is test-only;
The current generated, governed fifteen-field G4 LogRecord contract is implemented and tested in separate source modules; this **single-key EDLG checkpoint** remains a lower-level frame-only API and does not inspect a typed LogRecord or certify its root schema.

## 12 November 2026 source-complete current contract

**Public status enum** `EdlgCheckpointWriteStatus` values **Committed (0), InvalidFrame (1), FrameTooLarge (2), ProviderRejected (3)** distinguish exact semantic framing, configured size and native KeyValueStorage acceptance. `EdlgCheckpointWriteOutcome` holds both the semantic **Status** and the underlying **Store: KeyValueStoreStatus**, default NotReady on no attempted operation. **Committed is only provider StoreValue acceptance**, not the NVS strict durability ACK used by the separate EDLS protected rolling owner. The caller must select a KeyValueStorage concrete whose actual retention/interruption properties satisfy its application's needs.

**Public `WriteEdlgCheckpoint<TStorage>(storage,key,frame,maximumPayload)`** requires the existing app-provided EDP-Persistence KeyValueStorage `TStorage` and `KeyView`, borrowed complete EDLG source span and application-chosen maximum framed payload. It checks addition overflow and finite capacity, then uses the canonical `ReadEdlgFrame` to verify **exactly one complete CRC-clean frame** with no trailing bytes before invoking a single native `StoreValue`. It neither allocates a new NVS provider nor chooses a key or storage placement.

**Public read enum** `EdlgCheckpointReadStatus` values **Recovered (0), NotFound (1), BufferTooSmall (2), CorruptFrame (3), ProviderFailure (4)** and `EdlgCheckpointReadOutcome` fields **Status**, native **Read: KeyValueReadStatus**, and verified **FrameBytes** maintain a distinction between no entry, insufficient complete destination, corrupted frame and native failure. **`ReadEdlgCheckpoint<TStorage>(storage,key,destination,maximumPayload,output)`** borrows an existing read-capable provider, exact key, writable bounded caller storage and caller's `EdlgFrameView` output. It requires an independently declared `AvailableDataSizeIsKnown` fact and exact `BytesTransferred=AvailableDataSize` within destination; then canonical `ReadEdlgFrame` validates one and only one CRC-clean EDLG frame before **publishing output**. A failing native read may alter the destination bytes but must **not modify the caller's existing borrowed output view**. On Recovered, the view borrows `destination` and becomes invalid when it is mutated/destroyed. Every provider read/write must be serialized under its actual Bootstrap requirements. This API is a latest-key checkpoint, not a multi-generational EDLS archive, source type registry, UART transport, or a physical power-loss durability guarantee.
