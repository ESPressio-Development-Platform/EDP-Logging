# Reference — `src/logging/EdlgFrame.hpp`

**Classification:** public allocation-free binary framing API, internal frame wire helpers. [Source at reviewed commit](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/d6056ed0e463e05d512f90283bc13fe405a71ec6/src/logging/EdlgFrame.hpp). This header has **no EDP library dependency**. It writes an envelope around *already encoded* bytes; the caller, not this frame layer, owns the EDP-Serialisation writer and governed diagnostic schema.

## Public enums and semantics

`EdlgCodec : uint8_t` offers **Cbor** (=1, payload bytes are externally encoded CBOR) and **Json** (=2, payload bytes are externally encoded JSON); it does not validate either representation. The `EdlgFrameWriteResult` values are **Succeeded** (complete frame and `written` published), **InvalidCodec** (unsupported enum value), **InvalidPayload** (invalid length/bounds, invalid nonempty pointer or forbidden overlap), and **InsufficientCapacity** (destination cannot hold the 20-byte envelope plus encoded payload). A failure never partially claims a successful frame.

The [frame writer](Reference-logging-EdlgFrameWriter) is an opt-in header.
The codec tags, write status vocabulary and CRC/wire utilities reside here
so the frame reader and umbrella can be used without EDP-Memory.

`Detail::DoEdlgByteRangesOverlap(source, destination)` is the internal
predicate used before framing or archive scratch reuse. It checks the two
nonempty spans with address subtraction on the supported linear address
model and reads no bytes from either range.
