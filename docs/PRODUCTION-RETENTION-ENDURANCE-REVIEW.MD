# Application storage capacity and flash wear

All versions remain 0.1.0. The application determines where logs are persisted, including the storage provider and medium. ESP-integrated flash is a tested configuration, not a platform requirement.

## Application-owned configuration

The application supplies the Persistence sink's byte capacity during Bootstrap or before initialisation. The capacity must be immutable after initialisation. The platform does not prescribe a retained byte count or history duration.

Qualification must demonstrate that the sink does not exceed its configured byte limit, including temporary candidate storage needed for safe successor commit. The existing segment-count bounds and near-full refusal experiments do not by themselves establish this byte-limit contract. Explicit accounting, enforcement and boundary tests remain to be verified.

The throughput target remains 100 records per second. It is not merely a stress-test rate. Existing evidence covers finite workloads, including 30,000 scheduled records over five minutes; this is not an indefinite-throughput guarantee.

## Storage selection and wear guidance

Allocating more physical storage to persisted logging can spread erase cycles across more flash blocks, reducing wear per block where the storage provider distributes wear across that space. It does not necessarily reduce total writes or total erase operations. Increasing only the logical retention limit does not necessarily enlarge the physical wear-levelling pool.

Applications requiring substantial persisted logging in production release builds should ideally use separate storage rather than ESP-integrated flash. The application selects that storage and its provider according to its requirements. Separate storage is not automatically more durable; its characteristics and provider behaviour still matter. Any independently persisted acknowledgement or identity metadata must also be considered when identifying writes that remain on internal flash.

EDP provides no operational-lifespan or flash-endurance guarantee or assurance. The previously discussed two-year target is withdrawn as a platform acceptance criterion. No application lifetime estimate is required to continue implementation or qualification.

## Existing finite evidence

The tested internal-flash profile used 576 KiB LittleFS and 24 KiB NVS in a 2 MiB image map on hardware reporting 4 MiB flash. It retained two protected immutable segments plus a spare, with batches of at most 64 records / 16,384 framed bytes and an explicit 500 ms dwell.

The final five-minute run retained 124 original records in 17,472 serialised segment bytes. This measures stored representation size, not physical write amplification or flash endurance. Five minutes of processing does not imply five minutes of retained history.

Managed write-activity recovery 095844-2022f045 PASS; near-full refusal with 52 pending records / no ACK3 and POR-confirmed recovery 100726-066996bc PASS; committed-watermark/pre-unlink retirement 102037-886599f8 PASS; five-minute managed workload 103339-6856faf0 PASS. Each has independent retained-original/NVS media checks. No hosted GitHub Actions coverage is relied upon.

Historical endurance calculations remain available in Git history as superseded engineering exploration. They are not current acceptance criteria or lifespan predictions. Exact interrupted flash-op/GC timing remains outside the UART-triggered experiments.
