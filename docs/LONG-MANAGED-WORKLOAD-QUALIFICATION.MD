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
