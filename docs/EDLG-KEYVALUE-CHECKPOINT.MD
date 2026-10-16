# Optional EDLG KeyValueStorage checkpoint (partial archival gate)

Source: src/logging/EdlgCheckpoint.hpp (not in core umbrella).
This is a deliberately narrow opt-in bridge between existing owner facilities,
NOT a replacement for EDP-Persistence nor the approved EDLS rolling archive.

## Ownership and semantics

Bootstrap supplies one EDP-Persistence KeyValueStorage provider, its
caller-serialized invocation lifetime and one validated KeyView. Source
writer supplies an already-complete EDLG v1 frame, produced by the canonical
Logging writer. The bridge refuses bad CRC, flags, incomplete or concatenated
frames, and oversized payloads BEFORE attempting StoreValue. Exactly one
provider StoreValue/commit decides acknowledgement. A single key contains
only the latest frame; replacing it intentionally forfeits older frames.
The provider's declared retention and interruption atomicity are the ONLY
power-loss guarantee: Accepted is not power-loss-safe unless its provider
explicitly offers that property at the chosen Bootstrap Composition binding.

Recovery reads to one caller-owned fixed-capacity byte range via the
EDP-Persistence owner, rejects truncated and corrupt data, and publishes a
borrowed EdlgFrameView only after full CRC, completion and length validation.
On failure destination bytes may be modified, but output metadata is unchanged.
Caller must retain destination for the lifetime of all consumers.

No second allocator, byte-copy implementation, platform-specific concrete,
Clock, Serialisation, Mesh, runtime registry or storage provider is introduced
inside EDP-Logging. The header activates only the abstract EDP-Persistence
dependency (transitively EDP-System). NVS/ESP-IDF concrete lives solely in
the APP Bootstrap and EDP-Persistence-ESP-IDF.

## Proof and limits

GCC and Clang tests include missing, bad CRC, too-long input, oversized source,
native store failure, truncated read, native read failure, valid recovery,
damaged record and appended garbage rejection. The physical ESP32 HIL app
uses the actual EDP-Persistence-ESP-IDF NVS provider and requires a physical
runtime restart to recover previously committed test-only EDLG bytes; it also
selects EDP-Serial's abstract bounded frame queue and concrete ESP-IDF UART
writer to transmit one actual binary frame to host.

This **does not** constitute signed archival chain, multiple segment slots,
wear rotation, power-cut injection, EDLS continuity, historical schemas,
full governed LogRecord, multi-consumer retention or a flush-completed Serial
session. The source tuple and NVS namespace are HIL TEST ONLY.
