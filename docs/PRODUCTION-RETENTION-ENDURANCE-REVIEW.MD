# Production retention and endurance review

Implemented library configuration and finite qualification are distinct from selecting an application's production operating profile. All versions remain0.1.0. The current evidence does not select that product profile.

## Measured test configuration

| Item | Established value |
|---|---|
| Physical flash probe | JEDEC20/4016,4MiB detected,3.3V; exact package/revision not established |
| Test image map |2MiB declared map,576KiB LittleFS,24KiB NVS |
| Retention |2 protected immutable segments plus one spare |
| Batch |At most64 records and16384 framed bytes; explicit500ms dwell |
| Qualified long workload |30000 scheduled sources at100/s over300s; all30129 phase records durable, run103339-6856faf0 PASS |
| Final protected media |124 original records in2 segments,ACK472; raw/NVSCRC PASS |
| Actual final segment bytes |17472B for124 records, including EDLS/EDLC framing |
| Sampled serialised bytes per retained record |140.903226B; this is not a measurement of physical flash programming |

A five-minute test does not retain five minutes of history. The archive intentionally rolls expired segments. At dense100/s, the tested two-segment window contains roughly one second of records. Dwell, source sizes and partial batches change the count and time represented by a segment.

Current strict archive supports2..63 protected segments with one spare, and the batch supports at most64 records per segment. Therefore4032 is an upper record-count capacity for this topology, not a guaranteed retained record count. Partial/large-record batches retain fewer records. Larger supported configurations need their own space and timing qualification;100/s was measured with2 protected segments. Bootstrap must budget the spare, worst-case segment sizes and filesystem reserve.

## Explicit engineering model, not a lifetime certificate

The manufacturer-hosted XM25QH32C v2.1 datasheet lists JEDEC20/4016 and specifies endurance above100000 erase/program cycles. The measured ID is consistent with that family; it does not identify the exact silicon/package revision. Use100000 here only as a stated engineering budget scenario, not certification of the attached part.

Primary source: [XMC XM25QH32C v2.1](https://www.xmcwh.com/uploads/799/XM25QH32C_Ver2.1.pdf), feature section and section7.1.1. Downloaded official PDF SHA256 bbcf181d5eb3b8a39430f814d47e6e20b2620729eae2e97c01551bf0b060d3fb, read-only appliance review103624-e77200e3 PASS. No vendor PDF is redistributed.

Idealised budgeted days = filesystem capacity * budgeted cycles / (sampled segment bytes per record * active records per second * active duty fraction * assumed write amplification *86400).

This model assumes uniform wear over all576KiB, dense100/s active periods and no other writers. It excludes NVS wear, metadata reserve, bad blocks, temperature/voltage margins, actual erase distribution and device ageing. Amplification is an assumption, not a measurement. Quiet periods and a uniformly sparse stream can have different batching/write costs. These values are budget sensitivities, not predicted replacement dates or guaranteed service life.

| Time active at100/s | A=1 days | A=2 days | A=4 days | A=8 days |
|---|---:|---:|---:|---:|
| 100% | 48.4 | 24.2 | 12.1 | 6.1 |
| 10% | 484.5 | 242.2 | 121.1 | 60.6 |
| 1% | 4844.9 | 2422.5 | 1211.2 | 605.6 |

The installed4MiB is not all assigned to this test filesystem. A larger application partition map can change storage budgets, while firmware/OTA/assets and NVS also consume flash. Expanding a map does not itself establish lifetime. The present calculation is insufficient to approve continuous100/s for an arbitrary multi-year product.

## Product decisions needed for acceptance

- Required retained history: records, bytes or a time window, including expected partial batches.
- Sustained event rate and peak burst duration/duty cycle; distinguish production traffic from the100/s stress target.
- Required minimum service life and acceptable durability/admission-to-ACK latency.

These inputs determine the capacity, partition, batching/dwell and workload/endurance acceptance matrix. They are application requirements rather than a permission to run more tests. Until selected, the named finite configurations are qualified; universal production readiness and a flash lifetime guarantee remain unclaimed. Exact interrupted flash-op/GC timing also remains outside the UART-triggered experiments.

## Completed reliability evidence

Managed write-activity recovery095844-2022f045 PASS; near-full refusal52pending/noACK3 and POR-confirmed recovery100726-066996bc PASS; committed-watermark/pre-unlink retirement102037-886599f8 PASS; five-minute managed workload103339-6856faf0 PASS. Each has independent retained-original/NVS media checks. No hosted GitHub Actions coverage is relied upon.
