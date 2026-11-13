# Reference — `src/logging/EdlgFrame.hpp`

[Source](../src/logging/EdlgFrame.hpp). This header defines the public
EDLG codec and write-result vocabulary and the internal wire helpers shared by
the reader and writer. It depends on the C++ standard library. The application
supplies bytes encoded by its selected serialisation writer.

## Public enums and semantics

`EdlgCodec : uint8_t` offers **Cbor** (=1, payload bytes are externally encoded CBOR) and **Json** (=2, payload bytes are externally encoded JSON); it does not validate either representation. The `EdlgFrameWriteResult` values are **Succeeded** (complete frame and `written` published), **InvalidCodec** (unsupported enum value), **InvalidPayload** (invalid length/bounds, invalid nonempty pointer or forbidden overlap), and **InsufficientCapacity** (destination cannot hold the 20-byte envelope plus encoded payload). A failure never partially claims a successful frame.

The [frame writer](Reference-logging-EdlgFrameWriter) is an opt-in header.
The codec tags, write status vocabulary and CRC/wire utilities reside here
so the frame reader and umbrella can be used without EDP-Memory.

`Detail::DoEdlgByteRangesOverlap(source, destination)` is the internal
predicate used before framing or archive scratch reuse. It checks the two
nonempty spans with address subtraction on the supported linear address
model and reads no bytes from either range.


`Detail::EdlgFrameCrc32C(bytes, previous = 0U)` is an internal helper shared
by the framing reader and writer. It computes reflected Castagnoli CRC32C.
Pass zero for the first block and the returned finalized value for each
subsequent block. The header prefix, payload and completion marker can thus
be processed without allocating contiguous temporary storage.

`Detail::WriteEdlgLe32(output, offset, value)` is an internal writer helper.
It stores an unsigned 32-bit value as four little-endian bytes. Its caller
must establish that the range at `offset` is writable. It changes only those
four bytes.
