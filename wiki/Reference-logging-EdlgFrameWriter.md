# logging/EdlgFrameWriter.hpp

**Source:** [EdlgFrameWriter.hpp](../src/logging/EdlgFrameWriter.hpp). This
public opt-in writer composes the existing EDP-Memory ByteOperations
capability with the EDLG format.

## Public operation `WriteEdlgFrame`

`TByteOperationsProvider` must satisfy the EDP-Memory ByteOperations
provider traits. The synchronous operation borrows the selected provider and
the caller's buffers for the duration of the call.

| Parameter | Contract |
| --- | --- |
| `codec` | Selects CBOR or JSON for the frame header. |
| `payload` | Borrows the already encoded bytes. It can be empty. |
| `output` | Borrows writable frame storage. This span must be disjoint from the payload. |
| `written` | Receives the complete frame length on success and retains its prior value on failure. |
| `maximumPayloadBytes` | Sets the finite payload limit for the selected application profile. |
| `byteOperations` | Supplies the non-overlapping copy operation for a nonempty payload. The writer retains no provider reference. |

The writer checks the codec, nonempty payload pointer, configured and wire
length limits, output capacity and overlap before changing the destination.
See the [write-result vocabulary](Reference-logging-EdlgFrame) for the result
of each check.

On success it writes the 16-byte EDLG v1 header, the supplied payload and
the four-byte GLDE completion marker. The header carries the codec, zero
flags, a little-endian 32-bit payload length and CRC32C. The checksum covers
the header prefix, payload and completion marker. Only the payload reaches
the provider. `written` is the payload size plus 20. Bytes after the complete
frame remain available to the caller.

The writer uses bounded caller storage and takes no mutex. It can run
concurrently on disjoint output buffers when the chosen provider permits
concurrent use. The call does not establish an ISR-safe contract. Success
establishes a complete in-memory frame. Application code separately owns
payload validation, transport and durable acknowledgement.

## Validation and reading

The [frame reader](Reference-logging-EdlgFrameReader) verifies a borrowed
frame. GCC and Clang tests cover the reference vector, capacity checks,
overlap refusal and corruption. Native UART and archive receipts have their
own measured configurations in [G1–G15 acceptance](../docs/ACCEPTANCE-G1-G15.MD).
