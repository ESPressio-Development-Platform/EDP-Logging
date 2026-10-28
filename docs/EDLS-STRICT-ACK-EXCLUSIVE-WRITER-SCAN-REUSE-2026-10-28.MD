# Strict durable EDLS: exclusive-writer recovery scan reuse (28 October 2026)

**Status:** First narrowly scoped optimisation implemented, GCC/Clang host PASS, actual ESP32-WROOM native performance and independent physical media audit PASS. Remaining production goals are open; all versions **0.1.0**, no automatic production-readiness claim.

## Verified problem

Independent real physical RPI400 ESP32-WROOM-32 256-record profile `20261028T173305-6d2590ff` measured 13,734 `FileStorage.ReadFileAt` calls and 7,143 `GetFileSize` calls, **87.37%** of 183,201,537us workload. Source: [EDLS provider latency report](EDLS-PUBLISH-256-RECORD-PROVIDER-LATENCY-2026-10-28.MD). A full initial `Recover()` scan is repeated unnecessarily at the start of every `Publish` after the same exclusive writer has already fully verified a successful prior transaction.

## Optimisation and strict safety boundaries

Two private trivial `PowerLossAcknowledgedEdls` fields store a **volatile hint**, NOT durable record provenance: `exclusiveWriterCompleted_` and `exclusiveWriterAck_`. The hint is set only if the same writer has successfully completed all of:
1. Replaced exactly the physically vacant successor and read back its complete CRC-qualified EDLG/EDLS segment.
2. Proved all still-protected predecessors unchanged.
3. Committed and independently read back its **PowerLoss-atomic NVS** watermark.
4. If the finite N=2 archive is full, independently verified all newest records and successfully completed oldest physical retirement/verification.

On the next caller-serialized `Publish`, the hint is invalidated **before any provider call**; a fresh NVS read must match the remembered ACK and runtime incarnation. Only the initial redundant `Recover` scan is skipped. The existing `Detail::InspectProtectedRollingPlanWithScratch` still always physically reads, checks CRC and verifies the entire protected archive **before any new file mutation**. Successor publication and readback, predecessor verification, NVS StoreValue + independent reread, and final retirement all remain unchanged. No previously cached CRC result can authorise a physical deletion. Any explicit `Recover`, `InitializeEmpty`, unsuccessful publication, uncertain state or reboot invalidates or loses the hint. Unexpected state fails closed; no attempt to extend generic VFS FileStorage's conservative Restart retention/atomicity claims.

Existing strict-ACK host negative tests were extended: direct corruption of a still-protected physical predecessor while the hint is valid prevents a new ACK and retirement; an NVS read failure is never accepted; the older torn candidate, corrupt ledger, wrong incarnation and cross-boot recovery tests remain. GCC UBSan **`20261028T175725-ec842c7c PASS`**, Clang **`20261028T175725-46bad7eb PASS`**, existing owner-generated typed CBOR -> Memory/Threading Task strict ACK GCC **`20261028T175915-1fee96fe PASS`** and Clang **`20261028T175915-53f20212 PASS`**; HIL/offline 64 tests **`20261028T175916-b38eb199 PASS`**.

## Actual physical before/after, identical hardware & 256-record test

Native ESP32-WROOM-32 ESP-IDF5.5.5 exact test-only provider proxy build **`20261028T175726-64286b00 PASS`**, factory SHA256 `01b460f4784537d8231f0b559470ae83b25a52fc60d2ce29ef68816fa7b6338c`. Actual RPI400 GPIO17-controlled physical run **`20261028T180338-4be22db3 PASS`** included fresh verified 2,097,152B full-chip backup, firmware + seeded LittleFS write/readback verification and 256 consecutive typed CBOR+EDLG durable EDLS generations with NVS ACK256 and exactly g255/g256 physically retained.

| Metric | Previous verified run | This optimised run | Observation |
| --- | ---: | ---: | --- |
| Total 256-record workload | 183,201,537us | **163,771,926us** | **10.60% less** |
| Per-`Publish` p50 | 764,260us | **677,533us** | **11.35% less** |
| Per-`Publish` p99 | 840,320us | **750,398us** | **10.70% less** |
| Throughput | 1.397 rec/s | **1.563 rec/s** | **11.88% higher** |
| `ReadFileAt` calls | 13,734 | **12,207** | **1,527 fewer** |
| `GetFileSize` calls | 7,143 | **5,869** | **1,274 fewer** |
| `Nvs.ReadValue` calls | 768 | **513** | **255 fewer** |
| `Nvs.StoreValue` calls | 256 | **256** | unchanged durable ACK boundary |
| LittleFS `ReplaceFile` | 256 | **256** | unchanged writes |
| LittleFS `RemoveFile` | 254 | **254** | unchanged retention |

Detailed measured operation times: file reads 116,406,986us; file size 24,148,002us; ReplaceFile 14,562,559us; RemoveFile 2,825,909us; NVS StoreValue 2,322,971us; NVS ReadValue 451,191us; other full-workload 3,054,308us. Timer errors **zero**; max observed LittleFS allocation 8192B; minimum free heap in **test-instrumented image** 256316B and app_main stack headroom 2080B (instrumentation adds ~33 KiB static RAM; not a production budget).

Independent **real physical raw flash audit `20261028T181035-d1bb92e5 PASS`** read physical LittleFS 0x170000/589824B SHA256 `041c88aa16bbeb7d4736a9525787bd541a8c6ade5cd2df20e2fede4c90883db5` and NVS 0x9000/24576B SHA256 `504521441b54b1f2524ee05f6827367573555168e582ac6e46d99fb3254c0014`. Independently checked actual immutable physical g255/g256, all approved 15 TypedEnvelope CBOR fields, EDLS/EDLG CRCs, and `profack` NVS watermark ACK256 incarnation7 with official page/entry/blob CRCs PASS. Private backup, UART and raw partition images remain only on RPI400.

## What remains

This first safe scan-reuse step demonstrably improves throughput, but **1.563 records/sec is still far short of the provisional 100 rec/sec target**. Per-record archive scans within the plan, successor and retirement machinery remain the dominant source of elapsed time. Next optimisation should remove further *same-transaction redundant* full scans using validated snapshot passing under strict sole-writer controls, while **always fully verifying newest protected records and NVS acknowledgement before physical oldest deletion**, and validating new file contents/readback. Must rerun all host negative tests, same 256-record physical baseline, power-loss cut at ACK boundary and raw independent NVS/LittleFS audit before treating that as production safe. Eventual segment-batched durability, acceptable latency and flash wear require separate policy decisions. Actual FreeRTOS multi-producer worker contention, near-full flash/wear and red CI issues remain open.
