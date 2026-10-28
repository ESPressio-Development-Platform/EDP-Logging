# EDLS strict durable-ACK `Publish()` bottleneck — ESP32-WROOM-32 physical evidence (28 October 2026)

**Status:** Measurement COMPLETE, hardware + independent raw-media audit PASS; optimisation NOT yet implemented; all EDP versions remain **0.1.0**. Does **not** declare Logging production ready, 100 records/sec achieved, or generic LittleFS FileStorage PowerLoss atomicity.

## Original problem and experimental control

The earlier uninstrumented, ESP32-WROOM-32 ESP-IDF 5.5.5 test performed 256 individually durable ACKed owner-generated 15-field CBOR records through immutable 2+1 EDLS/LittleFS and independently PowerLoss-atomic NVS KeyValueStorage. It measured 184,292,842 µs overall, 767,372 µs median, 850,282 µs p99, ~1.39 records/sec, retained g255/g256 and NVS ACK256, independently raw-media verified. See `docs/WROOM-RESOURCE-QUALIFICATION.MD` and Policy/main handoff.

For the new test, RPI400 was explicitly authorised and exclusively locked under `gpt6-logging-profile-20261028-1724`. Source was isolated in a new scratch worktree copied exactly from the previously tested `feature/logging-v1` Git tree `93fee4b775d24441034097d9e86ea5772d9cee9a`. **Production `PowerLossAcknowledgedEdls::Publish` and both concrete Persistence drivers were left unmodified.** Test-only nonallocating, caller-serialised, real provider transparent proxies measured microsecond elapsed time of each `FileStorage::GetFileSize`, `ReadFileAt`, `ReplaceFile`, `RemoveFile`, `KeyValueStorage::GetValueSize`, `ReadValue`, `StoreValue` and `RemoveKey` call, with bounded per-generation counters; existing real interfaces and compile-time KeyValue PowerLoss properties were preserved. The timer was enabled **only inside each `Publish`**, explicitly excluding periodic test-only `VerifyTypedArchive` / `archive.Read`. No UART output occurs during each measured `Publish`; all counters are emitted after the 256th commit.

RPI400 native build: first `20261028T172539-97854626` **FAIL** because test-only NVS proxy failed to forward required advertised `ClearAllKeys`; corrected without modifying production providers, subsequent `20261028T172927-745a4ba0` **PASS**, ESP-IDF 5.5.5 factory image SHA256 `5755c3af0e5b04814cb8f1f97c180d2876615b93eea02f8a6559a603cb0f704c`. 64 historical HIL negative/forensic Python tests `20261028T172959-37245c89` **PASS**; actual strict EDLS VFS+KeyValueStorage host GCC test `20261028T172959-a47b1b24` **PASS**.

## Actual instrumented 256-record physical results

RPI400 GPIO17-controlled integrated WROOM-32, verified 2 MiB full-chip pre-test backup, exact ESP-IDF factory firmware write/readback and 589824-byte seeded LittleFS write/readback. Native physical 256-ACK run **`20261028T173305-6d2590ff PASS exit0`**:

- 256 strict `Publish()` complete durable ACKs, final retained EDLS generations g255+g256.
- Total workload **183,201,537 µs**, median strict `Publish` **764,260 µs**, p95 **840,300 µs**, p99 **840,320 µs**, worst **855,887 µs**, approximately **1.397 individually ACKed records/s**.
- Minimum sampled free heap **256,316 bytes** and minimum sampled main-task remaining stack **2,096 bytes** (includes approximately 33 KiB of test-only timing matrices, so cannot compare absolute memory directly to uninstrumented product use). Maximum sampled LittleFS allocated **8,192 bytes**; this is NOT a flash-wear measure.

| Actual measured operation | Calls (256 records) | Calls per record | Wall time | Share of total workload | Per-generation p50 |
|---|---:|---:|---:|---:|---:|
| **LittleFS ReadFileAt** | **13,734** | **53.65** | **130.563 s** | **71.27%** | **536.013 ms** |
| **LittleFS GetFileSize** | **7,143** | **27.90** | **29.492 s** | **16.10%** | **115.231 ms** |
| LittleFS ReplaceFile | 256 | 1.00 | 14.102 s | 7.70% | 13.408 ms |
| LittleFS RemoveFile | 254 | 0.99 | 2.868 s | 1.57% | 11.090 ms |
| **NVS StoreValue (set_blob + commit)** | **256** | **1.00** | **2.353 s** | **1.28%** | **8.467 ms** |
| NVS ReadValue | 768 | 3.00 | 0.652 s | 0.36% | 2.581 ms |
| NVS GetValueSize / RemoveKey | 0 | 0.00 | 0 s | 0.00% | 0 ms |
| All other time including CBOR encoding, caller validation and test overhead | — | — | 3.172 s | 1.73% | not separately isolated |

Provider timer sum **180,029,765 µs** = **98.26%** of the complete 183,201,537 µs workload. There were **zero timer errors**. The final 3.172 seconds is **NOT** strictly CPU-only `Publish` overhead because total workload starts before producer projection/CBOR encoding and includes periodic independent test validation; per-provider timings exclude those activities.

Additional finding: `ReplaceFile` has a **max call latency of 283.842 ms** and per-generation p99 ~280.560 ms despite a median of 13.408 ms, suggesting episodic flash filesystem write/metadata/GC work; the current evidence does not distinguish GC from wear levelling or individual VFS steps. NVS write+commit is **not** the dominant cause of the observed 767 ms median.

## Independent physical forensic qualification

Separate raw-media test **`20261028T174032-c580c906 PASS exit0`** read actual LittleFS 0x170000+0x90000 SHA256 `77c6e19b1737c1be038c923c48c691481befde55932080eba8d83e6631eb4f88`, and real NVS 0x9000+0x6000 SHA256 `504521441b54b1f2524ee05f6827367573555168e582ac6e46d99fb3254c0014`. The independent immutable EDLS/EDLG CRC and owner-approved 15-field CBOR typed envelope parser found physically retained g255/g256, all fields and identity intact. Official Espressif NVS parser found namespace `profack`, ACK **256**, incarnation **7**, and **PASS** page/entry/blob CRCs. No raw image or full-chip backup is published to GitHub. Firmware restored OFF GPIO17 HIGH after test; lock and LED release must be verified separately at session end.

## Causal interpretation and design consequences

The dominant **measured operation** is repeated file reads and size queries: **21,877 calls across 256 records**, **87.37%** of total wall time. Current immutable-pair implementation intentionally rechecks the same CRC-qualified records multiple times in `Recover`, `InspectProtectedRollingPlanWithScratch`, `PublishEdlsSuccessorIntoEmptySlotWithScratch`, `ConfirmProtectedRollingPredecessorsWithScratch` and `FinishEdlsProtectedRollingRetirementWithScratch`. The timing identifies the **filesystem read/metadata path**, not specifically whether the underlying per-call latency comes from LittleFS access, the VFS bridge, CRC checks (outside provider duration), flash controller, or GC. Revalidating everything blindly on each record is costly.

**Recommended next optimisation order (no implementation authorised by this measurement alone):**

1. Quantify and remove **redundant same-transaction** scans. Carry forward CRC-qualified immutable slot observations *inside an exclusive worker transaction* and detect any unexpected provider mutation. Full re-scan after crash, ownership uncertainty, failure, reboot or when a protected predecessor is about to be retired remains mandatory. No weakening of integrity guarantees.
2. Preserve exact ACK ordering: successor fully written/readback -> predecessor proof -> **separately PowerLoss-atomic NVS watermark committed and reread** -> only then retire physically oldest. Do not use a cached observation to claim flash power-loss atomicity.
3. Re-measure identical 256-record hardware workload after each narrow change, including GPIO17 crash-window and independent raw NVS+LittleFS audits, to prove both durability and achieved speed. Use EDP-Profiling for per-method high-frequency CPU attribution later if necessary.
4. **100 records/sec is NOT likely from scan removal alone.** Holding non-read/size work constant gives a best-case approximately **90 ms per individually acknowledged record**, about 11.1 records/sec, before further write-path optimisation. Immutable segment grouping, logging policy, or a different bounded physical write strategy may be necessary for 100 sustained rec/sec, and require explicit durability/retention/endurance acceptance before implementation.

All figures are one controlled 256-record WROOM physical workload, not a general workload SLA, WROOM flash endurance study, active Mesh scheduler benchmark, or CI-pass evidence.
