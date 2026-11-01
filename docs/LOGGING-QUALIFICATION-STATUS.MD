# Logging qualification status

Branch: feature/logging-v1. All EDP library versions remain0.1.0.
Execution host: authorised RPI400. Hosted GitHub Actions is not a coverage
dependency; the user confirmed its monthly action budget was exhausted.

A successful row qualifies the stated configuration and finite experiment.
It does not imply that every configuration or all production gates pass.

| Area | Verified evidence | Scope still open |
|---|---|---|
| Bounded batch integrity and completion | GCC/Clang/UBSan owner tests; complete-frame and seal CRC, source validation, finite retention, strict NVS ACK and uncertain-commit refusal | Application capacity, resource and shutdown selections |
| Synchronous64-record physical batch | 256 records at132.828 records/s; independent original15-field/NVS audit;8.220s POR-confirmed cut and recovery, sequence20261030T145734-fce800f4 | Scheduled workload and arbitrary write/GC timing are separate |
| Existing managed coordinator, O2/500ms/160MHz | Two50Hz producers,2000 scheduled records; all2129 accepted phase records retired and durable; raw media ACK35,20261030T180038-622f5b3b | This20-second sample does not establish an indefinite100records/s rate |
| Longer managed workload |6000 sources offered at100records/s over60seconds; all6129 phase records retired/durable, raw media ACK97;232528-9e980d90 PASS | Indefinite sustained operation and remaining interruption/endurance gates |
| Five-minute managed workload | 30000 scheduled sources/all30129 accepted phase records durable;103339-6856faf0 PASS, rawACK472/124originals, finite drain300.949900s | Named five-minute profile only; explicit last6000 percentiles, no indefinite/lifetime claim |
| Three-target container demo | Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF builds183532-8bfb4a6b; actual two-boot tests184352-e1b78bb7 all PASS | Small CBOR container values; see the separate full typed managed row |
| Full typed managed demo | All three builds and actual two-boot tests 20261031T001504-eee39309 PASS; RAM/retired/durable distinctions, original replay and teardown | This finite demonstration is not endurance or interruption evidence |
| Canonical lifecycle atomics | Standard and canonical bindings pass GCC/Clang UBSan and actual ESP-IDF provider integration; SDK atomic flags retained | Lock-free does not promise wait-free per-caller timing |
| Public provider boundaries | Existing Memory, Threading, Clock, Serialisation, Localisation and Persistence owners reused; no FileStorage PowerLoss trait upgrade | No claim of generic filesystem interruption atomicity |
| Sealed64 write-activity cut | 20261031T003134-4bb013c2 and004328-1221099b PASS;8.232/8.257s externally verifiedOFF, protected originals recovered, subsequent128-record/NVS raw audit PASS | Write-activity window only; managed queue, exact flash-op/GC timing and endurance remain separate |
| Managed worker write-activity cut | 20261031T095844-2022f045 PASS;8.223s externally verifiedOFF, raw comparison of all128 original source coordinates/full15-field records and NVSACK2 CRCs | Finite write-activity window; retirement, exact GC timing, near-full and endurance remain separate |
| Managed near-full refusal and cut | 20261031T100726-066996bc PASS;4096bytesfree, realIoFailure12,52pending records retained/noACK3,8.243s POR-confirmed cut and128-original/NVS raw audit | Finite capacity failure and recovery; precise GC timing and endurance remain separate |
| Shared64-record committed-retirement gate | 20261031T102037-886599f8 PASS;8.279s POR-confirmed cut atNVS3/pre-unlink, exact2/3 recovery, ordinaryACK4 and128-original/NVS raw audit | Deliberate pre-unlink boundary; exact interrupted unlink/GC instruction and endurance remain separate |
| Physical reliability campaign | Existing complete-candidate, candidate-write activity and near-full experiments retained in native fixture README | Managed write-activity window now passes; shared retirement boundary now passes; exact GC timing/endurance qualification remains |
| Immutable byte capacity | GCC/Clang boundary/recovery and admission tests,134 Python tests,three-target builds; physical/raw124610-2a2c297e PASS,27088 B peak <=28000 B | Logical sink data including successor/ACK; physical provider overhead is separate. Lifespan guarantees are outside scope |

## Current application contract

The application chooses the persistence medium and supplies a byte limit during Bootstrap or before initialisation; that limit is immutable after initialisation. Explicit byte-limit enforcement now passes host boundary/recovery checks and actual ESP32 qualification: 28,000 B configured,27,088 B peak including successor and ACK,13,032 B final logical data. See [contract and evidence](IMMUTABLE-PERSISTENCE-BYTE-CAPACITY.MD). Provider-internal physical allocation overhead remains separately budgeted.

The required throughput target is 100 records per second. The application need not select a platform-wide retention amount. Operational lifespan and flash endurance guarantees are outside scope; the two-year target has been withdrawn. Substantial production logging should ideally use separate storage rather than ESP-integrated flash. More allocated physical storage can distribute wear over more blocks, depending on the provider; it does not necessarily reduce total writes.

This decision supersedes earlier references below to endurance as an outstanding acceptance gate. See [application storage and wear guidance](PRODUCTION-RETENTION-ENDURANCE-REVIEW.MD).

## Qualified extended finite experiment

The original pressure test remains128 accepted sources plus32 finite deadline
refusals before worker start; the isolated idle partial remains one source.
The extended phase then schedules3000 records on each of two producers at50Hz.
The actual successful run confirms all6129 accepted records retired and durably completed,
with finite quiescence and teardown, native original-field replay and independent
physical LittleFS/NVS parsing. The actual application command proves
O2, enabled level2 assertions,160MHz,500ms dwell and3000 sources per producer.

The first extended build231625-9890b348 FAIL exceeded static DRAM by6472B
because test telemetry grew with the workload. No hardware execution occurred;
its prerequisite parent231749-08442200 also failed before hardware.
Corrected evidence history uses512 ticket-tagged slots, bounded above all
queued/pending/retained needs, while retaining all6000 exact latency samples.
Stale coordinate reuse fails explicitly. The production queue, batch size,
codec, validation path and source/representation Memory topologies did not
change. The original2000-record environment remains selectable.
Physical run232528-9e980d90 PASS: complete drain61.096605s; ACKp50812.873ms,
p991257.849ms; sampled minheap162096B, worker stack3308B/main4740B.
Independent media validates124 retained original records and official NVS CRC
ACK97. This is finite60-second evidence, not indefinite sustained rate.

## Evidence locations and interpretation

[Batching contract](BOUNDED-EDLS-BATCHING.MD) specifies volatile admission,
durable completion, sealed segments, retention and configuration.
[Native fixture](../tests/hil/g7-littlefs-protected-rolling-espidf/README.MD)
preserves measured values, hashes, failures and electrical classifications.
[Container demo](../demos/ConfigurableBoundedEdlsBatch/README.MD)
contains exact three-target build and two-boot evidence.

Private flash images and UART transcripts remain on RPI400. The living
Policy handoff records current source commits, appliance ownership and active
run IDs. Pending rows must be replaced with actual outcomes before claiming
those gates passed. Logging is not yet declared production-ready.

[Five-minute scope and measured results](LONG-MANAGED-WORKLOAD-QUALIFICATION.MD) and [production retention/endurance inputs](PRODUCTION-RETENTION-ENDURANCE-REVIEW.MD) distinguish tested configurations from product acceptance.


## V2 explicitly numeric result enums — 1 November 2026

The existing Logging result classifications are now assigned their previous
numeric values explicitly in EDLS archive, generated projection, record
production/fanout, checkpoint, durable snapshot and positional reencoding
owners. This applies source naming/formatting policy without renumbering
earlier outcomes, adding statuses, changing serialization representations
or upgrading any persistence guarantees. The source-change run validates
the same host and typed/strict EDLS owner tests against current pinned
dependencies; no new power-cut claim follows from enum spelling.


## 1 November 2026 current-source acceptance: 30,000-record negative

The current production C++ implementation at `26cc69a4ca3531468e1c70757a36768cb54fa43a` and later documentation/test-only commits remains version `0.1.0`. **Production readiness is NOT declared.** The final selected native host regression `20261101T110807-8edfa5eb` passed **68/68** GCC/Clang scripts; the full Python offline suite `20261101T110239-ffd35f08` passed 178 tests; the Xtensa queue/RAII syntax test `20261101T110944-9006c76a` passed. Final managed 30,000-record firmware `20261101T112030-f19f4a1c` built successfully with -O2, Level2 assertions, 160 MHz and 500 ms batch dwell. G11 same-source whole-app off/on linked both variants and the corrected post-link evidence validator `20261101T143026-134bbf84` PASSED, text delta +132 B, data/BSS unchanged. G11 runner source correction was independently published as `a4ef4fc0e47fea01e909072d28e8917323beac32`; the earlier invalid 8192B stack assumption was corrected to the real 14336B G4 application fixture.

**Physical PowerLoss and independent media:** `20261101T112515-5db8d6b5` PASS, GPIO17 OFF verified **8.619s** and POR confirmed; 4096 bytes free, 52 pending, provider replacement failed, 128 records protected through independent ACK2 across two recovered contiguous generations. Raw first audit `20261101T112958-6226b621` FAIL due esptool read digest mismatch; separate non-overwriting retry `20261101T143128-72ce9400` PASS, real LittleFS 589824B and NVS 24576B, 128 original records independently decoded and compared across all 15 fields, ACK2/Incarnation7 official NVS page/entry/blob CRCs PASS. Generic FileStorage replacement remains unsupported for PowerLoss atomicity; no flash endurance/lifetime guarantee.

**NEW BLOCKER, not a PASS:** `20261101T143310-918c289b` **FAIL exit1** on exact current-source managed 30,000-record / 100 rps physical acceptance. Approximately the first 12,000 scheduled records progressed at 100 rps with zero failures. Then **three phase3 producer failures** appeared: `RecordProductionStatus::AdmissionFailed=8`, `RecordAdmissionStatus::DeadlineElapsed=1`, `RecordQueueResult::CapacityUnavailable=2`, `MemoryAttempted=0`, `Commit=Empty=1` and `reservation_active=0`. In the fixture `batch_worker_probe.hpp`, each producer uses a 100 ms absolute monotonic deadline `After(100000000U)`; the queue was not available by that deadline. This is a real missed-bounded-admission service objective, **not** evidence of broken NVS acknowledgment or corrupted persisted records. Output on fail: `SCHEDULE_RESULT|published=13078|refused=32|failures=3|retired=12950|accepted=12950|durable=12917|partial=0|exit=3|phase0=3|phase1=3`. The observer halted on failed acceptance; it never certified a full five-minute drain. Actual root cause of the long-tail queue unavailability **not yet identified**; do not arbitrarily lengthen deadlines to make test pass. Investigate coordinator/Threading/Memory wake timing, native LittleFS NVS replacement and segment retirement service tail distribution, and queue capacity before proposing bounded design/config changes; make detailed instrumentation reproducible, then repeat physical 30,000 records with independent raw-media audit. Historic 2026-10-31 30,000-record passing experiment remains valid only for its exact earlier tested run.

**Appliance preservation:** independent pre-flash 4MiB read digest `a6e554df77d3efde40878a8cc3dfd665330b03f37a93a1529326d0334779c87a` and second independent 2MiB backup match across first 2MiB. Physical chip reports 2MiB to ESP-IDF; restoration targets exactly these 2MiB of altered boot/application/NVS/LittleFS, verifies through esptool, then independently reads and compares all 2MiB. Restoration run `20261101T143758-18c763af` **PASS exit0** at 14:41:45 CET: original 2MiB pretest firmware/NVS/LittleFS image written and verified, independently reread all 2,097,152 bytes and byte-compared exactly, SHA-256 `ae95c91a3dcb1be56fb9440707507b2441d84e0d21caa98c1709752c04904fb5`; GPIO17 controller powered OFF after restoration. Original 4MiB archival read remains preserved without attempting an unsafe full 4MiB write to a device whose detected Flash capacity is 2MiB. Keep Power OFF and release exclusive RPI400 lock after verification. The final Policy/main living handoff records the source and evidence IDs.
