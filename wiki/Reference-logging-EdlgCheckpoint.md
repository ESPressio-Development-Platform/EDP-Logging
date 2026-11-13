# Reference — `src/logging/EdlgCheckpoint.hpp`

[Source](../src/logging/EdlgCheckpoint.hpp). This public opt-in adapter
stores and recovers one checked EDLG frame at an application-selected
KeyValueStorage key. The application composes the provider in Bootstrap.
The key holds the latest value. For protected multigeneration history, see
[EDLS acknowledgement and recovery](../docs/EDLS-ACKNOWLEDGED-RECOVERY.MD).

## Public write status and result

| `EdlgCheckpointWriteStatus` | Meaning |
| --- | --- |
| `Committed` | The selected provider accepted the new value. Retention and interruption behaviour follow the provider's capabilities. |
| `InvalidFrame` | The input failed strict frame validation, including an otherwise valid frame with trailing bytes. |
| `FrameTooLarge` | The configured maximum plus EDLG overhead would overflow or the offered frame exceeds that sum. |
| `ProviderRejected` | The provider returned a failure from `StoreValue`. |

`EdlgCheckpointWriteOutcome::Status` is the checkpoint result, initialized to
`ProviderRejected`. `Store` preserves the actual native `KeyValueStoreStatus`
after an attempted mutation and defaults to `NotReady` before one.

## Public read status and result

| `EdlgCheckpointReadStatus` | Meaning |
| --- | --- |
| `Recovered` | The exact stored value was received and verified as one complete frame. |
| `NotFound` | The selected key has no value. |
| `BufferTooSmall` | The reported value exceeds the destination or the transfer did not cover its reported extent. |
| `CorruptFrame` | The complete reported value is not exactly one CRC-valid EDLG frame. |
| `ProviderFailure` | The read failed, did not establish that the stored size was known, or reported more bytes transferred than the destination can hold. |

`EdlgCheckpointReadOutcome::Status` is the recovery result, initialized to
`ProviderFailure`. `Read` preserves the provider's native
`KeyValueReadStatus` and defaults to `NotReady`. `FrameBytes` is zero on
failure and holds the whole verified frame size on recovery.

## Public operations

`WriteEdlgCheckpoint<TStorage>(storage, key, frame, maximumPayload)` borrows
a live provider supporting `StoreValue`, the application key and complete
encoded frame bytes. `maximumPayload` bounds the payload in the strict
reader and bounds the full offered frame after adding the 20-byte envelope.
It verifies the extent and CRC before issuing one store at the selected key.
Any mutation or failure guarantee after that call comes from the provider.

`ReadEdlgCheckpoint<TStorage>(storage, key, destination, maximumPayload, output)`
borrows a live provider supporting const `ReadValue`, the key and writable
caller storage. It requires the provider to establish
`AvailableDataSizeIsKnown`, a complete transfer and a CRC-valid frame
within `maximumPayload`. Only `Recovered` replaces the caller's `output`
view. A failed read may change destination bytes and invalidate an earlier
view that borrowed that same storage. A newly published view remains valid
until the backing bytes are modified or destroyed.

Both calls are synchronous and take no mutex. The application serializes
access as required by the provider, keeps all borrowed resources live, and
budgets storage and time for the provider and a linear CRC scan. The adapter
makes no ISR-safety claim. The [host checkpoint tests](../tests/run-edlg-checkpoint-tests.sh)
cover exact framing and provider outcomes. See
[qualification status](../docs/LOGGING-QUALIFICATION-STATUS.MD) for native
configurations.
