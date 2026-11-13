# Reference — `src/logging/EdlgFrame.hpp`

**Classification:** public allocation-free binary framing API, internal frame wire helpers. [Source at reviewed commit](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/d6056ed0e463e05d512f90283bc13fe405a71ec6/src/logging/EdlgFrame.hpp). This header has **no EDP library dependency**. It writes an envelope around *already encoded* bytes; the caller, not this frame layer, owns the EDP-Serialisation writer and governed diagnostic schema.

## Public enums and semantics

`EdlgCodec : uint8_t` offers **Cbor** (=1, payload bytes are externally encoded CBOR) and **Json** (=2, payload bytes are externally encoded JSON); it does not validate either representation. The `EdlgFrameWriteResult` values are **Succeeded** (complete frame and `written` published), **InvalidCodec** (unsupported enum value), **InvalidPayload** (invalid length/bounds, invalid nonempty pointer or forbidden overlap), and **InsufficientCapacity** (destination cannot hold the 20-byte envelope plus encoded payload). A failure never partially claims a successful frame.

## Public operation `WriteEdlgFrame`

Parameters: `codec` is the chosen canonical representation; `payload: span<const byte>` borrows already encoded bytes; `output: span<byte>` is the application-owned writable frame storage; `written: size_t&` is the result/output length and is **unchanged on all preflight failures**; `maximumPayloadBytes` is the application's finite configured profile bound. The caller must keep source and destination **nonoverlapping**. The operation rejects invalid codec, null nonempty payload, a payload greater than either the configured bound or the 32-bit wire length, an undersized output span, and overlapping spans before writing.

On success it writes the fixed **16-byte EDLG header** (magic, version 1, codec, zero flags, little-endian 32-bit payload size, CRC32C), exactly the supplied encoded payload, and **4-byte GLDE** completion marker. `written = payload.size() + 20`. There is no heap allocation, task, OS time, FileStorage or implicit mutex. A successful frame write is a complete in-memory representation; it is **not** a sink ACK, FileStorage transaction, power-cut-proof commit, authenticated record or semantic LogRecord validation.

## Private wire helpers

`Detail::EdlgFrameCrc32C(bytes, previous=0)` performs incremental reflected Castagnoli CRC32C using the finalized previous-block accumulator, without a lookup table or extra allocation. `previous` defaults to zero for the first block. The checksum detects accidental corruption, not active tampering, and its representation must remain stable across hosts/architectures. `Detail::WriteEdlgLe32(output, offset, value)` writes four explicit little-endian bytes; the caller supplies sufficient destination space after preflight. Both helpers are internal implementation details, not supported independent providers.

## Validation and reading

`ReadEdlgFrame` in [EdlgFrameReader](Reference-logging-EdlgFrameReader) is the separate strict borrowed verifier; it does not decode CBOR/JSON. The current source is covered by real GCC and Clang native host tests (including reference vector, capacity, corruption) and has participated in bounded ESP32 integration as documented in [G1–G15 acceptance](../docs/ACCEPTANCE-G1-G15.MD). Earlier historical statements that the tests were “NOT RUN” are superseded by the November 9, 2026 evidence (including the 68/68 host suite). Do **not** generalize this into an arbitrary VFS interruption-atomicity guarantee.
