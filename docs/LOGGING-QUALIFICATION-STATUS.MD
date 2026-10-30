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
| Three-target container demo | Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF builds183532-8bfb4a6b; actual two-boot tests184352-e1b78bb7 all PASS | Small CBOR container values; a full generated typed managed-worker demo remains separate work |
| Public provider boundaries | Existing Memory, Threading, Clock, Serialisation, Localisation and Persistence owners reused; no FileStorage PowerLoss trait upgrade | No claim of generic filesystem interruption atomicity |
| Physical reliability campaign | Existing complete-candidate, candidate-write activity and near-full experiments retained in native fixture README | Current batched managed pipeline still needs write/retirement/GC/near-full interruption qualification |
| Endurance and product acceptance | Latency, bounded memory and stack observations exist for named finite fixtures | Sustained duration, flash wear/lifetime, final ACK latency and product resource acceptance remain incomplete |

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
