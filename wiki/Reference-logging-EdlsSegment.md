# src/logging/EdlsSegment.hpp

Portable EDLS v1 archive-segment header writer and reader. One exact 40-byte header stores `EDLS` magic, version1, zero flags, LE16 physical slot, LE64 nonzero generation, 16-byte nonzero DeviceIdentifier, LE32 nonzero RuntimeIncarnation and LE32 CRC32C over the first 36 bytes.

`WriteEdlsHeader(EdlsHeader, span<byte>)` validates argument identities/capacity before touching caller storage, uses explicit LE stores and shared Logging CRC32C; `ReadEdlsHeader(span<const byte>, EdlsHeader&)` checks exact length/version/flags/CRC/identity, and leaves destination unchanged on failure.

**Not a persistence system:** does not choose slot/generation, enforce generation advancement, bind physical file paths, append encoded records, establish power-loss atomicity, or claim durability. Bootstrap and downstream EDP-Persistence providers own those operations. The new `EdlsSegmentTests.cpp` fixture has byte-for-byte parity with `tools/edp_logging/archive.py`, but both suites remain unexecuted pending appliance availability.


## Sealed content profile

Public EdlsContentProfile: Unsealed=0, SealedFrames=1 in header byte5. EdlsHeader.Profile defaults0 so prior golden bytes are identical; unknown profiles fail. EdlsSealBytes=16 covers EDLC completion, frame count, framed length and whole-segment CRC. ReadEdlsHeader checks only header; existing slot inspection checks entire seal/body. See [wire layout](../docs/BOUNDED-EDLS-BATCHING.MD).
