# Generated LogRecord → canonical CBOR → finite EDLS archive

**Implementation**: `logging/GeneratedLogRecordEdlsArchive.hpp`. Optional existing G3/G4/G7 integration; all component versions remain **0.1.0**.

## Ownership, capacity and ordering

The existing generated EDP-Localisation diagnostic message contract selects exact positional types. The existing `ProjectGeneratedLogRecord` validates the source LogRecord (all 15 approved permanent FieldIds, identical permanent System TypeIdentifier, schema fingerprint, source identity, severity, record facts) and reconstructs each owned argument under that generated contract. The EDP-Serialisation owner measures and writes one **canonical CBOR TypedEnvelope**, including original permanent identifier and typed Field13 values, into Bootstrap/worker-supplied bounded bytes. The existing Logging EDLG framing then writes the checked CRC32C envelope. Finally the already-qualified protected N+1 EDLS archive owner publishes that exact EDLG frame into an unused physical filename, verifies the new generation and all still-protected predecessors, and only then retires the oldest eligible segment.

`PublishGeneratedLogRecordToProtectedEdls<TMessageSchema,TProtectedSlots,TMaximumPayload,TMaximumSegment>(record, projection, positionalScratch, cborScratch, edlgScratch, FileStorage, physicalPaths, deviceIdentifier, runtimeIncarnation)` is a **synchronous worker-side** composition helper, not a new worker, pool, storage provider or wire encoder. Both output scratch spans, the typed projection and generated argument scratch are caller owned; they must remain alive for the entire call, not alias, and may be provisioned through the existing Bootstrap EDP-Memory owner. Record/device incarnation must exactly match the physical EDLS header arguments. The source and generated Localisation fingerprint must match. No hidden allocation, unbounded queue, task, or direct ESP-IDF import is introduced.

The compile-time protected segment count N is a **finite** retention maximum and exactly N+1 physical filenames are required. Complete EDLG payload bounds and complete EDLS segment bounds are compile-time configuration. Indefinite history growth is forbidden. A physically exhausted LittleFS partition may refuse a new immutable candidate; the preceding acknowledged retained history is not deleted. This does not determine the future production partition reserve; physical free-block/GC/wear requirements are separate acceptance gates.

## Results

`GeneratedEdlsPublishStatus` distinguishes `Published`, `SourceIdentityMismatch`, `ProjectionRejected`, `CborRejected`, `CborCapacityExceeded`, `FramingRejected`, and `ArchiveRejected`. `GeneratedEdlsPublishOutcome` retains exact nested `GeneratedLogRecordProjectionOutcome` and `EdlsProtectedRollingOutcome`, plus encoded CBOR/EDLG byte counts. Even `Published` means logical complete readback / protected successor ordering **only**; it does not upgrade `FileInterruptionAtomicity::None` or assert physical power-loss durability of a typed production record without separate hardware testing.

## Verification

New `tests/GeneratedEdlsArchiveIntegrationTests.cpp` plus `run-generated-edls-archive-integration-tests.sh` generate a **real owner-produced** EDP-Localisation schema, compile with actual EDP-System/Clock/Serialisation/Memory/Persistence VFS dependencies, and test multiple finite rollovers with independently reopened filesystem instances. They inspect each persisted EDLG payload and independently invoke the owner CBOR TypedEnvelope deserializer to check all permanent identity/argument fields, not merely EDLG/EDLS CRCs. Negative tests refuse changed device identifier/runtime incarnation, too-small CBOR scratch and changed schema fingerprint *without mutating the archive*. GCC, Clang and sanitised results are tracked by immutable RPI400 run IDs in the Policy handoff.

The remaining blockers for a full production release are the actual queue/worker/pluggable sink lifetime integration under saturation, physically qualified **this full typed payload** on ESP32-WROOM during hard VDD interruption, unambiguous torn-candidate recovery, finite physical write reserve / garbage collection and wear budgets, and outstanding G1–G15 acceptance gates. Existing synthetic and short EDLG native hard-cut results do not automatically qualify all longer typed payloads.


## 2026-10-26 — existing EDP-Memory / Threading Task handoff (G10)

Optional `logging/GeneratedEdlsTaskAdapters.hpp` adds only two adapter types for the
**already-existing** owner pipeline. `GeneratedLogRecordCborEncoder` takes
existing generated schema-qualified source `BasicLogRecord`, caller-owned
`GeneratedLogRecordWireProjection` and `OwnedPositionalScratch`, and emits
EDP-Serialisation `TypedEnvelope` CBOR directly into the existing
`EncodeOnceAndSubmitToSinks` EDP-Memory buffer. The existing
`AsyncCopyingTaskSink` makes an independently owned, finite EDP-Memory copy
before dispatching an existing EDP-Threading `Task<RecordSinkSubmission>`;
Task admission is **not** archive durability.

The Task's application-composed destination is
`TypedCborProtectedEdlsTaskDelivery`. It verifies exact canonical CBOR
re-encoding (rejecting trailing bytes/noncanonical alternatives), generated
Localisation message and fingerprint, permanent typed field identity,
incarnation, DeviceIdentifier, attempt and declared sequence width; then
uses existing EDLG framing and `PublishEdlsProtectedRollingSuccessor`.
This callback is synchronous on the existing Task worker and never retains
its borrowed CBOR span. Return `Accepted` ONLY when the protected archive
commits and verifies the generation; otherwise `Rejected` for untrusted
input or `ProviderFailure` for archival failure. `LastCompletion` and
`LastArchive` are terminal **Task-owned** evidence, read only after
Task completion/consumer-owner synchronization (not atomic). The adapter
allocates no new worker, queue, pool, codec or concrete FileStorage.
The caller supplies exclusive FileStorage ownership across scan/write/retire,
separate bounded frame scratch, static N+1 path array, projection scratch
and stable lifetime of every provider and task handle.

Host generated-schema real VFS tests execute the encoder and Task delivery
callback, including invalid codec, malformed CBOR, canonical trailing-byte
rejection and correctly checked finite generation rollover. Prior
`AsyncCopyingTaskSinkTests.cpp` independently qualifies real EDP-Threading
Task lifecycle, Memory-copy ownership, cancellation, backpressure and release;
combination under actual production scheduling/physical crash remains a
release gate, not inferred from distinct host validations. WROOM worker stack,
physical scratch and GC/wear bounds also remain open; versions remain 0.1.0.

The native WROOM 4096-byte payload test found the former stack-backed nested
archive path unsafe at main task stack 8192B. Production archive workers
**MUST use** the `WithScratch` typed publication and Task delivery adapters,
provisioning external `EdlsHeaderBytes+TMaximumPayload+20U` archive scratch
separate from live EDLG source-frame bytes. Both original convenience APIs
are retained for compatibility but do not carry an ESP32 high-payload stack
budget promise. See [protected rolling](EDLS-PROTECTED-ROLLING.MD).

## 27 October 2026: complete typed hardware VDD and independent physical decoder

After an initial test-fixture failure (native HALT TYPED_PREPARE; **no VDD cut**, run 20261026T131102-e6b39b9f FAIL), the generated HIL fixture now initializes an empty BasicLogRecord for each emitted generation, as required by the source-assembly contract. The corrected exact native ESP32-WROOM firmware was compiled successfully in run 20261026T131821-14f5f33a and subjected to the genuine GPIO17-controlled physical VDD cut **20261027T114439-028890f5 PASS exit0**: 8.254s independent OFF interval, new ROM POWERON_RESET and ESP-IDF reset=1, CRC/typed recheck of immutable g1/g2/g3, recovery of pending retirement only after successor verification, final finite retained g3/g4. Firmware checked all 15 declared fields and the generated argument values on both sides of reset.

**Independent physical readback**: RPI400 run 20261027T115023-aead4879 read 589824B at physical partition offset 0x170000, raw SHA256 **2754ea8b07e51d0756b39035ca864fd9465d1f1c047edca972f4374e266dd263**. The initial offline auditor correctly exited FAIL because its assumed wire fixture was WRONG: a never-present optional synchronized timestamp (FieldId 7) is canonically *omitted*, and std::array<uint8_t,16> diagnostic fingerprint (FieldId 12) serializes as a sixteen-element numeric array, not a CBOR byte string. No physical data, firmware or production serialization contract was modified to appease the auditor. The independent auditor has since been corrected to require exact mandatory FieldIds 0..14 except absent 7, explicit per-field values, and immutable owner-generated 16-byte fingerprint **[197,113,70,141,242,104,131,221,93,63,240,70,7,164,83,169]**. Rerunning against the SAME raw physical dump produced **OFFLINE_TYPED_EDLS_AUDIT=PASS oldest=3 newest=4 fields=15**, and the full 50 host offline negative/controller tests PASS. Original auditor FAIL transcript remains preserved; do not relabel it. Both host-verified full typed 15-field source ownership and on-media envelope are now qualified at this one inter-operation cut boundary, NOT at write/GC interruption.

**Actual existing owner lifecycle**: additional test and CI runner integrate a genuine Bootstrap-created EDP-Memory ObjectPool, OwnedRepresentationFanout canonical TypedEnvelope encoder, AsyncCopyingTaskSink using EDP-Threading Task handles, and post-completion EDLS physical archive. The deterministic test executor intentionally delays Task completion, verifies the source may disappear without invalidating copied CBOR, verifies no durable ACK at Task admission, then reopens actual filesystem across 16 finite generations. GCC 20261027T114556-29d189da, Clang 20261027T114556-af142881, GCC UBSan 20261027T115520-1d7f297c all PASS. **This tests the real Task handle ownership under a deferred test scheduler**, not a fully scheduled/free-running on-target worker and not saturation throughput.

Open production blockers remain cut-during-actual-write/GC behavior, provenance-authorized automated torn-candidate handling, real flash reserve/endurance measurement, full native worker scheduling/saturation/stack and G1–G15 release closure. Existing Persistence FileStorage interruption guarantees remain none/may-modify. All EDP versions remain 0.1.0.

## Strict NVS-acknowledged full typed LogRecord delivery

The existing `TypedCborProtectedEdlsTaskDelivery` generated 15-field TypedEnvelope adapter now supports an **optional compile-time injected** `PowerLossAcknowledgedEdls` owner. This is not a second CBOR serializer or Task/sink implementation: every code path shares the exact existing EDP-Serialisation canonical typed-envelope decode, re-encoding validation, EDLG frame writer and EDP-Threading Task callback. For the strict owner ONLY, `RecordSinkSubmission::Accepted` and `TypedEdlsTaskCompletion::DurablyAcknowledged` require a successful NVS PowerLoss-atomic acknowledgement commit/readback; the older FileStorage-only path retains its preexisting `Archived` completion and weaker durability semantics. In either mode, admission to `AsyncCopyingTaskSink` is NEVER a durability ACK. `AcknowledgedPendingRetirement` is accepted only after a committed watermark and verified successor; later publishing must successfully resume retirement first.

`tests/GeneratedEdlsStrictAckTaskIntegrationTests.cpp` reuses the actual existing EDP-Memory pool, bounded lease ownership, owner-generated Localisation schema and full 15-field canonical CBOR source, EDP-Threading deferred Task executor, POSIX genuine EDP-Persistence FileStorage and explicit PowerLoss-capability EDP-Persistence KeyValueStorage fake. It validates 17 separately Task-delivered generations, cross-boot incarnation 7->8, exact rolling physical latest two EDLS segments, no ACK on pending admission, terminal ACK only AFTER NVS commit, no false Accepted if KeyValueStorage StoreValue fails, correct failure terminal and retries after safe candidate reclamation. This is genuine C++ owner-composition testing but **not yet** proof of a deployed ESP32 scheduling, flash/worker stack, or wear budget; native HIL separately exercises strict typed NVS and EDLS on the physical chip.

Versions 0.1.0 unchanged; no new allocator, thread, codec, persistence store or SDK imports in production Logging code.

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
