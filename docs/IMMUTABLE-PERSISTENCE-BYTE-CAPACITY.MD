# Immutable Persistence sink byte capacity

## Consuming developer

The application selects the storage medium/provider and supplies `byteCapacity` as the trailing constructor argument to `PowerLossAcknowledgedEdls`, before `InitializeEmpty` or `Recover`. It is copied into a const 64-bit member; there is no setter, and assignment is unavailable. Reconstructing an owner is a new lifecycle and requires exclusive access and compatible persisted history.

Existing callers can omit the argument and retain the compile-time bound `40 + TPhysicalSlots * TSegmentMaximum`. This is a finite bound derived from the application's template configuration, not an unlimited default or a platform-prescribed retention size.

The budget counts all sink-owned EDLS file bytes, including each 40-byte header, any 16-byte seal, the temporary successor, and the independent 40-byte acknowledgement value. It does not count filenames, directory entries, filesystem block rounding/GC/copy-on-write metadata, NVS journal pages, application identity state, or unrelated writers. Those are provider/application physical-storage budgets. A hard physical partition quota must be established by the application/provider; generic FileStorage cannot report all such overhead.

For configured budget B and P physical slots, each segment is limited to:

`min(TSegmentMaximum, floor((B - 40) / P))`

The division reserves all P slots, including the successor. Unused space in a short segment is deliberately not borrowed by another segment: every future valid successor must fit without deleting protected history first. Up to P-1 acknowledged segments are retained; this is an upper byte bound, not a promise to fill the budget or retain a particular time window.

Budgets too small for a header and one minimally sized frame in each slot return `InvalidByteCapacity` before provider mutation. A larger attempted segment returns `ByteCapacityExceeded`. `ByteCapacity()` exposes the frozen total; `ConfiguredSegmentByteCapacity()` exposes the effective segment share.

`BoundedEdlsBatch` subtracts header/seal overhead from that share and uses the smaller of the resulting body allowance and its compile-time body capacity. It returns `FlushRequired` without consuming the next input, or `FrameTooLarge` when one record cannot fit. An exactly full body is immediately due. `ConfigureDwell` returns `InvalidByteCapacity` when the configured archive cannot fit a valid frame for the selected sealed/unsealed profile.

On restart, an existing file exceeding the share is refused without deletion. The application must not silently reduce the budget and truncate protected history. Ordinary commit ordering remains successor write/readback, protected-history verification, atomic ACK commit/readback, then oldest retirement.

Storage location remains application-owned. For substantial persisted logging in production release builds, separate storage is recommended over ESP-integrated flash. There is no operational-lifespan or flash-endurance assurance. The throughput target remains 100 records/sec; each tested configuration has its own finite qualification scope.

## Library expansion

The byte bound belongs to the existing strict archive transaction owner. No new archive, allocator, queue, worker, filesystem provider, codec or Memory operation is introduced. The additional instance state is one const uint64_t, subject to ordinary object alignment.

Both single-frame and batch publication check the bound before recovery or mutation. Recovery checks actual file sizes before removing anything; the completed-writer path checks fresh inspected segment sizes, never a cached capacity estimate. Each owned file is bounded by the reserved share, and the ACK representation is exactly 40 bytes. Thus P * share + 40 <= B, including interrupted candidates and delayed retirement.

Preserve exclusive ownership, source/scratch lifetime, original frame validation, CRC/readback, atomic watermark and no-retire-before-ACK rules when extending this path. Provider-internal physical writes are not observable through this logical byte contract. Do not promote it to a filesystem or device-wide physical quota.

## Qualification

Host coverage exercises zero/invalid budgets, exact fit, one-byte-short refusal, 100 rolls, sealed and unsealed APIs, uint64_t maximum input, incompatible pre-existing media, failed ACK, commit-then-error, recovery, immutable assignment and batch admission/deadlines at the reduced share.

The native managed-worker fixture selects 28,000 sink-owned bytes. Its test-only forwarding instrumentation independently observes all three actual file sizes before each successor write and includes the incoming candidate and ACK value in the peak. The independent raw audit verifies original records, final file sizes and native peak evidence; the existing NVS audit verifies the committed watermark.

## 31 October 2026 byte-capacity qualification

All execution was on RPI400; versions remain 0.1.0. No hosted Actions coverage is relied upon.

- Final GCC/Clang archive/batch UBSan and generated typed Memory/Threading integration: 20261031T123312-22a2d852 PASS. This also builds the actual O2/assertion-enabled/160 MHz/500 ms native profile.
- Controller and media evidence tests: 20261031T123433-767ce9f1 PASS, 134 tests, including malformed/missing/duplicate/over-limit byte witnesses.
- Three-target explicit 640-byte demo builds: 20261031T123804-946f8e52 PASS (Arduino IDE, PlatformIO Arduino, PlatformIO ESP-IDF). These are build results, not three new physical demo runs.
- Final native instrumentation build: 20261031T124551-6912a2dc PASS, actual compile profile verified.
- Final ESP32 workload plus independent raw-media/NVS/byte audit: 20261031T124610-2a2c297e PASS at 12:48:55 CET.

The ESP32 received 2,000 scheduled records at 100/s plus 128 accepted pressure records and one idle record; all 2,129 were durably completed. The configured sink budget was 28,000 B; independently observed pre-retirement peak was 27,088 B, final retained files plus ACK were 13,032 B. Final ACK35 retained 92 original records, independently checked against all fifteen source fields and original coordinates. Official NVS page/entry/blob CRC and incarnation7 checks passed. Device exited powered OFF.

Scheduled drain was 21.092329 seconds, including final flush. ACK p50 832352 us / p95 1140539 us / p99 1263239 us; maximum scheduling lateness 69854 us. Sampled minimum heap 178188 B; worker stack headroom3436 B, main4884 B. This remains finite 20-second offered-load evidence, not an indefinite-throughput or lifetime guarantee.

Factory SHA256: 6fc0ff42c7ae1bceca3686022058723c933e4ba01fc0876c56ef716cbcc6aa54.
LittleFS SHA256: e1c1cec9d0e20442b10becb709801038b8ced8d5a15220d2180d9ebd40cbaeba.
NVS SHA256: 2d12820fa3b33b54d8e3cef1ca4ce18d70f43fa8145ed2e6da17596dc7043c9e.

Earlier run123633-33f6ed8c also passed physical/raw qualification with the same production code. Review then found the new test-only observer would incorrectly count unrelated padding in the separate near-full fixture. It now matches only owned paths, and the final native run explicitly exercises exclusion of an oversized hypothetical unrelated write. No production transaction ordering changed. The final repeat above qualifies the exact corrected fixture.

Logical sink bytes include EDLS headers/seals, the temporary successor and the fixed40-byte ACK value. Filesystem allocation/GC and NVS internal metadata are separate provider budgets, not measured by this logical limit. Provider selection and physical partition allocation remain application-owned.

