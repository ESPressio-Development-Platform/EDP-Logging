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
