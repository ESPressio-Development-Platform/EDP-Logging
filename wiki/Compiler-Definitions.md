# Compiler and conditional compilation

This implementation requires C++20. EDP_LOG_WHEN_ENABLED provides a compile-time threshold gate and should eliminate disabled argument evaluation entirely. Application and test build variants must select the same canonical EDP owner dependencies and SDK configuration when comparing code size or timing.

## Boundaries

G11 site-level and whole-application compiler proofs are separate. Logging opt-in headers introduce dependencies only when imported. No platform-wide enabled logging severity or unbounded global queue is implied.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.

## Core demo selection

`FramingAndInstrumentation` adds no repository-defined compiler definitions. The C++20 `EDP_LOG_WHEN_ENABLED` macro uses template `SeverityThreshold`/emitted `Severity` constants chosen by the application, **not** preprocessor switches; it must leave disabled-site expression evaluation absent. The two PlatformIO projects explicitly request `-std=gnu++20`. `Arduino_IDE` requires a board/core toolchain configured for C++20. There is no hidden default threshold enable, universal diagnostic log site registration, or new provider-selection flag.

## G11 test-only compiler definitions (source-owned versus fixture-owned)

`EDP_LOGGING_G11_ENABLE_SITE` is defined **only** by the host `tests/run-g11-compile-out-proof.sh` `-D` for `tests/G11CompileOutProof.cpp`; the fixture intentionally requires an explicit value **0** (disabled site) or **1** (enabled). There is no production default; the paired linked-symbol and side-effect tests compare the exact same source with only this fixture value changed. It is not a Bootstrap severity setting or a globally enabled Logging runtime.

`EDP_LOGGING_G11_APP_SITE_ENABLED` is an independent **test-only** definition for the ESP-IDF HIL `tests/hil/g1-espidf/src/main.cpp`: **0** disables the instrumented proof site (its effective fixture default), **1** enables the Info-level proof; values outside the supported set are rejected at compilation by that fixture. It measures a single site in a constrained target, not a universal application binary footprint or a new production compiler knob.

There are no new repository-defined public `#define` configuration switches in `Severity.hpp` or `Instrumentation.hpp`: the supported `EDP_LOG_WHEN_ENABLED` macro takes **compile-time template arguments**, not an external preprocessor severity knob.


## Maintained host fixture and demo controls

The host test runners supply the following fixture switches. Reuse flags
are defined inside the including test translation unit and have no
numeric value. Their meaning depends on presence. Setting a presence
flag to 0 still defines it.

| Definition | Scope, values and effect |
| --- | --- |
| `EDP_LOGGING_G1_LIFECYCLE_TEST_INCLUDE` | Presence flag in host fixture includes. Omits AdmissionWakeCoordinatorTests' standalone entry point so its shared Memory and Clock fixtures can be reused. |
| `EDP_LOGGING_RECORD_PRODUCTION_TEST_REUSE` | Presence flag in CoordinatorSinkServiceTests. Omits RecordProductionTests' standalone entry point while reusing its queue and context fixtures. |
| `EDP_LOGGING_ASYNC_SKIP_ADMISSION_INCLUDE` | Presence flag in generated async integration. Suppresses the async test fixture's repeated admission-fixture include. |
| `EDP_LOGGING_STRICT_TASK_REUSE` | Presence flag in generated strict/batch integration. Omits the included async fixture's standalone entry point. |
| `EDP_TEST_CANONICAL_ADMISSION_ATOMIC` | Presence flag from the admission lifecycle test runner. Selects the canonical Platform atomic binding. Absence selects the separate standard atomic host case. |
| `EDP_TEST_SINK_COUNT` | Required integer from the record producer negative-compile runner. Values 0 and 9 must fail the supported 1–8 sink contract. The negative fixture has no implicit default. |
| `EDP_TEST_WEAK_RETENTION` | Presence flag in the snapshot negative-compile runner. Advertises Restart retention to prove that the PowerLoss snapshot profile refuses an insufficient provider. Absence selects the positive provider fixture. |
| `EDP_DEMO_DWELL_NS` | Signed nanoseconds in the shared ConfigurableBoundedEdlsBatch demo implementation. Default 100000000LL. The demo accepts 0 through 1000000000LL. Zero requests immediate sealing. It configures the example's existing bounded batch owner. |

The G11 definitions and their 0/1 validation are documented above.
The demo implementation is [BatchDemo.hpp](../demos/ConfigurableBoundedEdlsBatch/Arduino_IDE/src/BatchDemo.hpp).
The host test runners reside under [tests](../tests).

## G7 native archive and worker profiles

These controls belong to the physical qualification fixture.
[platformio.ini](../tests/hil/g7-littlefs-protected-rolling-espidf/platformio.ini)
supplies the profile flags. The
[main fixture](../tests/hil/g7-littlefs-protected-rolling-espidf/src/main.cpp),
[sealed probe](../tests/hil/g7-littlefs-protected-rolling-espidf/src/sealed_batch_probe.hpp)
and [worker probe](../tests/hil/g7-littlefs-protected-rolling-espidf/src/batch_worker_probe.hpp)
contain their exact conditional branches, defaults and constraints.
Use the named manifest environments with their inherited flags and
partition layout when reproducing a recorded qualification.

| Definition | Scope, values and effect |
| --- | --- |
| `EDP_TEST_EDLS_LOWSPACE` | Presence selector supplied by the G7 lowspace and capacity-cut environments. Enables the fixture's constrained filesystem-capacity scenario. |
| `EDP_TEST_EDLS_MIDWRITE` | Presence selector supplied by the G7 midwrite environment. Runs the write-interruption fixture and its host-controlled cut marker. |
| `EDP_TEST_TYPED_ARCHIVE` | Presence selector supplied by the G7 typed environment. Enables governed record generation and typed archive/recovery validation. |
| `EDP_TEST_STRICT_ACK` | Presence selector supplied by the G7 strictack environment. Runs the native strict watermark acknowledgement/recovery fixture. |
| `EDP_TEST_TYPED_STRICT_ACK` | Presence selector supplied by the G7 typed-strict environment and inherited batch profiles. Combines governed original records with strict acknowledged recovery. |
| `EDP_TEST_TYPED_STRICT_LOAD` | Presence selector supplied by the G7 typed-strict-load environment. Selects the original-record load scenario and its native result markers. |
| `EDP_TEST_TYPED_STRICT_PROFILE` | Presence selector supplied by the G7 typed-strict-profile environment. Selects timed single-record strict acknowledgement measurements. |
| `EDP_TEST_SEALED_BATCH` | Presence selector supplied by sealed-batch environments and inherited worker profiles. Activates the sealed multi-frame Bootstrap and native probe. |
| `EDP_TEST_BATCH_RECORDS` | Unsigned frame count for the sealed probe. Default 16U. The fixture permits 16U or 64U and derives finite segment and scratch sizes from it. The 64-record environments supply 64U. |
| `EDP_TEST_SEALED_BATCH_CUT` | Presence selector supplied by the sealed cut environments. Enables the bounded cut/recovery campaign and host cut markers. |
| `EDP_TEST_SEALED_WRITE_ACTIVITY_CUT` | Presence selector supplied by writecut environments. Emits write-activity markers for external interruption, with independently checked recovery. |
| `EDP_TEST_SEALED_RETIREMENT_CUT` | Presence selector supplied by the retirecut environment. Exercises an interruption around eligible predecessor retirement. |
| `EDP_TEST_BATCH_WORKER` | Presence selector supplied by batch-worker environments. Uses the topology-owned Threading coordinator and source producers for sealed batches. |
| `EDP_TEST_BYTE_CAPACITY` | Unsigned total logical archive byte capacity supplied to the G7 archive. The O2 worker manifest supplies 28000ULL. If absent, the fixture derives its bound from three segments and metadata. The selected archive validates that the value can hold its required layout. |
| `EDP_TEST_STORAGE_IO_PROFILE` | Presence flag automatically enabled in the G7 source for the selected wide/dual batch worker. Adds bounded canonical-clock measurements for the existing LittleFS and NVS calls. Absence uses the underlying providers without those probes. |
| `EDP_WORKER_DWELL_NS` | Signed worker batch dwell in nanoseconds. Default 100000000LL. The fixture accepts 100000000LL or 500000000LL. The 500 environments supply the latter. |
| `EDP_WORKER_RECORDS_PER_PRODUCER` | Unsigned scheduled count for each of two managed producers. Default 1000U. The fixture accepts 1000U, 3000U or 15000U, giving total scheduled workloads of 2000, 6000 or 30000 records. |
| `EDP_WORKER_SCHEDULE_RPS` | Unsigned aggregate offered records per second. Default 100U. The fixture accepts 80U or 100U and derives an absolute schedule for each producer. |
| `EDP_WORKER_OPTIMISED_BUILD` | Presence flag supplied as 1 by O2 environments. Labels the expected O2 profile. The build tool separately verifies actual compiler arguments and retained assertions. |
| `EDP_WORKER_WIDE_QUEUE_PROFILE` | Presence flag supplied as 1 by wide/80 environments. Selects 252 queue slots and the corresponding single Memory pool. Absence of both queue flags selects 128 slots. The 30000/100-rps wide fixture selects a 20 ms coordinator queue budget. |
| `EDP_WORKER_DUAL_POOL_PROFILE` | Presence flag supplied as 1 by the dual environment. Selects 496 queue slots and two separately owned identical Memory pools. This branch takes precedence when both queue selectors are defined. |
| `EDP_WORKER_POWER_CUT` | Presence flag supplied by worker writecut environments. Enables the worker interruption and recovered-media receipt path. |
| `EDP_WORKER_CAPACITY_CUT` | Presence flag supplied by the capacitycut environment. Extends the worker interruption profile with the constrained-capacity cut scenario. |

## External toolchain and SDK controls

`NDEBUG` is a compiler convention. The native worker fixture rejects a
build that defines it because its qualification requires assertion
checks. Optimisation flags such as `-O2` come from the compiler and the
manifest. `CONFIG_COMPILER_OPTIMIZATION_ASSERTION_LEVEL`,
`CONFIG_FREERTOS_HZ` and `CONFIG_ESP_MAIN_TASK_STACK_SIZE` belong to
ESP-IDF/FreeRTOS configuration. They govern assertions, native tick
resolution and Bootstrap stack storage for the named fixture.
`__cplusplus` identifies the selected language standard. These
definitions retain their SDK/toolchain ownership.

The validation runner separately checks the selected native profile.
A definition's presence supplies configuration and does not establish
that compilation or physical execution passed.
