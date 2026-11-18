# Logging qualification status

Maintained source: the checked-out Logging branch; authoritative source revisions and main publication are tracked below. All EDP library versions remain **0.1.0**.
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


## 2 November 2026 — current-source 100rps two complete physical passes

Physical ESP32-WROOM-32 integrated target and strict LittleFS/NVS storage: detached 20261101T193528-695b02cf and 20261101T194232-bc98b8ae both PASS exit0, exact 30,000 scheduled at 100/s, all 30,129 phase records accepted/retired/durable, zero unexpected failures, actual direct independent LittleFS/NVS CRC and 125 retained 15-field original-record comparison PASS, ACK472, minimum free heap 134060/134084 bytes. Exact native metrics and earlier failed configurations appear in LONG-MANAGED-WORKLOAD-QUALIFICATION.MD. The previously dated physical FAIL results remain correct for their earlier code and queue configurations; this newer scope supersedes only their conclusion for the currently tested finite topology. Native-source final qualification requires code publication, current source regression and post-change real PowerLoss qualification.


## 2 November 2026 — verified optimized PowerLoss, host and strict raw-media acceptance

The published feature/logging-v1 code at 9fd4956a195d83e15cc53dad08d7743688a59833 passed two genuine full 100 records/sec physical WROOM tests: 20261101T193528-695b02cf and 20261101T194232-bc98b8ae, each with 30,129 accepted, retired and durable records, 125 independent retained originals checked across all 15 fields and strict LittleFS/NVS ACK472 with page/entry/blob CRCs. Heap and stack remained finite with minimum free heap 134060/134084 bytes. The same current source passed 66 GCC/Clang host script invocations 20261102T083638-7e077e59 and 137 Python offline tests 20261102T084111-ff0df6ef. Six specialized firmware/xtensa-specific scripts were not part of the 66.

Actual optimized PowerLoss physical capacity-cut firmware built PASS 20261102T083816-1d0455ef. The first two attempts 20261102T084902-173395cc and 20261102T085423-20f22423 FAILED before relay switching, each reaching first 64 acknowledged sources but timing out the second half of initial 128-record pressure drain; preserve these real negative receipts. Test-only read-only pressure-drain instrumentation was added without changing the strict finite 15sec gate, source code or coordinator scheduling. The subsequent physical GPIO17 test 20261102T090006-e5a4b0db PASS exit0: durable ACK2, 128 protected records, safe near-full storage (pending 52, native replace status12, free 4096 bytes), genuine 8.260sec OFF, independently observed ROM PowerOnReset and app reset reason1, contiguous protected generations1,2, and post-recovery all15-field verification.

The first offline raw-media audit wrapper 20261102T094814-92f69ff8 exited1 after creating the real raw LittleFS/NVS images and extracted files, but the actual constituent integrity audits were each separately verified PASS. A consolidated, independently detached rerun 20261102T094916-cd7e45da PASSED exit0, verifies original raw-image SHA digests, 128 actual original 15-field records and genuine NVS namespace batchack generation2 page CRC, entry CRC, blob CRC. Additional independent LittleFS extraction 20261102T094958-b9fe17ac PASS and byte-identical retained slot0 and slot1 to first extraction. Raw Flash images and original UART remain unchanged. This is a real relay PowerLoss result, not a reset imitation.

**Qualification scope:** Finite bounded structured Logging with application-selected storage media, queue capacity and immutable logical byte limit is production-capable for the specifically qualified ESP32/strict LittleFS+NVS profile. It does NOT guarantee global/release-wide G1–G15 closure for all providers, arbitrary FileStorage PowerLoss atomicity, indefinite 100rps operation, all Serial options, Flash wear/lifespan, or all Clock providers. Final V2 owner/documentation compliance and any non-core optional integration remain separate review obligations. All versions remain 0.1.0.

## 12 November 2026 — current bounded native qualification and dependency-main regression

The 1 November **failed** 30,000-record timeout and first physical powercut wrapper failures remain correct *dated negative* observations. However, two later exact-source full WROOM-32 scheduled `100/sec × 300sec` runs `20261101T193528-695b02cf`, `20261101T194232-bc98b8ae` independently **PASS**, with all **30,129** phase records accepted/retired/durably completed, zero unexpected producer refusals, **125** retained original fifteen-field records checked by separate raw LittleFS/NVS/CRC auditors, ACK472, and bounded heap. The later `20261102T090006-e5a4b0db` physical **GPIO17 8.260-sec OFF with actual PowerOnReset** verified strict ACK2 and all 128 protected original records after restart. This is a scoped, finite, application-selected physical durability performance profile and **not** a promise about every native Flash state, indefinite rates, lifespan or generic VFS atomicity.

Current published owners selected by the optional generated/sink profile are `EDP-Localisation/main`, `EDP-Clock/main`, `EDP-Serialisation/main`, `EDP-Platform-Portable/main`, `EDP-Memory/main` and `EDP-Threading/main` at the exact SHA pins documented in [G14 owner baseline](DEPENDENCY-OWNER-BASELINE.MD). Independent RPI400 owner/regression gates **PASS**: 8/8 typed generator and ClockEra current-main checks `20261109T090236-f9d01ff1`, 16/16 new minimal Logging+Memory native examples `20261109T114835-9efcac49`, **79/79** post-Memory main host/native `20261110T141859-4bbe5666`, and **81/81** post-Threading main (all 3 Logging demo families × both PIO targets + selected 62 Logging host + foundations) `20261110T144522-0b07e433` **PASS exit0 at 15:28:44 CET**. Actual UART and Flash were not modified by the 9–10 November native *build-only* checks.

**Remaining core publication checks as of this document update:** complete V2 readme/examples/Wiki source-level semantics, exact current-feature Python tool runner after comment-only edits, reconcile generated status/branch descriptions, guarded `EDP-Logging/main` fast-forward (retain source branch and old main), and exact **post-Logging-main** host/native/downstream optional Serial consumer qualification. Only then assign a version 0.1.0 **feature-complete / scoped production-ready** status for the tested profile, if results pass. Never claim broad provider/runtime guarantees from finite tests or erase earlier fail receipts. NVS ACK counts **segments**, not admitted source records; the application selects immutable finite retention and storage placement, with separate Flash preferred for heavy production workloads.
## 12 November 2026 — final Logging main post-integration acceptance

Authoritative source: `EDP-Logging/main=9fea51b0bdebcd8a4206628b1e1def81ff620381`, matching the retained `feature/logging-v1` branch. Nonforced fast-forward from old stub `bbd0c1b53de347057d9573c6f324871bbf483012`; no branch deleted, no release tag, all versions 0.1.0. This is the *code-equivalent* tested release baseline; subsequent documentation-only commits do not change C++ production algorithms, tests or native demo source.

**Post-main independent test:** RPI400 detached `20261112T092223-3f2f828e` finished **PASS exit0, 09:34:12 CET, 83/83 checks**: 34 GCC + 34 Clang Logging host scripts, eight optional EDP-Serial/Serial-ESP-IDF feature-branch host contract checks, six real PlatformIO native compile/link builds (three Logging demo families × Arduino-ESP32/ESP-IDF) and one Python suite. Exact current production source had 42/42 Git blobs independently matched to GitHub prior to main promotion. Full TSV and per-case logs: `/srv/espressio/scratch/gpt6-logging-closure-20261109-1115-m8q2/receipts/logging-postmain-20261112-retry/`.

**Additional post-main source/tooling hygiene:** Final Python `unittest` ran **40 tests, OK** after two Python docstring/CLI-description-only corrections. Five isolated `git check-ignore` probes for macOS, Windows and PlatformIO generated files passed. The first post-main runner `20261112T091916-b371e41e` failed **before any source tests** owing to hanging direct GitHub `git ls-remote` from RPI400; preserved as infrastructure-only evidence. A separate initial ignore probe failed because the isolated source was not a Git checkout; the corrected temporary Git repository passed. Neither is a Logging implementation failure.

**Hardware safe state independently verified 12 Nov 10:36 CET:** `LOCK_PRESENT=NO`, status LED `SESSION=INACTIVE`, integrated ESP32 `POWER=OFF GPIO17=HIGH`; no new flash or physical relay cut during this main-merge qualification. The earlier two real finite WROOM 100-rps × 300-s full accepted/durable trials and independent GPIO17 8.260-s OFF/POR/strict LittleFS+NVS ACK2 retained-record audits remain the only physical basis for their respective bounded claims.

**Explicit qualification boundary:** Local C++20 Logging source is now integrated to main with independently passing selected host/native/downstream tests and pre-existing finite physical data-contract evidence. It is a *scoped, configuration-qualified implementation*, not an assertion of universal production readiness or all-provider PowerLoss atomicity, guaranteed flash life, indefinite throughput or indefinite retention. Full exhaustive V2 method/member semantic review of every one of 42 Wiki source references and global cross-repo audit remain open in Logging #15. Direct native EDP-Memory finite/Forever failed-relock waiter safety remains tracked separately in Memory #16 (Logging uses the NoWait/wake path); the initial downstream runs used the Serial feature branches. The later Serial integration section records their main promotion. Serial-Arduino remains deferred. Do not close #15 or create a release tag solely from the 83 passing cases. Historical failed tests and earlier dated blockers remain in their original context.

Full chronology, source pins and RPI400 receipts: [Policy/main Logging living handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-09/2026-11-09-09-29-EDP-Logging-Dependency-First-Reintegration.MD).

## 12 November 2026 — physical Serial integration and downstream main acceptance

The WROOM fixture `20261112T120312-447090b1` passed **20/20** with zero failures, independent UART EDLG CRC/payload capture, resource snapshot and G11 site evidence. The test exercised the current FIFO provider contract with Logging fixture commit `2623d2d2f96fc6ec1f0e1966a1a62bbd040131e7`. Its original 4 MiB flash image was restored and independently verified, and the board returned to power off. This fixture included a software restart. The separate recorded relay PowerLoss tests continue to govern the durable storage profile.

See [Serial output integration](SERIAL-OUTPUT-INTEGRATION.MD) and the [dependency owner baseline](DEPENDENCY-OWNER-BASELINE.MD) for the exact post-main host/native evidence and application ownership.

## 12 November 2026 — EDLS typed revalidation checkpoint

Logging source `8fb097ca26c9d4f034c57c539fd5eaed97394b27` passed the selected post-main host and Serial consumer checks on RPI400. The exact preceding archive candidate passed the strict typed-ACK-gap ESP32 fixture with an 8.273 s POR-confirmed GPIO17 cut. Independent LittleFS and NVS reads verified generations 3 and 4 and watermark ACK4. The full original 4 MiB image was restored and verified, and the integrated ESP32 was powered off. See the [G1–G15 qualification ledger](ACCEPTANCE-G1-G15.MD#12-november-2026--strict-edls-predecessor-revalidation) and the [living Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-12/2026-11-12-11-28-EDP-Logging-Serial-Release-Recovery.MD) for the run IDs and tested configuration.

## 13 November 2026 — release-readiness checkpoint

Both Logging branches include the typed queue borrow contract introduced by
`d6056ed0e463e05d512f90283bc13fe405a71ec6`. Callers inspect `Status()` and use
`Lease()` only on success. The cross-repository audit found two active caller
files, both in Logging's tests. EDP-Serial consumes the EDLG frame contract.
The queue GCC, Clang, UBSan and Xtensa checks, native G1 build, and downstream
Serial frame-pump/FIFO tests passed. The three demonstration families
compiled and linked across all nine Arduino IDE, PlatformIO Arduino and
PlatformIO ESP-IDF combinations. The [living Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-13/2026-11-13-08-38-EDP-Logging-Serial-Qualification.MD)
records the exact revisions and detached run IDs.

Run `20261113T110737-c123431b` passed ten GCC and Clang checks covering the
coordinator quantum, Memory NoWait admission, wake and release publication,
admission shutdown and producer coordination. These are host checks. The
strict LittleFS/NVS, GPIO17 PowerLoss and UART receipts above govern the
physical scope. The complete V2 reference review remains tracked in
[Logging #15](https://github.com/ESPressio-Development-Platform/EDP-Logging/issues/15).

The full host matrix on exact baseline `c80a80aae52362c79af1443bdf68897fca590d6c`
completed in RPI400 run `20261113T110345-92db528b`. All 34 GCC and 34 Clang
test runners passed, along with all 40 Python tests. The later diagnostic
API migration has its own focused qualification receipt in the handoff.

### EDLG and EDLS ByteOperations migration

Logging `5d854d7c7d6d96af0041c9943e50e129f9e5df36` and downstream Serial
`b021ff65fb405937ce2d097d17abe0d59422556e` were published to both active
branches. The opt-in writer borrows the selected Memory byte-copy
provider. Archive copies use the same owner contract and reject overlapping
frame and archive workspace before recovery. The default Logging umbrella
and reader remain dependency-free.

RPI400 run `20261113T115314-5fbcbbf0` passed 68 GCC/Clang runners and 40
Python tests. `20261113T123917-dd8fc7b0` verified the published refs and
passed four Serial pump/FIFO tests. The physical UART run
`20261113T121301-dffea777` passed 20/20, including an independent EDLG CRC
and the WROOM resource and G11 site checks. A fresh verified 4 MiB backup
preceded programming. The original image was restored and verified, and
the board was powered off. The separate strict LittleFS/NVS relay-cut and
finite 100-record/s receipts govern the storage claims.


All nine Logging demo target builds and all six downstream Serial demo
target builds passed on the ByteOperations pair. The three
`FramingAndInstrumentation` results are in `20261113T115541-ab83105e`.
The initial paired run then resolved the wrong Logging sibling in one
PlatformIO build. A corrected sibling checkout in `20261113T121711-ce7e6c43`
passed both Logging managed demos and both Serial demo families on
Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF.


The strict LittleFS/NVS fixture linked from Logging `5d854d7` in
`20261113T125037-f7525d43`. The first physical run,
`20261113T125128-a24d7661`, confirmed a power-on reset after 8.267 s off and
correct firmware replay. Serial noise interrupted the independent LittleFS
capture. The original flash was restored and verified.

Repeat `20261113T131942-75630da5` cut at the complete generation 3
candidate with NVS ACK2. The external controller measured 8.283 s off.
The new boot epoch and ROM and ESP-IDF reset reasons confirmed a power-on
reset. Independent LittleFS readback verified generations 3 and 4, all 15
original typed fields, and the archive and frame checksums. Independent
NVS readback verified page, entry and blob CRCs, ACK4 and incarnation 7.
The raw read used fixed-size chunks and baud-rate fallback.
The original full 4 MiB image was restored and verified. The target was
powered off before the job completed with exit 0.


## 18 November 2026 — EDLS readback and declaration verification

Logging 8d5474d499a0ba00a1fcdd1b88175560a59454da routes oldest-eligible
rotation and sealed successor readback through the existing stateless
EDP-Memory ByteOperations provider. It preserves the two readback
statuses and the predecessor's typed header and prefix checks. The new
negative fixtures store a different but CRC-valid frame after native write
acceptance. Both publication paths return ReadbackUnconfirmed, and
oldest-eligible rotation independently revalidates the protected newest
slot. The exact source passed 34 GCC runners, 34 Clang runners, and
40 Python tests in RPI400 runs 20261118T112133-adf36814 and
20261118T112133-1e85e5c4.

The selected 11 EDP domain manifests form a closed, acyclic dependency
graph. The default Logging umbrella and package manifest have no
mandatory EDP library imports. For the optional native scopes,
EDP-Platform-ESP-IDF remains pinned to
adc4164031016c200ad88d742fc4f05c86d66833 for the qualified Clock and atomic
provider. The other 15 selected owner revisions match their refreshed
main refs. The direct native EDP-Memory blocked-waiter failure case
continues under Memory #16. Logging admission uses the NoWait and
Threading/Clock bounded coordination profile.

The latest declaration review corrected the severity predicate,
generated archive adapters, and selected bounded admission, queue,
snapshot and wire conversion descriptions. All 42 production headers
have indexed branch-local source links in the 62-page repository Wiki.
The documentation pass resolved all 558 local links in the Wiki, README,
dependency graph and focused docs. The per-declaration semantic review
of the remaining Logging and affected owner surfaces is tracked in
[Logging #15](https://github.com/ESPressio-Development-Platform/EDP-Logging/issues/15).
The exact Xtensa syntax and compile-out checks passed in
20261118T113942-907bb482. The prior named physical results above apply to
their recorded source and configuration.

## 18 November 2026 — exact single-frame readback and 100 records/s retest

Logging `main` and `feature/logging-v1` both published executable source
`be1b2bac863c13c4d5751612c788823dc75b432b`. The preceding release baseline
`76e6986` passed 34 GCC and 34 Clang host runners and 40 Python tests on RPI400.
The new regression reproduced acceptance of a different same-size CRC-valid
single frame on the previous source. The correction compares the intended
frame with the verified whole-file capture through EDP-Memory before any
acknowledgement or retirement. It adds no FileStorage read for a single
frame. The sealed publication path retains its existing readback.

RPI400 `20261118T121326-743cb89a` passed 16 focused GCC and Clang archive,
VFS, generated-output and strict ACK checks. The exact native
`esp32dev-typed-strict` build `20261118T121438-6969a5f1` passed under
ESP-IDF 5.5.5. Its factory SHA-256 was
`5e34d7d0303bea1f2f9f99fb7ded3c44f8fb91f5457f31575753154f7807e88c`.
Guarded physical run `20261118T121439-eaf7b495` passed after an **8.382-second
GPIO17 power cut**, initiated with ACK2 and a complete generation 3 candidate.
Both ROM and application reported power-on reset on the fresh boot. Independent
LittleFS readback confirmed generations 3 and 4, including all 15 typed fields.
The independent NVS read confirmed ACK4, incarnation 7, and valid page, entry
and blob CRCs. The original full 4 MiB flash image was restored and verified,
then the integrated target powered off.

The **252-slot 100 records/s workload failed** on executable `5e6aaa2` in
`20261118T121219-9a597074`. The fixture was configured to offer 30,000 records
over 300 seconds. It halted after reporting 25,922 published, 25,670 accepted and
retired, and 25,653 durable records. Three producer attempts hit their 300 ms
deadlines while the queue was full, before attempting Memory acquisition.
The measured maxima were 493.639 ms for a native write and 707.233 ms for
worker service. The combined receipt establishes an admission/throughput
failure on this run. The workload halted before its independent media
audit. The guard restored and verified the original flash and powered off.

Historical 100 records/s passes and the separately qualified strict
power-cut result retain their exact source and configuration scope. This
fixture selects 160 MHz, O2, assertion level 2 and a 500 ms batch dwell.
Current release tracking remains in [Logging #15](https://github.com/ESPressio-Development-Platform/EDP-Logging/issues/15).

The existing 496-slot dual-pool profile also stopped at full-queue admission
deadlines, after 21,141 publications on exact executable source `be1b2ba`.
The [workload report](LONG-MANAGED-WORKLOAD-QUALIFICATION.MD#18-november-2026--guarded-100-recordss-workload-repeats)
records both trials, independent flash restoration and their distinct timing coverage.

The matched 252-slot and 496-slot trials completed and independently audited the
30,000-record workload. The later optimised 252-slot trial confirmed one fewer
native read per sealed successor, then stopped at a full queue after the
coordinator reported ReclaimUnconfirmed. Its exact sealed power-cut and raw
media test passed. See the [matched timing and readback record](LONG-MANAGED-WORKLOAD-QUALIFICATION.MD#18-november-2026--matched-timing-and-sealed-readback).

## 18 November 2026 — scoped 100 records/s gate

Scoped release fixture `b78a736` finished all 30,000 scheduled records
at 100 records/s and verified the retained LittleFS data and raw NVS ACK472.
It uses a 20 ms coordinator mutex budget with the 300 ms producer deadline.
GCC and Clang passed 34 runners each, and Python passed 40 tests. See
[scoped workload qualification](LONG-MANAGED-WORKLOAD-QUALIFICATION.MD#18-november-2026--scoped-252-slot-workload-qualification).
