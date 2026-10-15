# logging/EdlgFrame.hpp

**Status:** public C++20 transport-framing primitive; source-only, not yet runtime validated.

`EdlgCodec` has CBOR=1 and JSON=2; `EdlgFrameWriteResult` distinguishes Succeeded, InvalidCodec, InvalidPayload and InsufficientCapacity.

`WriteEdlgFrame(codec, payload, output, written, maxPayloadBytes)` emits a D13-defined `EDLG` binary frame with 16-byte header and 4-byte completion marker, using only caller-owned `std::span` storage. It preflights codec, configured payload limit, 32-bit wire size and destination capacity before writing, and does not change `written` on failure. Borrowed source and writable output must not overlap.

`Detail::EdlgFrameCrc32C` implements reflected Castagnoli CRC32C with incremental finalized accumulator, with no separate heap or state. `Detail::WriteEdlgLe32` explicitly writes LE32. The transport checksum detects accidental corruption only; it gives neither cryptographic authentication nor power-loss durability.

Test `tests/EdlgFrameTests.cpp` compares one 23-byte reference vector (`EDLG`, version1, CBOR, LE32 length3, CRC `F57F6380`, payload `abc`, `GLDE`), and confirms capacity failure does not mutate output. Tests have **NOT RUN** in an authorised execution environment.

## Strict native read-only frame inspection

`EdlgFrameReader.hpp` adds `ReadEdlgFrame`, a zero-allocation bounded, non-resynchronizing validator. It exposes a `EdlgFrameView` only after magic/version/codec/flags/length/completion/CRC all agree, and returns precise consumed byte count. Source backing must outlive the returned payload view. Invalid or torn frame content never yields partial/updated output. The helper is not a semantic CBOR decoder or archive durability provider; both tests and target compilation remain pending.
