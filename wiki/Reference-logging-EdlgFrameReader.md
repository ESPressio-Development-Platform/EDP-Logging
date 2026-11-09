# Reference — `src/logging/EdlgFrameReader.hpp`

**Classification:** public allocation-free, strict in-memory EDLG verifier; no EDP library dependency beyond the sibling `EdlgFrame.hpp`. [Source at reviewed commit](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/0ac9b2b4c10f31823c7834c39f2a372ca1ac98ad/src/logging/EdlgFrameReader.hpp).

## Public result enum

`EdlgFrameReadResult : uint8_t` enumerates the exact reason the candidate was rejected:

- **Succeeded:** complete header, supported codec/version/flags, length bound, GLDE marker and CRC all verified.
- **IncompleteFrame:** insufficient header or the declared complete frame has not yet arrived.
- **InvalidMagic:** first four bytes are not `EDLG`.
- **UnsupportedVersion:** framing version is not 1.
- **InvalidCodec:** codec is neither Cbor=1 nor Json=2.
- **InvalidFlags:** reserved flags are nonzero.
- **PayloadTooLarge:** advertised length exceeds caller's configured bound or cannot fit size_t plus envelope.
- **InvalidCompletion:** expected trailing `GLDE` marker is absent or damaged.
- **ChecksumMismatch:** CRC32C over header prefix + encoded payload + completion does not match the stored checksum.

These values distinguish incomplete transports from corrupt records; none triggers implicit resync or destructive repair.

## Public borrowed output `EdlgFrameView`

The struct's **Codec** identifies the accepted wire representation; **Payload** is a `span<const byte>` *borrowed* from the caller's input; **Consumed** identifies the exact prefix frame length, so a caller can advance over multiple complete frames without trusting arbitrary tails; and **Checksum** is the persisted validated CRC32C. The default instance is an empty view, not a valid decoded LogRecord. The view becomes usable **only** after `Succeeded`, and the backing source must outlive every view consumer. No output buffer or heap allocation is owned.

## Public operation `ReadEdlgFrame`

`ReadEdlgFrame(source: span<const byte>, output: EdlgFrameView&, maximumPayloadBytes: size_t)` inspects one complete frame at offset zero and leaves **output entirely unchanged on failure**. It first checks minimum header, exact magic/version/codec/flags and the bounded unsigned little-endian payload length. It then requires the 20-byte overhead plus payload, checks the completion marker and recomputes incremental CRC32C; only afterwards does it assign the verified view. It does not consume trailing bytes beyond the first verified frame and cannot salvage corrupted/torn data by scanning for another magic word.

This API performs **transport framing integrity only**: it does not attest CBOR/JSON semantics, governed 15-field diagnostic record identity, historical schema text, cryptographic authenticity, current Flash persistence or power-loss durability. A caller selecting an EDLS archive must use the appropriate separate segment inspector and strict acknowledged journal policy. See [frame writer](Reference-logging-EdlgFrame) and [normative archive rules](../docs/EDLS-ACKNOWLEDGED-RECOVERY.MD).

**Validation:** real GCC and Clang host framing/corruption tests and bounded named native integration suites are recorded in [G1–G15 acceptance](../docs/ACCEPTANCE-G1-G15.MD) and Policy/main. No generic low-level storage interruption guarantees follow.
