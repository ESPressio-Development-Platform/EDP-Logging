# Five-minute managed workload qualification

This profile extends the existing finite managed producer/worker fixture to15000 sources per producer,30000 scheduled records over300seconds at an offered100records/s. It reuses the same source/representation Memory topologies,128-slot queue, one DedicatedCoordinator, generated full15-field record codec,64-record batch, strict LittleFS/NVS archive,500ms dwell, applicationO2, enabled assertions and160MHz. No production API or provider changes; all versions remain0.1.0.

Select esp32dev-batch-worker-500-o2-30000. Native command verification requires the actual15000U per-producer define; short and long profiles cannot borrow each other's compile proof. The original2000/6000 modes remain selectable. The16-bit fixture asserts its finite ticket horizon;30161 is the last permitted ticket after pressure refusals.

The existing pressure phase remains128 successful volatile publications plus32 bounded deadline refusals before worker start. The isolated one-record idle partial still must flush without new input after its configured dwell. Both managed producers then run their ordinary50Hz absolute schedules. Successful completion requires all30129 accepted phase records retired, accepted and durably completed, with finite quiescence and actual Threading/Memory teardown.

Test telemetry remains bounded. The existing512 ticket-tagged coordinate slots still exceed queued/pending/retained evidence needs; stale coordinate reuse fails. The long profile stores exactly the final6000 occurrence-to-ACK samples. Its p50/p95/p99 explicitly declare latency_scope=last_6000 and latency_samples=6000. Separate aggregate count/minimum/maximum/mean cover all30000 scheduled records. Legacy2000/6000 profiles continue to report all scheduled samples. No last-window percentile is labelled an all-run percentile.

Every ten seconds Bootstrap emits native published/retired/durable/failure/heap progress, using the same canonical monotonic origin. This is test evidence, not a new logging queue or worker. Wall-clock stage timing includes scheduling and I/O; it is not CPU occupancy.

The existing physical runner holds ROM through full image/filesystem verification before boot. Independent raw auditing compares every retained full15-field record against original ticket/occurrence evidence and validates the official NVS watermark and CRCs. Finite retention means raw media contains the final protected window, not all30000 historical records; the native completion counters attest the full workload.

This is a finite five-minute experiment. It does not establish indefinite100records/s operation, every capacity/dwell/retention configuration, exact interrupted GC timing, or flash service life.

Execution status: host103255-e233b7ab PASS132. Native103039-ff4325a3 PASS with actual15000U command/O2/assertions/160MHz/500ms proof. Physical plus independent raw audit **20261031T103339-6856faf0 PASS**, completed31October10:46:17CET. All30129 accepted phase records were published, retired, accepted and durably completed; only the32 intentional pre-worker pressure refusals occurred, with zero failures/partial submissions and real cooperative teardown.

Scheduled30000-record drain300.949900seconds, or99.684366records/s including the final drain. All-scheduled occurrence-to-ACK minimum374848us, maximum1540196us and mean820230us. Explicit last6000 sample p50823952us, p951121836us, p991248615us. Maximum schedule lateness83006us, sampled minimum freeheap162704B, producer stack2916/2924B, worker3244B/main5068B. Producer/worker core masks3/3, queuecontentions1, source144B and queue2592B. Ten-second progress confirms bounded lag/no failures throughout the finite run.

Final strictACK472, two retained segments and124 original records independently compared across all15 fields including original occurrence coordinates. Official NVSACK472/incarnation7/page/entry/blob CRCs PASS. Final targetOFF independently verified.

FactorySHA256 6bdb389377b5f03a021daa0a8c2b49984059c60fac5c48c10471e51779a31f2e. LittleFSc4f6af5131e160d0828961f92d3b75a40108ec068ada2c7e8da0f5f2f1b0e51b. NVScc59386ad7bd7f62d2cf3fd85ec89b0fbfc5d92250575dbd5e143e5c8b7ee6c2. Private evidence remains on RPI400 in private-worker-30000-20261031-01.

Final receipt reader requires percentile scope in the actual PASS line, so a separate prefixed scope cannot qualify a misleading receipt. Final132-test regression plus a fresh read-only audit of the saved physical media,105342-cc6370ba, PASS. No firmware change or physical rerun was necessary for this stricter host-only check.


## 1 November 2026 — post-V2 current-source burst admission evidence and controlled 80 rps profile

The historic 31 October 100 records/sec result above remains **valid for its exact earlier image**, but it does **not** qualify the latest source, instrumentation, or an indefinite guarantee. The later current-source WROOM 100 rps physical acceptance \`20261101T143310-918c289b\` failed after 13,078 total admissions when three finite 100 ms queue deadlines expired. Following test-only monotonic instrumentation, repeat \`20261101T144948-68d8888b\` failed at roughly 8,308 admissions with actual service-call maximum 582,835 microseconds and source callback maximum 706,237 microseconds. Source pool acquisition was **not attempted** on each refused reservation.

Application-only bounded experiments preserved each real failure, without altering the production Logging API, immutable byte capacity, strict NVS acknowledgement, canonical 15-field record, 64-record EDLS batch, or the existing Threading coordinator:

| Physical run | Native WROOM profile | Result and decisive evidence |
| --- | --- | --- |
| \`20261101T145540-93f075e4\` | Original 128-slot queue; 300 ms *scheduled* admission deadline; pressure still exactly 100 ms with 32 genuine refusals | **FAIL**: 9,213 admitted, three scheduled admission deadlines, native service maximum 680,185 microseconds |
| \`20261101T151250-dd417dc5\` | Separate 252-slot queue and 254-instance EDP-Memory dedicated source pool (within the existing hard 255-instance limit); 300 ms deadline | **FAIL**: 11,504 admitted, three scheduled admission deadlines, 134,164-byte minimum free heap, service maximum 701,999 microseconds |
| \`20261101T153209-adc5d135\` | Separate 496-slot queue; two distinct EDP-Memory runtime *instances* of the same pool type, 252 source objects each; 300 ms deadline | **FAIL**: 21,753 admitted, four scheduled admission deadlines, 93,064-byte minimum free heap, service maximum 726,557 microseconds, 166,817,077 microseconds of measured native coordinator service time |

These are **not** source-memory exhaustion, confirmed record corruption, an acknowledged-segment loss, or success at the full workload. The available evidence instead implicates bursts of synchronous native LittleFS segment/strict NVS service that exhaust queue headroom. Increasing finite queue size delays but does not resolve them at 100 rps. The 496-slot mode uses two existing EDP-Memory owners and a single existing Logging queue, not a new Memory API or worker. The original 128-slot genuine backpressure/refusal profile remains separately selectable and is not silently recast as a pass in the 252/496-slot prime-only modes.

For a **distinct, lower application-chosen rate** an 80 rps physical target is built with 252 source slots, 254 source objects, two 40Hz native-clock scheduled producers, a 300 ms bounded scheduled admission deadline, 500 ms batch dwell, 64-record EDLS and unchanged strict Flash durability. Exactly 30,000 scheduled records now take 375 seconds, not 300; the same 129 initial/idle records are additionally required. Actual compiler verification explicitly checks the 80 rps macro versus the default 100 rps, and the UART PASS reader rejects relabelling. This is **not** a 100 rps qualification. A passing 80 rps run would qualify only that finite WROOM/internal-LittleFS configuration, not all production storage providers.

The authoritative 80 rps physical run launched under RPI400 exclusive lock at 18:55 CET as \`20261101T185538-b895ffad\`; its status, precise emitted statistics and independent retained-media audit must be appended from actual receipts before any PASS claim. All version numbers remain \`0.1.0\`. The Policy/main living handoff holds the private evidence paths and post-test restoration procedure.


## 2 November 2026 — current-source, twice-repeated 100 records/sec ESP32 physical acceptance

Two independent runs with the isolated retirement-read optimisation completed the exact 30,000 scheduled records on the integrated ESP32-WROOM-32. Application topology: 252-slot queue, 254-object EDP-Memory source pool, 300 ms bounded scheduled admission, two 50 Hz managed producers, 64-frame sealed EDLS, 500 ms batch dwell, strict NVS ACK over LittleFS, 28,000-byte logical retention, O2/Level2 assertions/160 MHz. All library versions remain 0.1.0.

| Physical receipt | Retired and durable | Scheduled drain | Last 6,000 p50/p95/p99 ACK | Min free heap | Independent raw media |
| --- | --- | --- | --- | --- | --- |
| 20261101T193528-695b02cf | 30,129/30,129, no unexpected failures | 300.900824 s | 895.360 / 1204.079 / 1349.799 ms | 134,060 bytes | ACK472, 125 retained originals, all 15 fields, genuine LittleFS/NVS CRC PASS |
| 20261101T194232-bc98b8ae | 30,129/30,129, no unexpected failures | 300.900847 s | 894.974 / 1206.078 / 1353.056 ms | 134,084 bytes | ACK472, 125 retained originals, all 15 fields, genuine LittleFS/NVS CRC PASS |

Both independently extracted raw LittleFS and NVS partitions from actual physical Flash against unmodified UART source-coordinate evidence. Both observed task stack high-water margins producer 2884/2892 bytes, worker 3068 and main 4952. Full scheduled min/max/mean occurrence-to-durable latency first run 397.928/1511.215/889.058 ms and second 398.601/1511.210/890.252 ms. Native GetFileSize 7544 calls/~24.25 s, ReadFileAt 5176 calls/~53.4 s, ReplaceFile 472 calls/~103 s, RemoveFile 470 calls/~4.95 s. These are operation wall times, NOT CPU occupancy.

Prior 128/252/496 queue 100rps and a second 80rps attempt failed on their original distinct images; do not erase negative evidence. The internal EdlsArchiveRotation protected-retirement optimisation removes a duplicated CRC read of immutable predecessor slots between a complete pre-removal CRC scan and the old-slot removal, under one exclusive writer. It preserves the full preflight, checked successor, protected-history validation before NVS ACK, durable NVS ACK, and full post-removal CRC scan. A test injects damage during removal and requires RetirementUnconfirmed and fail-closed recovery. The host GCC and Clang UBSan negative passed on 1 November.

The physical workload has passed TWICE, but source publication, current-source full regression, a new physical PowerLoss cut after the optimisation, G1–G15 documentation reconciliation and original-chip image restoration remain release requirements. This finite test is not indefinite rate, all hardware, arbitrary FileStorage PowerLoss atomicity, or device lifetime certification.

## 18 November 2026 — guarded 100 records/s workload repeats

Both WROOM fixtures selected two producers scheduled at a combined 100
records/s, 30,000 planned records, a 300 ms admission budget, 64-frame
EDLS segments and a 500 ms dwell. Both stopped when three scheduled
admissions exceeded their deadlines at full queue capacity. The producer
receipts show `CapacityUnavailable` before Memory acquisition.

| RPI400 run | Executable source and fixture | Published at stop | Retired and accepted | Firmware-reported durable | Minimum sampled free heap |
| --- | --- | ---: | ---: | ---: | ---: |
| `20261118T121219-9a597074` | `5e6aaa2`, 252 slots and one source pool | 25,922 | 25,670 | 25,653 | 134,060 B |
| `20261118T122356-f106fa4a` | `be1b2ba`, 496 slots and two source pools | 21,141 | 20,645 | 20,532 | 93,040 B |

The exact 496-slot build `20261118T122356-91e3ea9a` passed the native
compile profile check for O2, level-2 assertions, 160 MHz and 500 ms dwell.
Its factory SHA-256 was
`4ebb9c4fbc6d7eae9cfa01e22d1bd659005f7afea124dbbe9f3d587a72cb3908`.
Both runtime runs ended FAIL, exit 1. Both guards independently
verified and restored the original 4 MiB image and left the target OFF.
Each firmware halted before the independent workload raw-media audit.

The 252-slot timing probe observed a maximum native write of 493.639 ms
and maximum worker service time of 707.233 ms. The 496-slot fixture
recorded 317 flush deliveries taking 153.218 seconds in aggregate and a
707.561 ms maximum callback. The native per-operation storage counters
are compiled only into the 252-slot profile, and the fixtures use different
pool topology. The timings locate long synchronous storage service in
both runs and leave the cause of individual latency spikes unresolved.
The tested dual-pool fixture reduced sampled free heap by 41,020 bytes
and did not meet the workload target. The historical passing runs
remain evidence for their recorded revisions, fixtures and initial state.

Next compare queue depth and the timing of successor write, NVS commit
and retirement on repeated runs with the same provider instrumentation,
fixture seed, time budget and worker configuration. Use a supported
application storage and memory configuration that completes the full
offered workload with independent raw-media verification. See
[Logging #15](https://github.com/ESPressio-Development-Platform/EDP-Logging/issues/15).

## 18 November 2026 — matched timing and sealed readback

The two matched baseline images used executable source `99cd72d` with
fixture probes `b83ae37`. Both native compile profiles and independent
raw LittleFS/NVS audits passed. Both completed 30,000 scheduled records
at 100 records/s and all 30,129 phase records durably, with no deadline
failures. The underlying production path was unchanged between these
two fixtures.

| Receipt | Slots | Sampled backlog peak | Minimum sampled heap | Native reads | Successor writes | Write wall time | NVS store/commit wall time |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `20261118T125151-58276d14` | 252 | 79 | 133,596 B | 5,159 | 469 | 102.589 s | 4.479 s |
| `20261118T125152-6c5fd82e` | 496 | 76 | 92,448 B | 5,159 | 469 | 102.755 s | 4.567 s |

The 252-slot optimised firmware `d16a4e9`, built in
`20261118T125622-014d4a6f`, reuses the verified sealed successor capture
for the exact Memory comparison. Its trial `20261118T125847-8545448f`
observed **10 native reads per successor write** (2,970 reads for 297
writes), compared with 11 in both matched baselines. It stopped after
19,409 publications and four full-queue admission deadlines. The final
coordinator exit was `ReclaimUnconfirmed` (3), also present in the two
earlier failures. The queue samples reached 252. LittleFS successor
writes still dominated the timed storage stages, but the coordinator
exit prevents the queue from draining. The underlying retirement result
must be observed directly before assigning the stall to a specific
mutex or Memory failure. The run restored and verified original flash.

The optimised 64-record sealed fixture passed power-cut and independent
media run `20261118T125849-63124424` with 8.277 s OFF and confirmed POR.
The audit verified both protected generations (128 records across all
15 fields) and NVS ACK4/incarnation 7. The guard restored and independently
verified the full original 4 MiB image. The optimised path also passed
16 focused GCC/Clang checks under `20261118T125515-9468147f`.

The failing fixture selects a 5 ms bounded coordinator mutex budget and
a 100 Hz FreeRTOS tick. The producer deadlines remain 300 ms. Diagnose
the typed retirement failure and compare the existing budget with one
that spans the native timer granularity before qualifying the workload.
