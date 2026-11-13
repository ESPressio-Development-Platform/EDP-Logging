# logging/EdlgFrameWriter.hpp

**Source:** [EdlgFrameWriter.hpp](../src/logging/EdlgFrameWriter.hpp). This
public opt-in writer composes the existing EDP-Memory ByteOperations
capability with the EDLG format.

## Public operation `WriteEdlgFrame`

Parameters: `codec` is the chosen canonical representation; `payload: span<const byte>` borrows already encoded bytes; `output: span<byte>` is the application-owned writable frame storage; `written: size_t&` is the result/output length and is **unchanged on all preflight failures**; `maximumPayloadBytes` is the application's finite configured profile bound. The caller must keep source and destination **nonoverlapping**. The operation rejects invalid codec, null nonempty payload, a payload greater than either the configured bound or the 32-bit wire length, an undersized output span, and overlapping spans before writing.

On success it writes the fixed **16-byte EDLG header** (magic, version 1, codec, zero flags, little-endian 32-bit payload size, CRC32C), exactly the supplied encoded payload, and **4-byte GLDE** completion marker. `written = payload.size() + 20`. There is no heap allocation, task, OS time, FileStorage or implicit mutex. A successful frame write is a complete in-memory representation; it is **not** a sink ACK, FileStorage transaction, power-cut-proof commit, authenticated record or semantic LogRecord validation.

## Private wire helpers

`Detail::EdlgFrameCrc32C(bytes, previous=0)` performs incremental reflected Castagnoli CRC32C using the finalized previous-block accumulator, without a lookup table or extra allocation. `previous` defaults to zero for the first block. The checksum detects accidental corruption, not active tampering, and its representation must remain stable across hosts/architectures. `Detail::WriteEdlgLe32(output, offset, value)` writes four explicit little-endian bytes; the caller supplies sufficient destination space after preflight. Both helpers are internal implementation details, not supported independent providers.

## Validation and reading

`ReadEdlgFrame` in [EdlgFrameReader](Reference-logging-EdlgFrameReader) is the separate strict borrowed verifier; it does not decode CBOR/JSON. The current source is covered by real GCC and Clang native host tests (including reference vector, capacity, corruption) and has participated in bounded ESP32 integration as documented in [G1–G15 acceptance](../docs/ACCEPTANCE-G1-G15.MD). Earlier historical statements that the tests were “NOT RUN” are superseded by the November 9, 2026 evidence (including the 68/68 host suite). Do **not** generalize this into an arbitrary VFS interruption-atomicity guarantee.


`TByteOperationsProvider` must satisfy the EDP-Memory provider traits.
`byteOperations` is borrowed for the duration of this synchronous call.
Only a nonempty payload reaches `CopyBytes`, after all codec, size and
disjoint-buffer checks have passed. Header fields and the trailer are
encoded at their fixed wire offsets. The provider receives the exact
payload span and no retained reference. This keeps CRC, overlap refusal and
`written` behaviour independent of the selected concrete copy provider.
