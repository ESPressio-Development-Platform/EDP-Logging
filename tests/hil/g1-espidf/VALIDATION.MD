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

## 2026-10-16 — extended standalone Clock / producer-to-EDLG physical proof

Real dual-core ESP32-D0WD, /dev/serial0 115200, 4 MiB flash. Real Bootstrap
providers: FreeRTOS EDP-Threading, EDP-Memory ObjectPool, standalone monotonic
EDP-Clock (NO Mesh), producer sequencing with the dedicated Threading mutex,
bounded queue, genuine EDP-Serialisation positional CBOR encoder, and canonical
Logging EDLG CRC frame. App sink emitted exact EDLG bytes as UART hex.

ESP-IDF 5.5.5/pioarduino pinned d41d8383706dc146fd91a0e1d3c59a915a4d493e,
C++20, compiled/linked HIL SUCCESS. Static RAM 13,920 bytes (4.2%);
Flash application 188,557 bytes (4.5%) of reported target limits, test app
only. Temporary merged factory firmware 254,496 bytes.

Before writing, complete fresh original flash read in eight 512 KiB chunks,
reassembled full 4,194,304-byte original, SHA-256 independently checked PASS.
Full original backup stays ONLY on RPI400 in the secured recovery directory.
Temporary firmware actual esptool 254,496 byte FLASH PASS on-write hash verified.

Independent PHYSICAL runs 1, 2 and 3:
- each real GPIO reset and actual UART read;
- each exactly 9/9 mandatory app markers PASS, fail=0;
- independent *existing* Python EDLG framing/CRC scan of actual wire bytes
  verifies a single canonical CBOR frame, codec and test payload
  [attempt=1, monotonic timestamp, int 515], edlg_crc_cbor=True;
- real sink rejection for second record accounted as failed submission,
  and Memory reclaimed successfully;
- legacy dual-core producer wake, queue lease ownership, reserved-drain-slot
  isolation, bounded shutdown and native Memory teardown all PASS.

**No permanent governed LogRecord was emitted.** Payload is TEST ONLY; G4
TypeIdentifier allocation has not occurred, and no EDLS durable archive,
Persistence power interruption, cryptographic proof or full multi-sink
LogRecord worker pipeline was exercised. Acceptance is not durability.
Physical proof is real but limited to this explicit scope.

The original flash restoration, verification, normal reboot, source revision,
native compiler tests and release are recorded in the authoritative Policy
living handoff. The physical raw capture traces and private flash bytes stay
on RPI400, not GitHub.

### Extended HIL original firmware restoration — PASS, exact full flash digest

After all three physical test runs, 2026-10-16 ~19:48–19:50 CEST,
entered integrated ESP32 ROM-download mode and wrote the freshly backed
up complete 4,194,304-byte original flash. A second independent raw
esptool invocation, using both --before no-reset and --after no-reset,
verified the ENTIRE 0x400000-byte original image against on-chip flash,
**without a normal application reset between writing and verification**.
Actual hardware tool returned:

    Verifying 0x400000 (4194304) bytes at 0x00000000 in flash ...
    Verification successful (digest matched).

Only after passing the full-chip digest was the original application
rebooted with normal GPIO reset (OK RESET). The restoration process
completed with exit code zero; its private logs are
logging-production-hil-20261016/restore-write.log and restore-verify.log
on the RPI400. Later normal firmware activity may modify its NVS,
as expected; equality was verified before that boot. The original image
must not be published because it may contain private application data.

## 2026-10-16 20:16–20:18 CEST — actual native EDP-Persistence/NVS + EDP-Serial UART FIFO proof

**Source:** EDP-Logging feature/logging-v1 following 0d787ef, including
logging/EdlgCheckpoint.hpp optional EDP-Persistence KeyValueStorage adapter.
PioArduino pinned d41d8383706dc146fd91a0e1d3c59a915a4d493e,
ESP-IDF 5.5.5, Xtensa C++20.
Physical target was RPI400 integrated dual-core ESP32-D0WD, 4 MiB flash,
physical /dev/serial0 @115200. Actual firmware build/link SUCCESS:
native-serial-nvs-build2.log, 392.60 s, static RAM 14,288 bytes (4.4%)
and application flash 224,897 bytes (5.4%) per build toolchain.
Combined temporary factory binary: 290,832 bytes. Prior first build
failed only because new include flags had literal backslash-n joining,
fixed before the successful build. No EDP-Mesh implementation linked.

Before writing, read whole original **4,194,304 bytes** in eight
independent 512 KiB actual flash reads. 512 KiB slices 5 and 6
encountered serial digest mismatches and were retried; ALL eight
eventually passed and reconstructed original SHA256 independently
verified: FLASH_BACKUP|ALL_4MB_VERIFIED|PASS. Private original image and
raw evidence remain exclusively under RPI400 secure directory
/srv/espressio/recovery/logging-persistence-serial-20261016.
NEVER upload the original firmware or NVS contents to GitHub.
Raw esptool physically flashed 290,832 bytes and independently
verified temporary firmware digest BEFORE release of ROM download
mode; capture observer attached BEFORE GPIO reset.

Actual real-on-device test mechanism: the app chooses concrete
EDP-Persistence-ESP-IDF NvsKeyValueStorage, using EDP-Platform-Portable's
already-existing EDP-Memory ByteOperations provider. The app commits a
24-byte TEST-ONLY EDLG CBOR frame through generic Logging
WriteEdlgCheckpoint into namespace edp_log_hil/key frame. After success,
it calls esp_restart. Only on the next real physical boot can
ReadEdlgCheckpoint recover, validate exact size/CRC/codecs/bytes and
then delete/commit the test key. Host captured BOTH pre-restart
and post-restart markers. This establishes native restart retention,
NOT the physical cut-and-restore of external power.

Then real EDP-Memory/Threading/Clock producer and FIFO tests run and
Bootstrap selects existing EDP-Serial::BoundedEdlgFrameQueue and
EDP-Serial-ESP-IDF::UartFifoByteStreamWriter on the real UART0
hardware with zero application TX buffer and exclusive writer use.
A complete 24-byte raw binary EDLG frame is sent to the physical
RPI400 serial RX, with no printf framing substitution. The
independent existing Python EDLG scanner validates the CRC32C,
the Cbor codec and exactly the CBOR [1,2,3] tuple; the UART FIFO
reports completion and the native UART transmit-drained boundary
is explicitly checked. Host-observed raw frame bytes:
45444c47010100000400000068bf707083010203474c4445 .
All stages are HIL test data, not the canonical governed LogRecord.

**Three independent full GPIO-reset NVS-commit/restart/recovery/Serial cycles
PASSED**, each real UART host capture ended:

    EDP_LOGGING_NATIVE_HIL_SUMMARY|markers=11/11|fail=0|binary_edlg_verified=True

Evidence logs native-hil-run1.log, native-hil-run2.log, native-hil-run3.log
under the private RPI400 recovery directory. This genuinely covers
EDP-Persistence-ESP-IDF and EDP-Serial-ESP-IDF, not mocked endpoints.
No external power-cut, EDLS historical multi-slot archive, wear
retention, source-sink multiwriter arbitration beyond a single
exclusive FIFO, signed archival identity or canonical G4 LogRecord
is claimed.

**Complete original flash restoration independently VERIFIED PASS.**
After 3/3 physical runs, raw esptool remained in the SAME ROM download
session while it wrote the entire original 4,194,304 bytes and then
independently verified all 0x400000 bytes before any normal firmware
reboot. Tool reported:
    Verification successful (digest matched).
Only AFTER this whole-chip digest match did esp32-integrated reset
return the original application to normal boot. Full logs:
restore-original-write.log, restore-original-verify.log.
Native host GCC/Clang and UBSan results remain separately recorded
in the authoritative Policy living handoff. All packages remain 0.1.0.
