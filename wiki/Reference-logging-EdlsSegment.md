# Reference — `src/logging/EdlsSegment.hpp`

**Classification:** PUBLIC C++20 versioned EDLS wire-header contract; PRIVATE wire helpers. [Exact source](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/d6056ed0e463e05d512f90283bc13fe405a71ec6/src/logging/EdlsSegment.hpp). This header includes the sibling EDLG CRC helper and standard C++ headers only. It does not import EDP-Persistence, select Flash storage or perform I/O.

## Public constants, enums and values

`EdlsHeaderBytes = 40` fixes the exact version-one header length. `EdlsSealBytes = 16` identifies the separately inspected sealed-content completion footer; a header read does **not** validate that footer or the EDLG body.

`EdlsContentProfile : uint8_t`: **Unsealed (0)** represents the existing version-one unsealed segment; **SealedFrames (1)** means the archive content is subject to complete-frame count/length and whole-segment CRC sealing. Other values are invalid. Profile selection is not a claim that underlying FileStorage becomes interruption-atomic.

`EdlsHeaderStatus : uint8_t` has individually meaningful values: **Succeeded (0)**, header written/verified; **InvalidArguments (1)**, encoding rejects invalid nonzero identity/generation/runtime or unsupported profile; **InsufficientCapacity (2)**, destination cannot hold the 40-byte header; **MalformedHeader (3)**, reader found invalid layout, length, flags/profile or decoded identity; **UnsupportedVersion (4)**, not EDLS version one; **ChecksumMismatch (5)**, header CRC mismatch. Invalid source cannot be silently “repaired” or converted into a successful header.

## Public `EdlsHeader` fields

`Slot : uint16_t` is the physical slot ordinal assigned by the caller's archive path map (default 0, not intrinsically proof of an existing slot). `Generation : uint64_t` is the externally allocated nonzero, nonwrapping EDLS generation; it is authoritative only after the containing archive manager checks candidate ordering. `DeviceIdentifier : array<byte,16>` is an immutable identity in the wire header, required to contain at least one nonzero byte; zero/unknown device identity cannot be published as a valid segment. `RuntimeIncarnation : uint32_t` is the nonzero deployment/incarnation discriminator. `Profile : EdlsContentProfile` defaults to Unsealed for backward-compatible version-one golden bytes. The Type itself stores only values, no dynamic storage or pointer ownership.

## Public operations

`WriteEdlsHeader(const EdlsHeader& header, span<byte> destination)` checks the nonzero generation, nonzero incarnation, nonzero device identity, supported profile and minimum destination capacity **before writing any destination byte**. On success it emits `EDLS`, version one, profile, little-endian Slot and Generation, 16 raw identity bytes, little-endian RuntimeIncarnation, and a CRC32C over the first 36 bytes at byte offset 36. Extra destination space is not part of the header. No generation is allocated here, and there is no durability ACK.

`ReadEdlsHeader(span<const byte> source, EdlsHeader& destination)` accepts **exactly 40 bytes**; checks magic, version, profile, stored CRC and decoded nonzero identity; assigns a temporary parsed header into `destination` **only on success**. The reader cannot determine whether `Slot` actually matches a physical path or `Generation` belongs to a contiguous retained archive; those are explicit higher-level inspector obligations. The caller's input must remain available for this synchronous call, but no view is retained afterwards.

## Private implementation helpers

`Detail::WriteEdlsLe(destination, offset, value, width)` encodes a selected number of little-endian bytes into already validated destination bounds; `Detail::ReadEdlsLe(source, offset, width)` parses an unsigned little-endian scalar from already validated source bounds. They do not themselves check dynamic span capacity and are not supported independent API. `Detail::IsValidEdlsIdentity(header)` rejects zero generation, zero runtime incarnation and an all-zero 16-byte DeviceIdentifier; it does not authenticate identities or establish clock correlation.

## Constraints and validation

CRC32C detects accidental corruption, **not** a malicious modification or successful physical persistence. Individual EDLG frames, sealed EDLC footer, protected N+1 lifecycle, KeyValueStorage watermark and actual PowerLoss-qualified concrete belong to separate owners. Real GCC/Clang byte-for-byte tests and Python parity checks, along with bounded electrical qualification of the separately composed archive, are recorded in [G1–G15 acceptance](../docs/ACCEPTANCE-G1-G15.MD); no generic VFS FileStorage interruption-atomic guarantee is inherited.
