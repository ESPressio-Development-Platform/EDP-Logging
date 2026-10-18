# WROOM-32 whole-application resource observation (partial G11)

2026-10-18: implementation and physical measurement on the existing
EDP-Logging ESP-IDF / FreeRTOS integrated qualification application.
All package versions remain 0.1.0 and Logging remains independent of Mesh.

## Measured boundary

One ESP-IDF app_main wraps real native NVS checkpoint restart/recovery,
SPIFFS, Clock, Memory, producer, dual-core wake, EDP-Threading
Dedicated Thread, CBOR encoded copying-sink fanout, and Serial owner.

The integrated test samples:
- free 8-bit heap at app_main entry and after the final test using
  heap_caps_get_free_size(MALLOC_CAP_8BIT)
- minimum free 8-bit heap since boot, using
  heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)
- FreeRTOS app_main stack high-water *remaining* in ESP-IDF bytes using
  uxTaskGetStackHighWaterMark(nullptr)
- test-bundle elapsed microseconds using esp_timer_get_time
- compile-time sizeof(test Record), sizeof(test Queue)

The host UART capture script requires exactly one resource line with
app_scope=whole_test_bundle, positive fields, and consistency of minimum
free heap against both before and after readings. The new mandatory
18th checkpoint is WROOM_WHOLE_APP_RESOURCE_SNAPSHOT.
No fixed pass/fail thresholds for a different product are fabricated.

These numbers are for an entire qualification app. They must NOT be
attributed solely to Logging and do NOT prove production-level CPU,
RAM, stack, flash, or battery budgets.

## Stack configuration finding

At the ESP-IDF default CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584,
the first verified physical 18/18 pass measured only 448 bytes of
remaining app_main task stack after the integrated tests. This small
margin is a valid resource concern regardless of passing assertions.
The HIL's own sdkconfig.defaults now specifies 8192 bytes for the
app_main test task, and this fixture's regenerated sdkconfig uses
the same value. This is NOT a global platform default and does not
change Logging's portable library.

Production applications must select and profile their own task stack
budgets and include all independent EDP-Threading worker stacks.
The app_main reported margin excludes the dedicated worker's task
margin. Exact measured results and test firmware SHA should be
retained in the Policy living handoff rather than generalizing them.

## Qualification and exclusions

The initial WROOM image and a reconfigured higher-stack image are
tested on the actual board with EDLG CRC validation, all 18 native
functional markers and resource-line parsing on separate resets.
The user expressly authorized flashing disposable development boards
without a fresh backup on 2026-10-18. Current devices therefore
need not be restored to prior development images after an authorized
test, but all flashing still requires an exclusive appliance lease,
a known target board and an independent firmware digest verify.

G11 remains incomplete until measurements cover isolated enabled vs
compile-out-only builds on the same pinning, per-task worker stack,
per-stage CPU time, flash partitions and a concrete production app's
explicit declared budgets. Normal reboot is not unexpected power loss.

## Verified two-configuration WROOM measurements — 2026-10-18

Both sets of measurements used real ESP32-D0WD/WROOM hardware via
RPI400 integrated UART and required 18/18 markers, zero failed
checks, independent EDLG frame CRC and a validated resource line.

- SDK default main-task stack 3584 bytes: measured app_main remaining
  stack margin 448 bytes; free 8-bit heap before 295092 and after
  291572 bytes; minimum free heap 286648 bytes; elapsed 1466718 us;
  Record static size 16 bytes, Queue static size 56 bytes.
  Initial detached native run 20261018T071630-41b9480e PASS.
- Fixture main-task stack 8192 bytes: three physical runs
  20261018T072450-bead0b95,
  20261018T072524-51bdc0f0,
  20261018T072558-b09f3074 all PASS 18/18, binary EDLG CRC
  and resource snapshot validated. Available app_main stack
  margin respectively 5056, 5072, 5056 bytes. Free 8-bit
  heap before 290484, after 286964, and minimum 282040 bytes
  on each. Test elapsed 1466748 us; static Record 16 and Queue 56.

The difference in before/after/minimum free heap between builds was
exactly 4608 bytes, the deliberate 8192-3584 test-task stack
increment. No additional user product memory consumption was
attributed. The app_main remaining stack margin increased by
4608-4624 bytes. This improvement is confirmed by repeated
hardware measurements, but it is NOT a measurement of every
EDP-Threading Dedicated Thread's stack. The board deliberately
remains on the new disposable test image; the user authorised
no preflash backup or restoration for development boards.

## Independent dedicated-worker stack evidence — 2026-10-18 (G11 partial)

A test-only `MeasuredCallable` wraps the real `EDP-Threading` Dedicated Thread
coordinator's callable in `tests/hil/g1-espidf/src/main.cpp`. It records
FreeRTOS `uxTaskGetStackHighWaterMark(nullptr)` **inside the actual native
worker**, at the callback's final boundary, and publishes the high-water
minimum-free-byte count through one atomic value read by the bootstrap
thread after verified quiescence. This is *test-only ESP-IDF instrumentation*,
not a new platform-independent Logging SDK dependency or a new public API.
`EDP-Platform` already owns the portable `ExecutionStackTelemetry` concept;
production instrumentation should prefer that existing owner if it becomes
available through the correct Threading context API.

The HIL parser requires the additional
`dedicated_worker_stack_margin_bytes` field, nonzero and not greater
than the **6,144-byte** Dedicated Thread configured stack capacity.
Each of **three independent fresh physical WROOM runs** on RPI400
reported **5,036 bytes remaining** for this worker, alongside positive
main-task and whole-app heap measurements. The high-water sample is taken
before the Threading framework completes the final context exit; the
reported measurement does **not** include any later Threading teardown,
other worker stacks, or future product workloads. It must not be
interpreted as an application-wide stack guarantee.

Clean-source immutable GitHub-pinned firmware build:
`20261018T092422-384c757e` (PASS), ESP-IDF/PioArduino,
`CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192`;
factory image **SHA-256
`806fd3d4995ea5545c54a9340ebc691db9c25dccfdf674d8c251bd24b3feb9e9`**.
Real device `ESP32-D0WD`, RPI400 `/dev/serial0` verified-write
run `20261018T093122-9db1dbb2` (PASS). Fresh physical cycles
`20261018T093216-c4b23987` and
`20261018T093236-a348032a`: **three reboots, 18/18 each,
independent EDLG CRC and whole-resource tuple valid**, no failed markers.
Worker margin: 5,036 bytes in all three; app-main margins:
5,056 / 5,056-or-5,072 / 5,072 bytes (exact per-cycle values retained in
the detached run logs, not used to claim an invariant). Free 8-bit heap
before/after/min: 290,468 / 286,948 / 282,024 bytes.
These are measurements of the *entire test application*, not isolated
incremental Logging RAM or CPU costs.

The clean source and all **17 pinned EDP dependency refs** are recorded
in `/srv/espressio/scratch/gpt6-logging-takeover-20261018-g11/source-pins.json`
on the RPI400 evidence appliance. Source blobs and exact publication
commit are recorded in the living Policy Logging handoff.

**Still open:** identical toolchain enabled-versus-compile-time-Off
whole-binary and `.bss`/heap/stack/flash comparison, per-stage CPU
and encoding-format compile-outs, small/large profile and all ticket
widths, and a real product-specific budget. Do not close G11 because
the present resource assertion and 18 HIL markers passed.

## Site-only disabled/enabled codegen comparison — 2026-10-18

The independent `tests/G11CompileOutProof.cpp` is compiled from the
*same source*, with exactly one build-level change:
`EDP_LOGGING_G11_ENABLE_SITE=0` (fully disabled) versus `=1` (Info
site). It simulates Clock, identity and argument captures under the
actual public `EDP_LOG_WHEN_ENABLED` lazy macro. The compiler definition
is **test-only**; it is not a production library configuration switch.
The companion `tests/run-g11-compile-out-proof.sh` builds and runs
both cases with GCC and Clang, examines linked symbols and reports code
sizes, and can compile the same two sources into Xtensa/ESP32 object
files when the target compiler is installed.

RPI400 detached run `20261018T094609-a2061881`: **PASS**;
the disabled site executed **zero** Clock, identity and argument
evaluations for both host compilers; the enabled site executed each
**once**. The disabled linked host binaries contained **no reachable
Clock capture function symbol**; the enabled binaries did.
Using identical `-Os -ffunction-sections -fdata-sections` and
`--gc-sections` options, GCC linked `.text` respectively
**1,900 versus 2,116 bytes** (216-byte enabled-site delta) and
Clang respectively **1,895 versus 2,055 bytes** (160-byte delta).
The Xtensa compiler built both object forms successfully: ELF
object `.text` **350 versus 367 bytes** and `.bss` **12 bytes in
both**. These precise deltas are for the **isolated proof fixture**,
not the Logging runtime, entire ESP-IDF firmware, or a real deployment.
An object-file `.text` delta is not necessarily a final linked
firmware footprint delta, because linking eliminates unreferenced
sections.

**Still missing for full G11:** a physically measured WROOM
**complete application** enabled-versus-compiled-out comparison
using the identical dependency/toolchain/stack/partition/feature profile,
including runtime heap, real coordinator/writer, flash segment size,
task stack margins and per-stage timing. These early host/object results
prove only the selected site-gate contract and are not a substitute.
