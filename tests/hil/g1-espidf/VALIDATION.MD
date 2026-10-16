# Physical validation evidence — 2026-10-16

**Hardware:** RPI400-integrated ESP32-D0WD, WROOM-32 class, two cores,
4 MiB external flash, UART /dev/serial0, observed ROM MAC ending 98:60.
**Source:** EDP-Logging/feature/logging-v1 commit 413e07d7b3ddb03c4dc8dd8f1a8ac0c958e8a578
plus this new HIL test harness. Package versions remain 0.1.0.
**Toolchain:** PioArduino platform d41d8383706dc146fd91a0e1d3c59a915a4d493e,
ESP-IDF 5.5.5, Xtensa GCC 14.2.0, C++20.
**Dependency topology:** source-pinned EDP-Logging, EDP-Memory,
EDP-Threading, EDP-Clock, EDP-System, EDP-BoundedTopology,
EDP-BoundedTypes, EDP-Platform, EDP-Platform-FreeRTOS and
EDP-Platform-Portable, resolved from EDP_LOGGING_DEPENDENCY_ROOT.
Application Bootstrap selects concrete FreeRTOS providers; foundation
headers consume only abstract EDP owner contracts.

## Material runs recorded on RPI400

- 20261016T190850-13969f69: PASS (exit 0), full ESP32-WROOM
  ESP-IDF application firmware compile/link, including the blocked-producer
  shutdown case. Images include factory bootloader/partition/application.
- 20261016T190448-849b3171: PASS (exit 0), complete original 4,194,304-byte
  flash backup in eight independently read 512 KiB ranges, SHA-256 verified.
  The first monolithic attempt (20261016T185808-4281b44b) failed after
  87.2% because the UART stream corrupted; the successful chunked backup
  superseded it. Original bytes and private data were NEVER committed.
- 20261016T191215-07880d34: PASS, read-only ESP32 eFuse summary: flash
  encryption disabled, secure boot v1/v2 disabled.
- 20261016T191239-cd335cc0: PASS, the 247,200-byte temporary test
  bootloader/partition/application factory image was actually flashed to
  physical ESP32 with on-write digest verification.
- 20261016T191309-7b39f6ef: PASS, first real serial HIL test, all seven
  expected markers present; no FAIL markers.
- 20261016T191342-ec7b821b: PASS, independent physical reset and second
  HIL run, all seven markers present; no FAIL markers.
- 20261016T191409-e454c545: PASS, independent physical reset and third
  HIL run, all seven markers present; no FAIL markers.
- 20261016T191437-5b9e6d05: PASS, separate on-chip digest verification
  of all 247,200 bytes of temporary firmware.
- 20261016T191454-f59766b5: PASS, original entire 4,194,304-byte flash
  restored, esptool on-write verification passed.
- 20261016T191700-a29f0c5b: FAIL, immediate separate full-chip digest
  comparison, because the RPI400 integrated helper unconditionally performs
  a normal reset after every esptool operation, letting restored firmware
  modify the NVS sector before the next check.
- 20261016T191724-f1768f5a: PASS, eight 512 KiB digest-probe commands:
  only range 0 differed; ranges 1-7 matched original images exactly.
- 20261016T191805-c0afd36e: PASS, bytewise readback of first 512 KiB:
  precisely 192 differences, all limited to NVS sector at address 0xB000,
  where original firmware had written runtime state after a helper reset.
- 20261016T191924-405313b7: PASS, original 4 KiB NVS sector at 0xB000
  rewritten and independently verified; a following helper reset once
  more allowed NVS runtime mutation.
- **20261016T192020-381dcdea: PASS, final conclusive restoration.**
  The affected 4 KiB original sector was rewritten using raw esptool
  after entering ROM download mode, followed **without an intervening
  normal reset** by an independent full-chip digest comparison of all
  4,194,304 bytes: **Verification successful (digest matched).**
  Only **after successful full-chip verification** was the GPIO reset
  control released to boot the original firmware. This confirms bytewise
  identity of the entire original on-flash image before its first boot.

## What was exercised on real hardware

- Canonical EDP-Clock timebase bound to an ESP-IDF 1 MHz provider.
- Real EDP-Memory pool acquisition, Logging FIFO Commit/Pop/Complete,
  exclusive RAII ownership and physical record destruction.
- Queue-full producer admission does not consume a scarce physical
  Memory pool object; releasing a queue reservation wakes the real
  FreeRTOS producer task pinned to CPU1.
- The special shutdown-drainer slot cannot be borrowed by a producer.
- Real task blocks on full queue during shutdown; shutdown waits for
  that active admission to retire; new admissions are rejected; all
  retained source objects are reclaimed before Memory teardown.

The UART boot log identifies the actual ESP32-D0WD, app built from
EDP-Logging 413e07d, ESP-IDF 5.5.5, main app on CPU0. Every HIL run
emitted a terminal RESULT PASS; no source-level test harness substitutes
for this physical result.

## Limits

This qualifies an opt-in partial **G1 queue/Memory/Threading lifecycle**.
It does NOT establish full governed production LogRecord capture, G2
Mesh/Clock synchronized timestamp/era, G4 canonical record identity,
worker/sinks/archives, filesystem power-loss durability, all remaining
G1 fault-injection cases, or complete G2-G15 programme. The source
library itself was not modified by this test harness. The durable
Policy/main Logging living handoff remains the authoritative overall
completion and next-step register.
