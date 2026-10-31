# G7 LittleFS real EDLS N+1 protected rolling / GPIO17 electrical-cut fixture

**Test-only ESP32-WROOM-32 application** under existing `EDP-Logging`, not a new FileStorage provider, retention owner or production deployable. Reuses `logging/EdlsArchiveRotation.hpp` (2026-10-26 immutable N+1 API), the existing `EDP-Persistence-ESP-IDF::LittleFsVfsMount` and conservative `VfsFileStorage`, canonical 40-byte EDLS CRC header and complete EDLG V1 CRC-bearing **CBOR framing**. Raw CBOR payload is a tiny deterministic test message (array [1,2,3]), **not** yet a typed full 15-field LogRecord projection via EDP-Serialisation.

## Finite geometry and crash phases

The application Bootstrap selects three static paths, `slot0` / `slot1` / `slot2`, with **two protected EDLS generations** and one empty physical successor name. Distinct device identity is 16 bytes of 0xA6, frames bounded by 32-byte payload and 1024-byte maximum segment. Each test segment is precisely 64 bytes (40-byte EDLS header + 24-byte EDLG/CBOR framed record). Initial Boot publishes/validates g1/g2 using the **new protected owner**. After 12 seconds and durable test-only NVS phase arm, it publishes fully committed/readback-verified g3 in an EMPTY spare using the existing guarded successor. Crucially, g3 is a complete N+1 candidate, but g1 is **not yet physically retired** when the host cuts power.

The borrowed official RPI400 `powercut_qualification.py` and `relay_cut_monitor.py` are reused as a **test-only host controller**, without weakening their raw reset+boot-epoch/controller OFF/ON/minimum elapsed hold requirements. The application emits established bounded qualification labels plus separately truthful `EDP_EDLS` labels: before cut g1/g2 are last ACK, g3 is complete but not ACKed; after cut, it independently confirms all three complete CRC-valid EDLS/EDLG files then deliberately calls `FinishEdlsProtectedRollingRetirement`. Only after g1 retires does it publish g4 via the new protected-rolling API, leaving precisely g3/g4 and one free physical filename. The host requires `LOWSPACE_ROLLED` (truthfully emitted for intermediate verified g2/g3) and final `RECOVERED newest=4 older=3` plus a new boot epoch and `REPLAY_VERIFIED`. No native reset code is treated as electrical proof; verdict separates `POR_CONFIRMED` and `EXTERNALLY_VERIFIED_POWER_CUT`.

## RPI400 electrical safeguards

Only the explicitly available authorised RPI400 under an exclusive `espressio-agent-lock` may run this. The borrowed `run_relay_cut_test.sh` first creates an exact **2MiB private full-chip backup** and separately verifies the native combined firmware image and 589824-byte dedicated LittleFS seeded test image at offset 0x170000. It never autoformats on mount. Set:

- `EDP_LITTLEFS_AGENT_ID=<current-held-owner>`
- `EDP_LITTLEFS_EVIDENCE_DIR=<unique-private-absolute-path>`
- `EDP_LFS_CUT_STAGE=lowspace`
- `EDP_LFS_BUILD_ENV=esp32dev-lowspace`
- `EDP_LFS_OFF_HOLD_S=8.0` (or another controller-confirmed, approved hold within the host's 5..60s range)

The wrapper traps exit to `esp32-integrated power-off` even on failure. Power interruptions are performed ONLY by the user-confirmed no-backpower GPIO17 relay. No proprietary flash image, raw UART, NVS state or firmware binary belongs in GitHub.

After a physical run, separately read the **actual** 0x170000 / 0x90000 partition into RPI400 private storage, extract it with the already installed `littlefs-python extract --block-size 4096`, and run `PYTHONPATH=EDP-Logging/tools python3 audit_extracted_edls.py <private-dir>`. It reuses the existing Python EDLS header CRC32C and EDLG frame scanner, requiring only seed.txt, g3 in physical slot2, and g4 in slot0, each complete with correct identity, incarnation, CRC, payload and byte length. **No generic FileStorage PowerLoss atomicity, throughput, physical GC-phase coverage or flash endurance limit follows.**

## Qualification limitations

This fixture tests a **complete immutable successor → physical VDD cut → CRC-verified reboot → oldest-eligible retirement → next verified roll** lifecycle. The power loss occurs between two logical operations; it is not evidence of a flash-operation mid-instruction cut. It does NOT cover torn candidate identification/repair, all low-space file allocation failures, full rate/heap/stack/capacity/wear bounds, or full LogRecord→canonical typed CBOR projection. Host `test_*.py` includes borrowed 32 preexisting controller/unit cases and eight new independently authored offline physical EDLS audit negatives. The canonical Logging owner remains 0.1.0; underlying VFS advertised traits remain Restart/None/MayModify and all earlier failure receipts remain unchanged.


## 2026-10-26 — real EDLS near-full exhaustion and controlled external VDD cut

With `EDP_TEST_EDLS_LOWSPACE` enabled in the separate native `esp32dev-lowspace` environment, this same G7 test-only ESP32 application first commits canonical small EDLS/EDLG g1/g2, retaining both physical names and persisting `ack=2` in the **independently established NVS store** before any attempted write. It then fills the 589824-byte LittleFS partition using disposable, **never-acknowledged** 8192-byte pad files. Actual recorded high-water was 585728 bytes used / 4096 free; pad index 47 failed with native `FileReplaceStatus=12` (IoFailure). The application then attempts a real **4156-byte EDLS generation**: a full 4096-byte, syntactically canonical CBOR byte string (0x59 0x0f 0xfd + 4093 bytes) inside a complete 4116-byte CRC-bearing EDLG frame and 40-byte CRC-bearing EDLS header. Crucially the test must REFUSE g3 without changing either acknowledged g1/g2 and without ACKing g3.

**Genuine failed precursor:** RPI400 HIL run `20261026T123043-dfb40466` was **FAIL exit1**. Hardware physically entered near-full exhaustion and immediately panicked (`LoadProhibited`, reset4) from nested >8KB stack use in EDLS preflight and frame construction, before the host could cut power. External controller declined to claim a cut and ended `HOST|TIMEOUT|cut=False`; forced power OFF. Xtensa addr2line traced the VFS/EDLS scan/successor nested path. That failure drove the in-place existing-owner `WithScratch` refactor documented under `docs/EDLS-PROTECTED-ROLLING.MD`.

**Corrected hard VDD experiment:** new native ESP-IDF 5.5.5 rebuild `20261026T124226-4d05fa12` PASS exit0, combined verified image SHA256 `7c454f4b2e964ff6b8fbed7434ac3148819c145a550c695b17b80231b9c46f9c`. A separate fresh exact 2097152-byte chip backup SHA256 `3cdcd102ca34729a9925290957add905bc5ea43c42f0647a7a0c102f8ed28aee` preceded firmware/0x90000-partition provisioning in private evidence `private-edls-nearfull-20261026-02`. Physical GPIO17 controller run **`20261026T124343-179d79b4 PASS exit0`**: actual EDLS g3 refused `EdlsProtectedRollingStatus=11 ProviderRejected`, native `FileReplaceStatus=12` at 4096 bytes free; g1/g2 independently CRC-clean; host saw `LAST_ACK=2` and triggered actual **8.275 seconds supply-OFF**, then supply-ON and independent fresh boot. ROM reported `SDIO_RESET`, app reset10, so honest classification **`EXTERNALLY_VERIFIED_POWER_CUT accepted=True`**, *not* POR-confirmed. Device boot recovered acknowledged g1/g2; the application verified independent NVS `ack=2`, then performed **TEST-ONLY** explicit deletion of disposable padding files and **only the externally unacknowledged** failed candidate g3. It used real protected N+1 EDLS owner to commit/readback-verify g3 before retiring g1, then g4 before retiring g2. Retained final g3/g4 exactly, one spare path, CRC valid; relay OFF at end. This test-only cleanup is NOT an approved general production torn-candidate recovery API. No writer other than this fixture was active.

**Independent physical-media verification:** separate detached run `20261026T124943-0d68f0a9 PASS exit0` read the actual 589824-byte LittleFS partition at 0x170000 and extracted it with the existing LittleFS Python decoder; raw SHA256 `4a3a95c9ed8027c05040884662d9fac376dafbde9de1c40e86e9e2b999a41b8b`. Existing EDP-Logging EDLS header/CRC32C and EDLG frame/CRC parser independently confirmed `OFFLINE_REAL_EDLS_AUDIT=PASS oldest=3 newest=4`, exact g3 in slot2 and g4 in slot0, seed.txt valid, no unexpected pads or stale third generation. Host parser/physical evidence remain private on RPI400, with no binary/raw UART upload.

**Limits:** This tests low physical capacity and a hard cut **after** the large new write was rejected, not a cut inside `ReplaceFile` or garbage collection. The source has true N=2 bounded retention and static 4156-byte caller-owned archive scratch (outside 8192-byte main task stack), not an automatic repair of unknown candidates. Underlying LittleFS FileStorage remains `Restart / None / MayModify`. The 4096-byte CBOR byte-string payload is canonical but **not** a generated 15-field typed LogRecord; the separately passing host typed CBOR path and native full-record integration require their own cut, load and stack gate. Native ROM reset10 must never be relabelled POR based on host cutoff. All versions remain 0.1.0.

## Oct 27 — unacknowledged candidate VFS write-storm external VDD cut

The test-only `esp32dev-midwrite` native ESP-IDF 5.5.5 environment exercises the genuine existing `EDP-Persistence-ESP-IDF` LittleFS `FileStorage::ReplaceFile` while keeping **complete CRC-qualified EDLS generations g1/g2 untouched in physical slots 0/1**. After both generations have been independently read back and NVS `ack=2` is committed, firmware declares `EDP_BND|MIDWRITE_START|attempt=1`, then performs successive 65536-byte synchronous writes to the **never-acknowledged spare slot2** only. The exclusive RPI400 GPIO17 controller cuts electrical VDD on this marker, holds OFF for at least eight seconds, and verifies a new boot epoch. The rebooting firmware must revalidate exact g1/g2 by existing EDLS and EDLG CRC readers, confirm NVS `ack=2` from a fresh session, and **refuse to retire protected generations** when slot2 contains an unknown/invalid candidate. No test code silently removes slot2. The physical postcut partition is read separately at 0x170000 length 0x90000, extracted **without repair**, and independently checked by `audit_midwrite_extracted_edls.py`.

This is a stronger electrical candidate-write activity test than the earlier cut between fully finished immutable successors and retirement. It is **not** instruction-level proof the relay opened strictly between `fwrite` and `fsync` inside one native call; the controller and UART are asynchronous. It does not prove generic `FileStorage` PowerLoss atomicity, salvage of an unacknowledged candidate, production automatic provenance-authorized removal, or flash wear bounds. The deliberately oversized unacknowledged stress file is a **test-only** mechanism for inducing LittleFS write/GC activity; it is never treated as a valid EDLS segment. Provider profile remains Restart/InterruptionAtomicity None/FailurePreservation MayModify.

Host monitor/auditor regression suite includes false POR, pre-cut replay, bad ACK CRC, invalid candidate and unexpected file checks. All pass/fail evidence uses retained detached Run IDs; preserve both first FAIL and corrected PASS rather than modifying old evidence. Versions remain 0.1.0.

### Physical 27 October outcome

## 27 October 2026 — actual unacknowledged candidate write-activity VDD interruption

With RPI400 solely authorised and locked, `esp32dev-midwrite` native firmware (SHA256 of factory image `bd05436b05fd2e0455adbc020c7f069125d5b17e889b22ad6c675e9b1d0c05e6`) created and CRC-verified immutable EDLS g1/g2, committed separate NVS `ack=2`, and started actual ESP-IDF LittleFS VfsFileStorage writes solely to physical **unacknowledged slot2**. The host independently commanded GPIO17 VDD OFF **8.275 seconds**, then ON. Detached run **20261027T133518-b251b2aa PASS exit0**. Actual hardware reported ROM `SDIO_RESET` and app reset=10; therefore the supported status is **EXTERNALLY_VERIFIED_POWER_CUT**, explicitly *not* POR_CONFIRMED. After reboot the candidate was a **zero-length existing physical file** (`InspectEdlsSlot` Empty is not proof path absence), so protected rolling conservatively **refused the unusable candidate** without removing/retiring g1 or g2. Reboot firmware checked the exact two ACKed CRC-complete generations and their NVS watermark.

Second independent physical partition flash extraction (0x170000 + 0x90000) **20261027T134001-1ae1e8e5 PASS exit0**, SHA256 `d27f56ece7d5ee7f1c5ec613eac45c3520bb630018598adfcc82b2a2fc167d42`, shows exactly complete 64B CRC-protected g1/g2 and zero-byte unacknowledged slot2, no unexpected files. Offline parser `audit_midwrite_extracted_edls.py` independently rejects damaged protected frames, wrong identities, unexpected files, and symlinks. **58 host fault/CRC tests PASS**, C++20 foundation PASS, Clang finite rolling PASS, existing real EDP-Memory/Threading Task integration under GCC UBSan PASS. Sources and logs are retained on appliance without publishing private partition images or UART dumps.

**Coverage boundaries:** the accepted electrical cut occurred during an *ongoing candidate write activity window*, but UART/controller latency prevents proving the physical switch opened between instructions of one synchronous `fwrite` / `fsync` call. The 65536B disposable candidate is a test-only GC/write-pressure payload, not a normal EDLS segment. No general FileStorage PowerLoss atomicity, production provenance-authorized automatic torn-candidate disposal, flash endurance, or on-target production scheduling guarantee follows from this test. Logging remains **not production-ready** until separately outstanding gates pass. All versions 0.1.0.

### Strict real NVS+EDLS external-cut acceptance and incarnations

## 2026-10-27 — real ESP32 PowerLoss NVS + LittleFS with new runtime incarnation

**Two independent, real RPI400 GPIO17 electrical VDD runs qualified.** Both used actual existing ESP-IDF 5.5.5 NVS KeyValueStorage and LittleFS FileStorage; no Logging flash SDK owner. Every run included a verified fresh firmware image, 2 MiB full-chip private backup, separately verified seeded LittleFS partition and a relay OFF hold of at least 8 seconds. A complete acknowledged g1/g2 immutable EDLS window and actual NVS KeyValue `ack=2` were established before the GPIO17 controller cut supply as the *unacknowledged* 65536-byte test-only candidate spare write activity began. Physical cut is verified by the relay state and OFF interval; reset classification is separately checked by ROM and app boot epoch. The 65536-byte spare is test stress, NOT a production EDLS frame, and precise mid-fwrite timing is **not** proven.

- **Initial strict ACK physical run** `20261027T152551-c81f6178` PASS exit0, `POR_CONFIRMED`, OFF **8.269 s**, initial/bootstrap/runtime incarnation 3 retained, fresh boot recovered g1/g2 under committed NVS 2, removed unacknowledged spare and durably committed/rolled g3 and g4. Firmware SHA256 `4b5029295365233dcec2317147cb1612a1ef7c4e03a234bcf577dca471a86966`. Independent raw LittleFS scan `20261027T153046-8e9ae8c5` PASS, partition SHA256 `5a946cd3f26daec8f2b25801387d5a0f70a73bc96b11f9b227a72379326cdf26`, exact two 64B EDLS/EDLG CRC-protected g3/g4 physical slots. Its real NVS state is also in the next full-chip backup; independent Espressif NVS page+entry+blob CRC and EDP-Logging EDLS ledger parse: `OFFLINE_REAL_NVS_WATERMARK_AUDIT=PASS ack=4 last_incarnation=3 ns=edlsack`.
- **Cross-boot/runtime-incarnation physical run** `20261027T153341-f365c3f2` PASS exit0, `POR_CONFIRMED`, OFF **8.260 s**, initial `RuntimeIncarnation=3`, new reboot `RuntimeIncarnation=4`, same canonical DeviceIdentifier. The new strict ACK reconstituted *old* acknowledged g1/g2, committed new g3/g4 under incarnation 4, retaining exactly g3/g4 and NVS ACK=4 bound to the latest writer's incarnation **4**. Firmware SHA256 `36ce5408dded1b0022d64f82fe3f674df81a6d77742f35a5208f26f02a250b0d`. Detached independent dual-physical-media scan `20261027T153856-41a898fa` PASS exit0: actual extracted LittleFS SHA256 `f263586dda681abea0a1994ae6a75828f4476462dee7eb7a8d988b644e12249e`, exact CRC-complete newest g3/g4, and NVS 0x9000/0x6000 SHA256 `219e04159848eadf4ec505bd14e2289fe45114418de5618bb7692ea4e450a2f0`; Espressif's installed independent NVS parser validated page CRC, individual entry CRC and blob value CRC plus existing Logging parser independently validated the 40-byte EDLS watermark: `OFFLINE_REAL_NVS_WATERMARK_AUDIT=PASS ack=4 last_incarnation=4 ns=edlsack2`.

**Actual conclusion supported:** real NVS-based durable ACK=2 survives external VDD cut, torn/zero-byte candidate cleanup can be authorized without prior ACK loss, and recovery can create new g3/g4 under a NEW application runtime incarnation, with independently verified NVS and LittleFS physical evidence. Retained history spanning incarnations is permitted; comparing the archived ledger incarnation to the *current boot* instead of to the **newest ACKed segment** was an identified and repaired design defect. The exact 15-field generated LogRecord source and full task queue durable-ACK semantics, high-stress GC interruption, high-water resource reserve, wear and all G1-G15 release acceptance remain separate OPEN gates. No general LittleFS FileStorage power-loss atomicity claim; all package versions 0.1.0. Full-chip backup, UART, raw flash and NVS images remain private RPI400 scratch.

## Reproducible 15-field canonical CBOR and strict NVS-ACKed EDLS build (28 October 2026)

The optional `esp32dev-typed-strict` physical HIL firmware requires an owner-generated full diagnostic schema. Build with `bash tests/hil/g7-littlefs-protected-rolling-espidf/build_typed_strict.sh` from EDP-Logging with `EDP_LOGGING_DEPENDENCY_ROOT` pointing to the parent directory containing EDP-Localisation and the other declared sibling EDP owners. The runner generates the exact, deterministic `src/generated/GeneratedLocalisationContract.hpp` via the existing **EDP-Localisation generator**, then compiles with pinned ESP-IDF/PIOArduino. Generated code is ignored and never checked in. CI's G1 firmware-build workflow now exercises this **compile-only** native variant, while electrical GPIO17 VDD cutting and raw 0x170000/0x90000 plus NVS 0x9000/0x6000 flash audits remain **exclusive to the authorised RPI400**. Historical native FAIL run `20261027T160548-c2af1c34` was caused by that missing generated header; its corrected build result is recorded separately and must never retroactively change the FAIL.

## 28 October 2026 — complete 15-field CBOR + strict NVS acknowledgement hard-cut qualification

RPI400-authenticated exclusive owner `gpt6-logging-production-20261027-1318-r1` recovered the previously stalled private source. The first native `esp32dev-typed-strict` compile on October 27 **FAILED** because the required owner-generated `GeneratedLocalisationContract.hpp` fixture was absent. The new deterministic `generate_typed_fixture.py` and `build_typed_strict.sh` call the **existing EDP-Localisation generator**; generated output remains ignored and never committed. The corrected native ESP32-WROOM / ESP-IDF 5.5.5 build **20261028T084741-7a2bbe25 PASS exit0**, factory image SHA256 `0c00c5a72f53c6fad1fba117da9d330a266ac060b85b065466e47c1fa89d7049`. Host owner-generated canonical 15-field typed CBOR -> existing EDP-Memory pool and EDP-Threading Task -> **PowerLoss-atomic NVS-ACKed** finite EDLS under delayed completion, failed-ACK refusal and cross-incarnation recovery passed GCC UBSan `20261028T084841-8eecfdde` and Clang `20261028T084841-3d67076f`. Strict ACK POSIX VFS GCC UBSan `20261028T084842-82c860e4` PASS; 64 native-controller/offline tests `20261028T084843-b670a7d1` PASS.

The first hardware launch `20261028T085428-2e10428c` **FAIL exit1** at the *safety preflight* because its evidence directory already existed; the runner did not flash/cut the hardware. The corrected genuine GPIO17 physical test `20261028T085517-e0e6b92c` **PASS exit0** included a fresh 2097152B full-chip backup, verified factory + seeded LittleFS, full typed CRC-qualified EDLS g1/g2 with separately NVS-committed ACK2, then a **complete, CRC-verified but UNACKed g3 candidate** deliberately written in the physically vacant slot. Host removed power for **8.226s**, confirmed a new ROM/app boot with **SDIO_RESET** (not POR), and correctly reported **EXTERNALLY_VERIFIED_POWER_CUT**. The real NVS ledger still read ACK2 after reboot; producer-authorised recovery discarded unacknowledged g3 without touching g1/g2; new g3/g4 were then committed and NVS-readback acknowledged, leaving only the latest g3/g4. GPIO17 was powered OFF by the controller at completion.

An **independent physically readback** of LittleFS 589824B at 0x170000 and NVS 24576B at 0x9000, detached run `20261028T085939-c63f1f40 PASS exit0`, verified (without repairing media) exact owner-generated **all 15 permanent LogRecord FieldIds** with the correctly absent optional Field7, canonical CBOR TypeIdentifier/fingerprint, EDLS/EDLG CRC and retained physical g3/g4. Raw LittleFS SHA256 `ccfbfabdabdff5e5adba2f345fdc2324e7ff886de1413524d8e25ffa3646adf9`. Independent Espressif NVS page/entry/blob CRC and EDLS-watermark parser checked namespace `typedack`, ACK4, runtime incarnation7; raw NVS SHA256 `cb8f517789ad5357f2847c794b95c969af2537219c4edd853570e0bc325fc20d`. Private UART and raw images remain **only on RPI400**. Initial resource boot telemetry: 290744B free heap / main-task high water 6688B; reboot 290712B free / 6688B high water. These are test-specific measurements, not worst-case full product budgets.

**Exact scope:** a cross-cut between completely written unacknowledged g3 and the absent NVS ACK3 has been physically qualified. No generic VfsFileStorage PowerLoss atomicity or instruction-perfect cut *inside* a flash write/GC is inferred. The true production scheduled multi-producer queue/worker with latency/backpressure, high-water CPU/stack/heap, flash wear, physical low-space reserve, complete G1–G15 gates and GitHub Actions status remain open. Versions remain 0.1.0; no Mesh changes.

## 28 October 2026 — bounded 256-record real ESP32 strict-ACK load and independent forensic check

RPI400 user-authorised ESP32-WROOM-32 native ESP-IDF 5.5.5 test-only `esp32dev-typed-strict-load` uses the existing 15-field owner-generated EDP-Localisation/EDP-Serialisation TypedEnvelope, EDLG/EDLS immutable N=2 finite archive, actual LittleFS FileStorage and actual **PowerLoss-atomic NVS KeyValueStorage acknowledgement for every record**. The test does **not** use a production asynchronously scheduled Logging worker: one synchronous app_main task serially publishes 256 records, collects 256 real esp_timer publication latencies and samples ESP-IDF heap, task stack and physical LittleFS use at generations 1 and 16..256. The first compile `20261028T091026-748555dc` **FAIL** (test-only std::min operand type mismatch for stack watermark) was corrected by explicit uint32_t conversion, without modifying the Logging library. Native rebuild `20261028T091337-6be65263 PASS exit0`, factory image SHA256 `0b68a70d02ee1620caeee1b1abf549ef31d67ecbfba178a2f59ccd39a0efcab8`; 64 host native-controller/offline integrity tests `20261028T091520-9d400688 PASS`.

**Physical RPI400 run `20261028T091659-28bd6730 PASS exit0`**: verified exact firmware and seeded LittleFS after fresh **2 MiB full-chip backup**, then actually committed 256 canonical typed records through the strict NVS-ACKed existing EDLS owner, retained final generations **255 and 256**, ACK=256. Measured `184292842 us` total, per-publish p50 **767372 us**, p95 **850256 us**, p99 **850282 us**, maximum **865901 us**, approximately **1.39 individually durably acknowledged records/second**. Minimum sampled free heap **289292 bytes**, minimum sampled app-main task stack high-water **3216 bytes remaining**, max observed LittleFS occupancy **8192 bytes** in the dedicated 589824B partition. This is a measured single-task 256-record sample, not an established universal stack/heap budget, full product workload distribution or endurance test. These records are **far too costly to automatically assume unrestricted Logging ingestion**; no performance target was authorised and no pass/fail throughput threshold is invented.

**Independent actual physical flash audit `20261028T092414-13af6a4a PASS exit0`** read LittleFS 0x170000+0x90000, raw SHA256 **`401271eb782047f29d36f822f60fc96f627de4b560ab8bb8a834c222375d110d`**. The existing independent Logging parsers verified actual CRC-valid immutable EDLS/EDLG physical slot2=g255 and slot0=g256, complete permanent fifteen-field owner-generated CBOR TypedEnvelope, unchanged TypeIdentifier, schema fingerprint, device/incarnation and attempt values. Real separately read NVS 0x9000+0x6000, raw SHA256 **`2598d051b58c74dc4dba86187e6fd2a9093bcc85bb67c852b92ccd4e701ecdd4`**, was independently parsed using Espressif's official NVS parser; exact namespace `loadack` ACK **256** incarnation7, page/entry/blob CRC **PASS**. The ESP32 was powered OFF after audit (GPIO17 HIGH) and private images/UART kept on RPI400 only.

**Remaining production gates:** clearly define acceptable sustained log-ingestion and per-record durable-ACK latency/flash write endurance before claiming a production budget. For higher rates consider separately explicit immutable *segment batching* with no per-record premature durable acknowledgement and preserved protected N+1 successor semantics; never silently weaken PowerLoss ACK. This particular test has no concurrent producers, no deployed full EDP-Threading worker/sinks, no crash during real GC at high wear, no exact lifetime wear characterization, and the still-red GitHub Actions jobs have no accessible step output via current connector. Versions remain 0.1.0.

## Exact physical provider operation bottleneck profiling (28 October 2026)

New compile-only test environment `esp32dev-typed-strict-profile` and `build_edls_provider_profile.sh` use the existing EDP-Localisation owner-generated 15-field contract and existing strict EDLS/NVS algorithm **unchanged**, substituting only nonallocating timed test-only Persistence proxies under the same static provider contracts. No log emission occurs during a measured `Publish` call. `run_edls_provider_profile.sh` makes a fresh verified 2 MiB device backup, flashes verified factory binary + LittleFS seed, runs 256 records and retains a UART transcript under a unique private evidence root. `summarise_provider_profile.py` validates the resulting counts/timers and prints a sorted read/write/NVS table. Independent physical LittleFS/NVS flash readbacks use the existing typed EDLS and official Espressif NVS auditors. The 28 October native profiler run `20261028T173305-6d2590ff` PASS, raw audit `20261028T174032-c580c906` PASS; see [measurement report](../../../docs/EDLS-PUBLISH-256-RECORD-PROVIDER-LATENCY-2026-10-28.MD). Production libraries unchanged; all 0.1.0.

## Configurable sealed 16-record native fixture (30 October 2026)

The optional `esp32dev-sealed-batch` environment composes the existing
BoundedEdlsBatch, PowerLossAcknowledgedEdls, canonical Clock MonotonicClockProvider
and the EDP-Platform-ESP-IDF EspTimerMonotonicTimebaseProvider at the native test
Bootstrap. It uses the same owner-generated fifteen-field TypedEnvelope CBOR
fixture. Each source record keeps its own attempt ticket. One strict NVS ACK
names a sealed SEGMENT of 16 records; it never names an individual record.
Physical geometry is three names, two protected segments, at most 4096 framed
body bytes plus a 40-byte header and 16-byte completion seal. Archive/readback
scratch and the bounded batch storage are static, outside the 8192-byte task stack.

Build using the existing typed-fixture helper with
`EDP_LFS_BUILD_ENV=esp32dev-sealed-batch bash build_typed_strict.sh`.
The additional native-only dependency is EDP-Platform-ESP-IDF at
`adc4164031016c200ad88d742fc4f05c86d66833`; its exact timebase header blob is
`d5b5709be51f0dd8814a743c0c68546ecdb49ad0`. No native dependency is added to
neutral production Logging headers.

The benchmark calls ConfigureDwell with an explicit 100ms duration, admits 16
records without a durable acknowledgment, then services one full batch. It
counts 256 individually verified durable record completions across 16 segment
commits. Record latency is measured from RAM admission to the completed strict
commit using the canonical Clock. Dwell limits intentional accumulation; it
does not promise 100ms overall durable completion. The library supports explicit
zero or shorter dwell settings; this test selects 100ms. This is a synchronous
one-task benchmark, not scheduled multi-producer qualification.

Use the existing physical runner with EDP_PROFILE_BUILD_ENV=esp32dev-sealed-batch,
EDP_PROFILE_AGENT_ID set to the held exclusive lock owner and
EDP_PROFILE_EVIDENCE_DIR set to a unique nonexisting private absolute path.
The runner retains the full-chip backup, exact verified firmware and seed
images, actual 8192-byte stack preflight and exit-to-OFF trap. The existing UART
monitor selects the sealed receipt only for this explicit environment.

The `esp32dev-sealed-batch-cut` environment first strictly ACKs segment1 and
segment2 (records1..32). It writes and independently verifies complete sealed
segment3 (records33..48) in the empty spare, while NVS still says ACK2. The
existing relay runner/monitor uses EDP_LFS_CUT_STAGE=typed_ack_gap and
EDP_LFS_BUILD_ENV=esp32dev-sealed-batch-cut. After the externally verified GPIO17
cut, firmware requires intact protected segments1/2 and the complete unACKed
candidate. Only the existing strict archive Recover removes that candidate.
New strict commits3/4 must retain records33..64. The same original record bytes
are reconstructed and compared for every physical frame. Physical cut
classification remains separate from ROM/app reset classification.

After either physical run, execute `bash run_sealed_batch_raw_audit.sh` with
the same EDP_PROFILE_AGENT_ID / EDP_PROFILE_EVIDENCE_DIR and
EDP_BATCH_AUDIT_KIND=profile or power-cut. This separately reads actual raw
LittleFS and NVS, extracts without repair, checks the whole EDLS/EDLC seal and
every EDLG CRC, validates every permanent typed field against independent golden
values, and uses Espressif's official NVS page/entry/blob CRC parser. Expected
windows are segments15/16 (records225..256) or segments3/4 (records33..64).
Raw images and UART remain private. Test outcomes and exact run IDs are recorded
in the living handoff; a build alone is not physical qualification.

Corrected sealed-batch cut 20261030T135327-96bdbd46 PASS: GPIO17 OFF 8.221 s,
EXTERNALLY_VERIFIED_POWER_CUT, native reset 10 / ROM 6 SDIO_RESET; final ACK4 and
segments 3/4 contain all original records 33..64. Independent actual raw media
audit 20261030T135804-8e1a4cfe PASS. First attempt remains FAIL because its new
terminal string was not recognised; the corrected firmware reuses the existing
STRICT_TYPED15_NVS_ACK_AFTER_CUT marker. No host acceptance predicate changed.
See docs/BOUNDED-EDLS-BATCHING.MD and the living handoff for exact firmware/media
hashes, measurements and remaining gates.

The same fixture has explicit esp32dev-sealed-batch-64 and
esp32dev-sealed-batch-64-cut environments. EDP_TEST_BATCH_RECORDS selects 64;
default remains 16. Each fixture budgets 256 framed bytes per maximum record:
body cap 16384 B and total/scratch cap 16440 B for 64, outside the 8192 B stack.
The 256-record workload ACKs four segments and retains original tickets
129..256 in segments 3/4. The same relay protocol protects records 1..128 before
its complete unACKed third segment. Select profile64 or power-cut64 in the raw
auditor; it checks every one of the 64 original frames per retained segment.
See the batching contract for current results, resource budgets and limitations.

64-record physical sequence 20261030T145734-fce800f4 PASS: 256 durable original
records across four segment ACKs, 132.828 records/s, p50 512326 us / p99 590018 us,
sampled min heap 241848 B / stack headroom 3076 B. Independent profile and
post-cut raw audits verify all 64 records in each retained segment 3/4 (129..256),
whole seals, frame CRCs and official NVS CRC ACK4. Fresh GPIO17 OFF 8.220 s with
matching native/ROM power-on reset is POR_CONFIRMED. Same verifier and explicit
100 ms dwell, larger 16384 B body cap; this is a synchronous burst sample, not
a scheduled workload or endurance qualification. Full details are in the contract.

## Managed batch worker fixture and discovered contention gap (30 October)

Environment esp32dev-batch-worker composes the existing DedicatedCoordinator,
EDP-Memory source/representation pools, canonical transactional queue,
generated typed delivery, BoundedEdlsBatch64 and actual LittleFS/NVS.
Two EDP-Threading Dedicated Thread producers use separate admission slots
and opposite native cores. Clock and ThreadContext use the same bound
EspTimer MonotonicClock instance. Source queue capacity128, source pool130,
encoded pool1; worker stack8192, producer stacks4096 each, main8192.

Intended phases are160 concurrent attempts against a stopped consumer
(128 RAM publications and32 finite pre-Memory capacity refusals), existing
worker drain, one100ms partial batch with no later input, then2000 records
scheduled across two50Hz producers. Original source coordinates are
retained only as test telemetry; final media replay reconstructs all15
fields and compares canonical bytes. The independent worker raw audit
reuses shared sealed/typed parsers plus the official NVS CRC parser.

This fixture is NOT physically qualified. First native build
20261030T160526-2f2b24bb failed a test telemetry unsigned/uint32_t mismatch.
Corrected build20261030T160913-62a706bd passed; first physical
20261030T161030-bcd6a9dd failed during phase1 after verified firmware/seed
and a full2MiB backup. No idle/scheduled result or raw audit followed.
Exact diagnostic build20261030T161857-423afcb6 passed without the earlier
internal-linkage warning. Diagnostic physical20261030T162046-198ff12f
again failed, now proving producer statusCommitFailed9, queue Busy3,
reservation_active1 after successful admission. Existing Commit uses a
NoWait queue mutex, so ordinary concurrent contention is observable as
an unpublished reservation that still needs resolution. Do not hide this
by dropping ownership or retrying an uncertain mutation. Target powered
OFF via runner trap; both private evidence roots are preserved.

Next production work must extend/reuse bounded transitions on this SAME
queue, preserve the original NoWait APIs and canonical deadline ownership,
and retain every reservation/claim on uncertain release. No new Memory
capability, queue, mutex or worker is proposed. Native idle/scheduled and
power-loss/resource gates remain open.

The shared monitor now verifies guarded power-on BEFORE opening UART.
84 independent controller/media/power-order negative tests passed on
RPI400 in20261030T162116-08ae965d, plus shell syntax. A mock UART PASS
printed by the unit test is not a physical worker result.

run_edls_provider_profile.sh defaults to EDP_PROFILE_BACKUP_POLICY=full.
Explicit disposable skips the backup only when the operator has already
authorized disposable target firmware; firmware/seed digest verification,
raw-media audits, controller power guard and power-off traps remain.
The current user explicitly waived preservation/restoration of attached
ESP32 firmware, so diagnostic repeat02 used disposable and logged that
waiver. This does not waive any durability or electrical-cut criterion.

## Existing bounded queue transitions implemented (30 October continuation)

The native Bootstrap now explicitly selects CoordinatorQueueLockWait<5000000>
on the same DedicatedCoordinator. ProduceRecordUntil uses its original
commit deadline. The existing queue rejects late grants, preserves stable
source views and retains release-once retirement plus notification failures
across metadata-finalization timeout. See
[bounded transition contract](../../../docs/BOUNDED-QUEUE-TRANSITIONS.MD).
GCC/Clang UBSan owner/deadline/typed regressions passed; clean native build
20261030T165014-930d07b6 passed. The earlier two physical FAIL results remain
unchanged. A new physical run and independent media audit must pass before
claiming idle partial, concurrent scheduled or throughput qualification.

## Scheduled native diagnostics: source and representation Memory topology

Corrected queue physical20261030T165539-4c8c315c reached pressure PASS
(160 attempts,128 RAM publications,32 pre-Memory deadline refusals) and
idle partial PASS (one record, no subsequent input,100ms configured dwell,
371270us occurrence-to-ACK), then stopped in scheduled ingestion.
Exact diagnostic build165955-05d6c16a PASS; physical170053-0f52d972 FAIL
showed ticket288 representation PoolCapacityUnavailable1/acquisition1,
releaseAlreadyEmpty1 and encoded_lease_empty1, before new delivery.
The shared EDP-Memory topology's NoWait coordination entrance may return
CapacityUnavailable while source activity holds that topology mutex;
this was not a new typed validation or storage failure.

Bootstrap now reuses the existing native G10 pattern from g1-espidf:
source and representation have independent EDP-Memory topologies,
ordinary mutexes, allocators and existing portable resource instances.
No new Memory API, allocator implementation, wait loop or provider was
introduced. Source130 and encoded1 dedicated counts remain finite.
Both actual runtimes initialize and tear down explicitly.

Build170501-ec8f335b PASS. Physical170655-a6dc7a68 FAIL reached the same
pressure PASS and idle100ms PASS (371508us), with no representation sink
failure, then source admission DeadlineElapsed during scheduled phase3.
These are partial gate results, not a successful2000-record workload,
100records/s qualification or raw-media audit. Raw audits were not run
after the failure; all five private evidence directories remain intact.
An explicitly selected longer-dwell profile is the next measurement;
default100ms and all earlier failures remain attributable configurations.

## Explicit500ms configuration and capacity result

esp32dev-batch-worker-500 reuses the existing BoundedEdlsBatch template
ceiling and ConfigureDwell500ms. The ordinary worker profile remains
separately selectable at100ms. All phases, including the single idle
record, use the selected dwell; terminal and host runner require the exact
scheduled_dwell_ns and matching idle marker. No production default or
wire/archive behavior changed.

85 host/controller/audit/dwell negatives passed in172055-72ad142c; native
new-environment build171750-4cb95781 passed cleanly. Factory SHA256
2ced5974eeb9a0829e57c103afb10ff60db4d70f33962ce3091d446ce0cbba1b.

Parent172745-bc464f09 failed before hardware use: its show0|rg-q
prerequisite hit BrokenPipe because this appliance helper prints full
logs for zero tail. Corrected private parent uses completed status
capture; no appliance helper or project acceptance criterion was changed.

Physical172959-735fd90a FAIL after verified factory/seed. Pressure PASS
(128 publications/32 deadline refusals); one500ms idle partial/no later
input PASS with808409us occurrence-to-ACK. Scheduled phase3 then failed:
AdmissionFailed8/DeadlineElapsed1, queueCapacityUnavailable2,
memory_attempted0. Observed published628/retired500/durable436; these
are diagnostic concurrent observations, not a coherent final snapshot.
No representation failure, no full2000-record success and no raw worker
audit. Private evidence07 intact;06 had no hardware/evidence phase.
A longer dwell alone did not qualify the target100records/s workload.

Next measure actual per-stage wall cost and evaluate a supported native
CPU configuration or an owner-preserving source-path optimisation. Keep
generic typed validation and all durability checks intact; do not hide
capacity refusals or claim a successful sustained rate.

## Same-owner stage wall profile,160MHz debug-optimisation baseline

Test-only SourceSink now measures the existing generated encoder,
generic typed delivery without/with an archive publication, and ordinary
service, using the SAME canonical Clock. These are wall nanoseconds,
including scheduling delay; they are not isolated CPU-time measurements.
No timer/provider, worker, queue, codec or production validation changed.

Native build174316-98a4bddd PASS. Factory SHA256
8d600af092e17f61da71fb6b62958a2351d34a990ae49a503a399f539338d520.
Actual physical174416-6c4851cf FAIL at queue-capacity admission as before.
Pressure128/32 and idle500ms/no-new-input passed (810470us ACK).
The failure path now explicitly requests all three managed threads stop,
wakes them, waits for finite actual quiescence and only then reads
same-owner counters. Pending queue/RAM is retained; no implicit flush.

Scheduled-phase observations after quiescence:
434 encoder calls,total337854us (~778.5us/call);
428 delivery calls without archive flush,total753267us (~1760us/call);
6 flush-bearing delivery calls,total3846770us (~641128us/call);
66 service polls,total2704us; total callback wall5006745us.
These are a failed finite sample, not steady throughput or full2000-record
qualification. Deferred completion deltas may remain in the sink at stop,
so the report's durable count is a lower bound rather than every committed
record. No independent worker raw audit ran.

The existing firmware selects160MHz and CONFIG_COMPILER_OPTIMIZATION_DEBUG.
Actual PlatformIO compile-database run174914-e976a892 PASS confirms the
application command has -Og twice and no NDEBUG macro. The SDK's supported
ESP32 Kconfig also permits240MHz, but no CPU change has been made.
Next measure an explicit -O2 configuration with assertion checks retained,
before considering any new source-path API or weakened validation.

## Controlled O2 managed-worker qualification (30 October 2026)

The explicit esp32dev-batch-worker-500-o2 environment selects the existing
500ms dwell ceiling/runtime setting and -O2 for application compilation.
The standard100ms and debug-optimised500ms profiles remain separately
selectable. CPU remains160MHz; SDK assertions remain enabled at level2.
The actual PlatformIO application compile command is independently checked
by verify_worker_compile_profile.py after compilation: exclusively -O2,
no NDEBUG, exact worker/dwell flags and SDK CPU/assertion configuration.
A rejected configuration cannot enter the O2 hardware runner. Seven new
negative/control tests join the existing85 tests; detached
20261030T175750-c0257ece passed all92 on RPI400.

Native build20261030T175705-9bac48b9 PASS, including the actual command
verification. Factory SHA256:
365fcfa8b3907bee589c75783301ed83f95f2289c676625c5ae8702d3f8d7fee.

Physical and independent raw-media sequence20261030T180038-622f5b3b PASS:
- Pressure:160 attempts,128 RAM publications and32 finite deadline refusals,
  with zero ACKs before the worker starts.
- Idle partial: one record, no subsequent input, explicit500ms dwell;
  occurrence-to-ACK717825us.
- Scheduled: two managed producers,1000 records each at50Hz, no refused
  scheduled records; complete drain in21002193us including final ACK.
  This qualifies finite ingestion offered at100records/s; including drain,
  the end-to-end2000-record average is approximately95.23records/s.
- Admission/occurrence-to-ACK latency p50809995us, p951113851us,
  p991215160us; maximum observed schedule lateness76027us.
- Sampled minimum free heap160452B; stack headroom producer02916B,
  producer12924B, worker3292B, main4748B; observed core masks3/3.
  Source object144B, queue2592B; zero reported queue-lock contentions.
- All2129 accepted records across pressure, idle and scheduled phases
  were retired and durably acknowledged before explicit finite teardown.
  NVS segment ACK35; native replay and independent physical LittleFS
  parsing verify all15 original fields in the final retained92 records
  across two bounded segments. This retention profile does not keep every
  earlier ACKed source record.

Actual LittleFS SHA256:
7ae87d042033d31b6af49a997a6de9bb652874bd29641046b0b07fbfb3a00586.
Actual NVS SHA256:
2d12820fa3b33b54d8e3cef1ca4ce18d70f43fa8145ed2e6da17596dc7043c9e.
Official independent Espressif page/entry/blob CRC parsing confirms ACK35
in batchack, writer incarnation7. Raw images/UART remain private on RPI400.

Scheduled-phase wall observations:2000 encoder calls607283us;
2000 non-flushing typed delivery calls1363738us;359 service polls15487165us;
callback wall total2138302us. Publications occurred through ordinary sink
service in this run. Wall times include scheduling and storage waits,
not isolated CPU utilisation. No generic typed validation, CRC/readback,
NVS ACK ordering, provider owner or CPU frequency was changed.

This is one finite successful O2/500ms workload. Earlier Og100ms/500ms
capacity failures remain FAIL. It does not qualify default100ms scheduling,
arbitrary in-write/GC power interruption, sustained endurance, a product CPU
budget or all release gates. No GitHub Actions execution was used.
All versions remain0.1.0; target was independently verified powered OFF.

## Extended6000-source managed qualification (30 October23:28CET)

The explicit esp32dev-batch-worker-500-o2-6000 environment keeps the same
two50Hz producers, ordinary coordinator,128-slot queue,130-source pool,
one encoded pool,64-record batch,500ms dwell and160MHz CPU. It schedules
3000 sources per producer. Default2000 remains separately selectable.
Actual compile proof additionally requires the selected producer count;
wrong workload, CPU, dwell, optimisation or disabled checks cannot satisfy
the extended gate.99 host/controller/media/command tests231645-6f27cbdc PASS.

First extended build231625-9890b348 FAIL: duration-sized test telemetry
exceeded static DRAM by6472B. No hardware phase ran; prerequisite parent
231749-08442200 also FAIL before hardware. Corrected test evidence uses512
ticket-tagged coordinate/order slots, covering queued, pending, deferred
completion and retained records; stale reuse fails explicitly. All6000
exact latency samples remain. This is evidence history, not work transport
or a production memory-pool change. Native rebuild232502-77723967 PASS,
including actual command verification with assertions enabled/level2.

Factory SHA256:
ded6c03fe262e2d177a0e29bb18a3b3fcc859f8ea61267b956d527755aecf379.

Physical + independent actual-media sequence232528-9e980d90 PASS:
- pressure128 accepted/32 finite refusals before any ACK;
- one idle partial, no subsequent input,500ms dwell, ACK718004us;
-6000 scheduled sources, zero scheduled refusal/failure; all6129 accepted
  phase records retired and durably completed, partial0, cooperative stop
  and actual Threading/Memory teardown;
- full scheduled drain61.096605s, approximately98.205records/s including
  final drain for the workload offered at100records/s;
- ACK latency p50812873us/p951117971us/p991257849us;
  maximum schedule lateness69849us;
- sampled minimum heap162096B; producer stack headroom2916/2924B,
  worker3308B, main4740B; source144B, queue2592B; core masks3/3;
  queue-lock contentions observed0;
- final NVS ACK97; native and independent raw LittleFS replay check every
  original15-field binding for124 retained records in the latest two
  segments; official independent NVS page/entry/blob CRC ACK97/incarnation7
  in batchack passes.

Actual LittleFS SHA256:
19cb68954a9a50a6fea69c979ca93173f4179cb9ed13329f693a809ea944c63e.
Actual NVS SHA256:
441be9b2dfdd1bf0c670fe2fc41134dd32e90811123a1bfb6f240bab0138061a.

Scheduled wall observations:6000 encoder calls1827705us;6000 non-flushing
typed delivery calls4051504us;945 service polls46636263us; callback6404210us.
These include scheduling/storage waits, not isolated CPU utilisation.
Recorded heap includes the corrected test history and is not directly a
product memory budget.

Provisioning now retains ROM/stub through independent full firmware and
filesystem verification using the existing appliance with-power interface,
as already qualified by the three-target demos. No controller/GPIO owner
or provider implementation changed. Raw images/UART remain private in
private-worker6000-20261030-01; target exitsOFF.

This is a finite60-second successful load. It does not establish indefinite
sustained rate, a current-pipeline write/GC cut result, flash endurance or full
production acceptance. All versions0.1.0; no GitHub Actions coverage relied on.

## Sealed64 candidate write-activity cut,31October

The existing fixture now selects esp32dev-sealed-batch-64-writecut with
EDP_LFS_CUT_STAGE=sealed_write. Reuses the real conservative provider and existing
relay controller; only the unacknowledged spare is repeatedly replaced during a
finite activity window. Native002959-a64a190f PASS, controller105tests
003048-62c31b03 PASS, physical+raw003134-4bb013c2 PASS.
Actual8.232sOFF; native10/ROM6 SDIO_RESET is classified
EXTERNALLY_VERIFIED_POWER_CUT. Never relabel it POR.
Original protected batches1/2 pass exact postcut replay; subsequent ordinary
ACK4 retains128 full original15-field records129..256, independently raw-verified
with official NVS page/entry/blob CRCs. Final targetOFF.
See [complete scope, hashes and procedure](../../../docs/SEALED-BATCH-WRITE-ACTIVITY-QUALIFICATION.MD).
This does not prove an exact interrupted flash opcode/GC phase or qualify the
managed source queue path. No generic provider PowerLoss capability was changed.

Final policy-formatted adapter rebuild004220-17236074 PASS; exact final image
requalified004328-1221099b PASS,8.257sOFF, same honest SDIO electrical
classification and full128-record/NVS raw audit. Host004321-80663e22 PASS106.
Factory2b0ce0183459f4782a1bc6b39a096cbb1b427fa6218c00d1f09b17f10dd5dd29.
Runner backup defaults full. Explicit EDP_LFS_BACKUP_POLICY=disposable is for
user-authorised disposable targets only; full image/filesystem verification
before boot stays mandatory. Both backup modes physically tested.

## Managed worker write-activity cut,31October

Existing native env esp32dev-batch-worker-500-o2-writecut, existing relay stage
sealed_write and explicit raw mode EDP_WORKER_AUDIT_POWER_CUT=1. Same source
Memory pools, transactional queue, managed coordinator, canonical Clock,
generated15-field codec and strict bounded batch/archive. Original128 source
coordinates are emitted before cut; no reconstructed originals are accepted.
Native cut build004645-18f579b9 and normal profile build PASS. Initial physical
parent004958-1bd62862 FAIL01:07:02 because the host classifier omitted the
new managed marker. It remainsFAIL; the living handoff records the following
unattended gap. Corrected classifier117tests095754-83c4528e PASS.
Fresh physical+raw **095844-2022f045 PASS**10:00:33CET. Controller8.223sOFF,
native7/ROM2 then16 watchdog sequence, EXTERNALLY_VERIFIED_POWER_CUT.
Independent raw audit verifies128 original full15-field records and each
pre-cut coordinate, plus official NVSACK2 page/entry/blob CRC. Final targetOFF.
[Complete scope, failures and hashes](../../../docs/MANAGED-BATCH-WRITE-CUT-QUALIFICATION.MD).
No exact interrupted flash instruction, GC phase or unbounded endurance claim.

## Managed near-full refusal and cut,31October

Existing padding helper is shared with full typed managed batching:
esp32dev-batch-worker-500-o2-capacitycut, relaystage managed_capacity,
rawmode EDP_WORKER_AUDIT_POWER_CUT=capacity. No new storageprovider,
allocator, worker, queue or codec. Native100438-e1dcfe78 PASS, host123tests
100558-2f942103 PASS. Physical+raw **100726-066996bc PASS**10:15:48CET.
After45 padding files, real allocation refused with4096bytesfree. A real
subsequent batch write returned nativeIoFailure12;52pending records remain
owned in RecoveryRequired,128durable and NVSACK2 unchanged, executionquiescent.
Actual8.243sOFF, native1/ROM1 POWERON_RESET gives POR_CONFIRMED.
Recovery checks protected128, removesonlytestpadding, rechecksprotectedhistory;
independent rawaudit comparesall128 original15-field records and original
timestamps, official NVSACK2 page/entry/blob CRCs PASS. TargetOFF.
[Scope, ownership disposition and hashes](../../../docs/MANAGED-BATCH-CAPACITY-CUT-QUALIFICATION.MD).
No exact interrupted GC/flashinstruction or unbounded endurance claim.

## Sealed64 post-watermark retirement cut,31October

Native esp32dev-sealed-batch-64-retirecut, relaystage sealed_retire.
Same batch/strict archive and forwarding adapter; deliberate gate before actual
oldestunlink, after existingNVS3commit. Native101932-01e64704 PASS;
host127tests101948-3f00a801 PASS; physical+raw **102037-886599f8 PASS**
10:28:21CET. Actual8.279sOFF, native1/ROM1 gives POR_CONFIRMED.
Freshboot observesACK3/allthreecompletegenerations, Recover must return
CommittedOldestRetired, exact original2/3 survive, then ordinaryACK4.
Independent rawall128 full15-field originals129..256 andofficialNVSACK4 CRCs PASS.
TargetOFF. [Scope and hashes](../../../docs/SEALED-BATCH-RETIREMENT-CUT-QUALIFICATION.MD).
This is a pre-unlink boundary test of the shared archive owner, not an exact
mid-unlink/GC-instruction or indefiniteendurance certificate.

## Five-minute managed30000-record workload,31October

Native esp32dev-batch-worker-500-o2-30000, actual15000U perproducer,
sameO2/assertions/160MHz/500ms/queue128/64-recordbatch. Native103039-ff4325a3 PASS;
host132tests103255-e233b7ab PASS; physical+raw **103339-6856faf0 PASS**
10:46:17CET. All30129 accepted phase records published/retired/durable,
zero failures/partial submissions; only32 deliberate initial pressure refusals.
Scheduled30000 complete in300.949900s includingdrain. Allscheduledmean820230us,
max1540196us; last6000samplep50823952us/p991248615us explicitlylabelled.
Minimumheap162704B, workerstack3244B/main5068B, queuecontentions1.
Nativeprogress every10s. FinalACK472/raw124 original15-field records and
officialNVSCRC PASS; targetOFF. Factory6bdb389377b5f03a021daa0a8c2b49984059c60fac5c48c10471e51779a31f2e.
[Full results and bounded telemetry](../../../docs/LONG-MANAGED-WORKLOAD-QUALIFICATION.MD).
Finite five-minute qualification only; production retention/duty/lifetime still
need application targets, not extrapolation from this benchmark.

Final stricter PASS-line percentile-scope check:132tests and independent saved-raw re-audit105342-cc6370ba PASS. Native image unchanged.


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
