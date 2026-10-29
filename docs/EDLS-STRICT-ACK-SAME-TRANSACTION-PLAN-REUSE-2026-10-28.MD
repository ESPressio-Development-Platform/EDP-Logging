# EDLS strict durable ACK — same-transaction archive plan reuse (28 October 2026)

**Status:** second narrowly scoped scan optimisation independently measured PASS on ESP32-WROOM-32, with real LittleFS/NVS forensic readback PASS. First preceding optimisation was published as `423525a8b275ef8687396ed8ae4331884141fb1c`. All package versions remain **0.1.0**, generic EDP-Persistence FileStorage PowerLoss guarantees remain conservative, and complete Logging production acceptance remains OPEN.

## Change and boundaries

The strict `PowerLossAcknowledgedEdls::Publish` already constructs a complete, CRC-verified, physically absent-vacant-slot `Detail::InspectProtectedRollingPlanWithScratch` for the same sole owner transaction, **before writing any successor**. The existing generic `PublishEdlsSuccessorIntoEmptySlotWithScratch` duplicated the entire archive `InspectEdlsArchiveWithScratch` immediately afterward. Now the successor helper accepts an **optional pointer** to an already fully inspected `EdlsArchiveInspection<TSlots>` from the immediately preceding same-transaction owner. **All normal/legacy calls default to a fresh full scan**, unchanged. The strict writer uses `prior.Inspected` and still verifies that the archive is unambiguous, matches NVS ACK + expected generation and has a physically empty destination. Its independent final size/vacancy check and complete new-frame CRC readback remain. After successor write the strict writer also explicitly checks `posted.Generation == ack+1`, slot within bounds and that the original verified plan marked that slot Empty **before accepting or committing an ACK**. All previously protected predecessor readbacks, NVS StoreValue+independent NVS reread, and full pre/post-retirement checks are unchanged. This is not a cached observation across separate publications; no snapshot is used to justify irreversible oldest retirement.

The earlier completed-exclusive-writer hint also remains: it skips ONLY the initial duplicate Recover scan after the same owner fully completed the previous strict transaction, but each subsequent physical plan remains fully CRC-qualified. On restart or any uncertain outcome it is invalidated and full `Recover` runs.

## Host and real native evidence

- GCC UBSan strict ACK/recovery + corrupted protected slot + bad NVS + crossboot + torn candidate: **`20261028T181606-a5221284 PASS exit0`**.
- Clang strict ACK/recovery same suite: **`20261028T181606-ff205989 PASS exit0`**.
- Owner-generated 15-field TypedEnvelope via actual EDP-Memory/EDP-Threading Task strict ACK: GCC **`20261028T181632-416824df PASS`**, Clang **`20261028T181633-89977fb2 PASS`**.
- Exact ESP32-WROOM-32 native ESP-IDF 5.5.5 profiling build **`20261028T181632-86b39cf7 PASS exit0`**, SHA256 factory image `d0c439992533dd652de0d2f14975ff6426783cc2a5dad6f848582532e17a54ca`.
- Actual **RPI400 GPIO17-controlled physical** 256-record same strictly durable-ACKed 15-field CBOR benchmark **`20261028T181729-832f5ea6 PASS exit0`**: fresh 2MiB full-chip backup, exact factory image and 589824B LittleFS seed flash/readback verify, full 256 durable NVS ACKs, final physical g255/g256, memory stable. Real provider nonallocating instrumentation emits only after workload, no serial output inside measured `Publish`.
- Separate independent **physical raw LittleFS+NVS flash audit `20261028T182416-3a1f0d90 PASS exit0`**: actual LittleFS 0x170000/589824B SHA256 **`8c49dbdbba068e5843fbc24bd4e1443e9db1059b35ace895acd6077847e5d940`**, exactly physical g255/g256 with all approved 15 typed CBOR permanent fields, full EDLS/EDLG CRC. Actual NVS 0x9000/24576B SHA256 **`504521441b54b1f2524ee05f6827367573555168e582ac6e46d99fb3254c0014`**, namespace `profack` ACK **256**, incarnation7, official Espressif page/entry/blob CRC PASS. Private raw files, backup and serial only on RPI400.
- Electrical ACK-gap power-cut regression of exact new source remains a separate test stage: see current Policy living handoff for test receipt; do NOT claim it passed until observed.

## Physical performance comparison

| Metric | Original without optimisations | After prior completed-writer hint | **Both changes (now)** |
|---|---:|---:|---:|
| Total for 256 records | 183,201,537us | 163,771,926us | **147,312,921us** |
| Durable per-record p50 | 764,260us | 677,533us | **607,458us** |
| Durable p99 | 840,320us | 750,398us | **670,479us** |
| Sustained throughput | 1.397 rec/s | 1.563 rec/s | **1.738 rec/s** |
| `ReadFileAt` calls | 13,734 | 12,207 | **10,680** |
| `GetFileSize` calls | 7,143 | 5,869 | **5,101** |
| `Nvs.StoreValue` | 256 | 256 | **256** |
| `FileStorage.ReplaceFile` | 256 | 256 | **256** |
| `FileStorage.RemoveFile` | 254 | 254 | **254** |

Same-task latest operation times: 102,689,542us in 10,680 file reads (69.71% of 147,312,921us); 21,176,772us in 5,101 size probes (14.38%); 14,698,308us in 256 ReplaceFile writes; 2,830,565us in 254 RemoveFile; 2,423,067us in 256 NVS StoreValue incl commit; 459,220us in 513 NVS reads. Total instrumentation inside real provider calls **144,277,474us**, rest of full workload **3,035,447us** (includes projection/test validation and cannot be interpreted solely as CPU inside Publish); timer errors 0. The second isolated change cuts 70ms more off p50 vs first scan-reuse alone; cumulative **20.52% lower p50** and approximately **24.4% higher throughput** vs original.

## Explicit unresolved acceptance gates

Even with two independent proven safe narrow scan-reuse steps, **~1.74 individually NVS-committed immutable records/sec** is nowhere near provisional 100 rec/sec. The critical residual cost remains repeated physical archive reads/stat probes for protected-history checks and the generic retirement flow; per-record physical immutable file creation+verified readback+NVS commit also imposes a nonzero lower bound. Do not remove the complete independent CRC qualification of the *newly written* candidate, nor protected pre-retirement readbacks, nor invert the NVS-before-oldest-deletion order. Any more aggressive owner-level verified-plan reuse, segment batching or persistent record ACK optimisation needs separate physical crash-window, near-full and endurance tests as well as full Logging worker-scheduling/backpressure acceptance. CI remains red/inconclusive due absent step logs. No Mesh change and all versions 0.1.0.

## 29 October 2026 — actual GPIO17 VDD interruption and separately audited physical media

Exact updated native ESP32-WROOM-32 ESP-IDF 5.5.5 strict typed ACK-gap firmware build `20261028T182622-6445ca2c` **PASS exit 0**, factory image SHA256 `6944b767a85069862b773956f1d0e9a4895da414d32e41f9ced9f9406e28f587`.

Physical run `20261029T092612-210bfb6c` **PASS exit 0**, with fresh verified 2 MiB full-chip backup, verified firmware and dedicated seeded LittleFS, generation 1 and 2 successfully NVS-acknowledged, generation 3 **fully written but not acknowledged** (`nvs_ack=2`), then an externally confirmed **8.258-second GPIO17 electrical power cut**. Recovery from a new boot did not treat the unacknowledged candidate as committed; it verified protected history and published new durable g3/g4. Controller classification `EXTERNALLY_VERIFIED_POWER_CUT|accepted=True`, application reset=10 / ROM SDIO_RESET (not falsely labelled power-on-reset). End power OFF GPIO17 HIGH.

Independent raw physical flash audit `20261029T100105-8e1a987f` **PASS exit 0**, actually read *after* that cut from LittleFS 0x170000 size 589824 bytes SHA256 `ccfbfabdabdff5e5adba2f345fdc2324e7ff886de1413524d8e25ffa3646adf9`; full typed canonical 15-field record EDLS/EDLG CRC verification `OFFLINE_TYPED_EDLS_AUDIT=PASS oldest=3 newest=4 fields=15`. Independently read actual NVS 0x9000 size 24576 bytes SHA256 `cb8f517789ad5357f2847c794b95c969af2537219c4edd853570e0bc325fc20d`, official Espressif NVS CRC checks `OFFLINE_REAL_NVS_WATERMARK_AUDIT=PASS ack=4 last_incarnation=7 ns=typedack page_crc=PASS entry_crc=PASS blob_crc=PASS`.

**Qualification scope:** this is one precisely controlled ACK-gap power-cut boundary, *not* generic atomicity of interrupted filesystem writes or lifetime wear qualification. Raw 2MiB backup, full UART, extracted LittleFS and NVS partition images remain private on RPI400 under `/srv/espressio/scratch/gpt6-logging-scanopt-20261028-1800/private-scanopt-ackgap-20261029-01`. The full suite of production gates remains open and versions remain 0.1.0.
