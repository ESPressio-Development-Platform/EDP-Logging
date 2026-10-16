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

Tests exercise native compiler negative cases and physical ESP32 NVS
reboot readback with owner Serial UART FIFO. Payload is test-only;
G4 governed LogRecord TypeIdentifier is still unissued.
