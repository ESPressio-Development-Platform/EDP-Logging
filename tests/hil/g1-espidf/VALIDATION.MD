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

## 2026-10-16 20:40 CEST — three actual hardware multi-sink record lifecycles

RPI400-integrated dual-core ESP32-D0WD on physical /dev/serial0.
The ordinary application HIL gained an extra three-sink test using
ONE Memory-owned Record, ONE FIFO ConsumerClaim and the same reclaim
path through DrainOneRecord: sink1 Accepted, sink2 Rejected, sink3
ProviderFailure, all called exactly once; aggregate status Partial,
FailedAndReclaimed; native Memory live count zero. It reuses
EDP-Threading/EDP-Memory; no Mesh or alternate allocator.

PioArduino pinned ESP-IDF 5.5.5 C++20 target build/link SUCCESS
(multisink-hil-build.log); RAM 14,288 bytes (4.4%) and application
Flash 226,261 bytes (5.4%) toolchain-estimated for THIS TEST APP
only; temporary combined binary 292,208 bytes.

New complete 4,194,304-byte original image freshly read from the
physical device in eight 512 KiB chunks, reassembled and independently
SHA256 verified BEFORE any test write, under private RPI400 directory
/srv/espressio/recovery/logging-multisink-20261016. Binary remains private.
Actual firmware flash verified through separate esptool digest, then
THREE independent physical reset/NVS committed-restart/Clock/Memory
production/multi-sink/Serial UART tests all PASS, each exactly:
    EDP_LOGGING_NATIVE_HIL_SUMMARY|markers=12/12|fail=0|binary_edlg_verified=True
Raw actual UART EDLG CRC checked independently by existing Python
decoder, not only app printed PASS. Private UART logs:
multisink-run1.log, multisink-run2.log, multisink-run3.log.

**Original image entirely restored and VERIFIED PASS.** Raw esptool
wrote ALL 0x400000 bytes, then independently verified digest match
without any intervening application boot:
    Verification successful (digest matched).
Only then normal GPIO reboot restored original firmware; status
DTR=HIGH RTS=HIGH and /dev/serial0 available. Logs under same
private recovery directory. No power cut or original-image bytes
published. Versions all remain 0.1.0. Source multi-sink tests do
NOT constitute async fanout, source-to-EDLS writer, or governed
canonical 15-field LogRecord.

## 2026-10-16 21:52–21:56 CEST — REAL SPIFFS native VFS EDLS successor, 3/3 HIL PASS, full restored image

**Actual device:** RPI400 integrated ESP32-D0WD (dual core),
physical /dev/serial0 at 115200, external 4 MiB SPI Flash.
Pinned PioArduino d41d8383706dc146fd91a0e1d3c59a915a4d493e,
ESP-IDF 5.5.5, Xtensa C++20. Test firmware FULL compile/link SUCCESS,
build log /srv/espressio/qualification/logging-production-20261016/
real-vfs-successor-wroom-build.log, 395.95 seconds.
Toolchain test-app static RAM 14,304 bytes (4.4% of 320 KiB), app Flash
257,541 bytes (6.1% of 4 MiB); factory merged binary 323,488 B.
These are **HIL test app** figures, not complete product resource budgets.

This test uses a **disposable custom partition table ONLY during HIL**,
decoded independently from actual generated partitions.bin before
flashing: nvs at 0x9000, phy_init at 0xF000, factory application at
0x10000 2 MiB, isolated edpfs SPIFFS at 0x210000 length 1984 KiB
(0x1F0000). No permanent board layout or flash changes were authorised
or left on the device. Source: tests/hil/g1-espidf/partitions.csv.
CMake lists spiffs as an application-only SDK component; EDP-Logging
source itself does not depend on any native or concrete provider.

**Flash safeguarding:** FRESH complete 4,194,304-byte physical original
image was successfully backed up as eight independently checked 512KiB
UART reads before any test writes. Original exact length and SHA-256
reconstruction passed (FLASH_BACKUP ALL_4MB_VERIFIED PASS).
Private original bytes and all UART logs reside ONLY on RPI400
/srv/espressio/recovery/logging-successor-vfs-20261016,
never in GitHub. Temporary test factory binary physically written to
real flash and independently verified in ROM download mode, result
Verification successful (digest matched), before first app boot.

**Real on-device provider proof:** Application Bootstrap selects
EDP-Persistence-ESP-IDF::VfsFileStorage bound to the mounted SPIFFS
filesystem and EDP-Platform-Portable::Memory::ByteOperationsProvider,
both existing library owners. It commits and readback-verifies a valid
EDLS header+canonical EDLG frame to slot0 (generation 1), constructs
a NEW concrete FileStorage provider and scans slot0, commits second
generation 2 in previously empty slot1, refuses third publication
(NoEmptySlot, older history preserved), then deliberately edits a
checksum-covered byte via native VFS to mimic corrupted storage,
and proves slot0 remains CRC-clean while slot1 has DamagedTail.
The test removes its own files and unmounts SPIFFS, then proves
the prior actual EDP-Persistence NVS committed esp_restart recovery,
EDP-Clock monotonic source, EDP-Memory/Threading producer,
three independent synchronous sink statuses, source reclamation,
native EDP-Serial-ESP-IDF binary UART EDLG output and independent
host frame CRC/codec/parser verification.

Three **SEPARATE actual GPIO reset campaigns**, each including a real
ESP32 app reboot, passed:
    EDP_LOGGING_NATIVE_HIL_SUMMARY|markers=13/13|fail=0|binary_edlg_verified=True
All three private captured files vfs-hil-run1/2/3.log contain actual
NATIVE_EDLS_VFS_SUCCESSOR_GUARD app marker and independent raw
serial-frame CRC verification. This is genuine Flash filesystem
IO through an actual app-composed EDP-Persistence owner, not a mock,
and not just UART debug printing.

**RESTORED ORIGINAL FIRMWARE IN FULL, digest MATCH:** raw esptool
rewrote the complete original 0x400000 (4,194,304) bytes, then
independently verified exact full-chip digest before ANY normal app
reboot, result:
    Verification successful (digest matched).
Only afterward did esp32-integrated reset allow original firmware
to resume. Actual integrated status DTR=HIGH RTS=HIGH,
UART=/dev/serial0 EXISTS=yes. Protected private logs:
restore-original-write.log and restore-original-verify.log.
Subsequent normal app NVS updates are expected after the complete
preboot original image was verified.

**Explicitly NOT certified:** REAL external power removal or an
interrupted filesystem write; atomic ReplaceFile with the selected
SPIFFS profile, because VfsFileStorage advertises
FileFailurePreservation::MayModify and FileInterruptionAtomicity::None;
automatic overwriting circular EDLS retention/erase/wear policy;
signed/historically authenticated LogRecord source and its EDP-System
permanent TypeIdentifier; full async multiworker fanout or product
G1–G15 gate closure. No power loss is simulated by esptool ROM reset.
Versions remain 0.1.0.

## 2026-10-17 source-record pre-wire fail-closed regression

The physical WROOM harness additionally checks the governed 15-field
`LogRecord` source validator: canonical valid monotonic-only record,
unknown Field 11/14 fact rejection and no falsely precise uncertainty
when a synchronized coordinate is absent. The capture parser expects
`GOVERNED_LOG_RECORD_VALIDATION` independently of other pre-existing HIL
markers. This test does **not** certify the canonical record's CBOR/JSON
typed envelope, authentic Clock coherence, asynchronous workers, or real
power interruption. Record source, full-flash backup/restore evidence,
and actual on-device result before claiming the physical gate.


## 2026-10-17 additional bounded production/coordinator physical checkpoint

The real ESP32 WROOM source harness now builds against the updated
`ProduceRecordUntil` occurrence/source ownership contract and also
uses the optional `DrainCoordinatorQuantum<1>` with two independent
static sinks over two actual `EDP-Memory` leased records. It asserts
one bounded record per pass, first all-accepted, second one sink
rejected without retry, immediate native reclamation after each
quantum, and a separate final canonical empty-queue check.
An independent marker
`EDP_LOGGING_HIL|PASS|BOUNDED_COORDINATOR_QUANTUM` increases the
capture contract from 14 to **15 distinct PASS markers**. This
is real physical synchronous fanout, NOT an actual
EDP-Threading Dedicated Thread or asynchronous forked encoder,
nor evidence of a power-loss-retaining production archive.
The original entire 4MiB flash must be backed up immediately
before this firmware is written and independently restored/verified
after the test.


### 2026-10-17 physical RED-first correction

The first real 15-marker firmware executed existing tests but reported
`FAIL|quantum-hardware-admission` because the new quantum block was
incorrectly inserted **after** the harness's irreversible
`ShutdownUntil` and `runtime.TearDown`. This was a test fixture
lifecycle ordering error, not evidence that native queue admission
itself could recover after shutdown. The regression has now been
moved before shutdown/teardown; an actual rebuilt/reflashed on-device
PASS is required before closing this checkpoint. The first physical
run correctly returned nonzero with 12/15 markers.

## 2026-10-17 EDP-Threading Dedicated Thread physical HIL (G10 extension)

The source has a new static `ThreadingTopology<DedicatedThread<...>>`
binding to `Logging::DedicatedCoordinator`, using the genuine ESP-IDF
ExecutionContext, SpinLock and FreeRTOS Signal/Mutex providers under
application Bootstrap. A real 2-record Memory-backed FIFO is
processed by this EDP-owned thread; two independent sinks test a
rejection, wake, finite JoinFor, cooperative stop and proper native
Threading finalization. New marker
`EDP_LOGGING_HIL|PASS|EDP_THREADING_DEDICATED_COORDINATOR` is the
16th requirement. This is the first **real managed Thread** validation
surface and is not equivalent to full G10 async representation fork
or production sink buffer lifetimes.

The original RPI400 integrated ESP32 image must again be freshly
backed up, SHA-256 checked, and independently re-verified completely
before normal boot after the test. Do not accept the new HIL gate based
only on compilation or on the prior 15/15 marker physical runs.


### 2026-10-17 native first-run JOIN contract finding

First genuine 16-marker WROOM run
`20261017T205059-b16f285a` FAIL (12/16, native
`FAIL|dedicated-thread-finite-join`) happened **AFTER** the native
worker activation, native source-processing and stop wake. The exact
owner's `DedicatedThreadRuntime::JoinWithBudget` first requires
`_router->CurrentContextIndex()`; external ESP-IDF
`app_main` is *not* an EDP-managed context, so its
`Thread::JoinFor` must return `Interrupted`.
Do not circumvent this with an unmanaged waiter or a second
scheduler. Corrected HIL requires the explicit `Interrupted`
negative result, then uses authoritative
`StaticThreadingRuntime::IsExecutionQuiescent()` under a finite
2-second physical-clock deadline (the concrete ESP-IDF test
Bootstrap, not a Logging library clock provider), followed by
terminal `FinalizeShutdown`. This is an ordinary managed Thread
with safe unmanaged Bootstrap observation, not a positive
Thread::JoinFor proof from the wrong context.


## 2026-10-17 native Memory-owned per-format copying-Sink qualification

The 17th HIL marker `EDP_LOGGING_HIL|PASS|MEMORY_OWNED_ENCODING_SERIAL_COPY`
uses a *distinct real EDP-Memory Runtime* with one dedicated
`OwnedEncodedPayload<32>` ObjectPool slot selected at Bootstrap.
A real EDP-Serialisation positional CBOR adapter encodes exactly
once per input into that EDP-Memory-owned slot; independent
EDP-Serial::BoundedEdlgFrameQueue copy-on-enqueue and Archive-like
synchronous copying sinks receive the immutable same payload.
Three source requests show native Serial FIFO capacity at two
frames while the archive-like sink still accepts the third
independently; all three source EDP-Memory leases are released
and its full runtime torn down. The two Serial queued copies
are then pumped to a real owner-conformant byte endpoint,
independently parsed as valid EDLG/CRC after the original
temporary payload leases have been reclaimed.
This is a *representation and storage-lifetime integration* test,
not a complete G4 LogRecord TypedEnvelope, a real Persistence
archive lifecycle or actual physical UART delivery for those
synthetic payloads. Explicit upstream Serial binary UART G1
test remains distinct.

## G11 WROOM fixture resource observation (2026-10-18)

The UART HIL now requires an 18th PASS marker:
WROOM_WHOLE_APP_RESOURCE_SNAPSHOT. Exactly one RESOURCE line
reports before/after/minimum 8-bit heap bytes, elapsed microseconds,
app_main task's remaining stack in bytes, and test Record/Queue
static sizes. The host parser rejects missing/malformed/nonpositive
or contradictory values. The reporter does not assert that these
are Logging-only CPU/RAM costs or establish a production budget.

Initial real 18/18 WROOM test using the ESP-IDF default 3584-byte
app_main stack had just 448 bytes reported remaining stack;
this prompted adding a HIL-only sdkconfig.defaults selecting 8192
bytes, pending independent subsequent native readings.
For details see docs/WROOM-RESOURCE-QUALIFICATION.MD.

### G11 revised stack qualification result — 2026-10-18

ESP-IDF sdkconfig.defaults now selects CONFIG_ESP_MAIN_TASK_STACK_SIZE
=8192 for this HIL only; generated local sdkconfig.esp32dev was
also updated for the source build. Full Xtensa/PioArduino target
build completed successfully under detached run
20261018T071810-5fbf6005 (generated config header verified 8192).
Flashed test image 342208 bytes and independently verified
digest 20261018T072415-16afd75a PASS. Three separate native
UART captures after actual GPIO reset:
20261018T072450-bead0b95,
20261018T072524-51bdc0f0,
20261018T072558-b09f3074, each 18/18, failures=0, valid
binary EDLG CRC, and verified resource tuple.

At 3584-byte fixture stack, app_main remaining margin was 448;
at 8192 it was 5056-5072. Extra configured 4608-byte stack
caused exactly 4608-byte reduction in free 8-bit heap. This is
a fixture-specific correctness margin, not a product-wide
application stack policy or a Logging-only memory profile.
See docs/WROOM-RESOURCE-QUALIFICATION.MD and Policy handoff.

## 2026-10-18 successor clean-source worker-stack regression (partial G11)

All verification below used RPI400 under explicitly authorised exclusive
agent lock `gpt6-logging-takeover-20261018-0909` and independently
published GitHub-feature-head-based, clean extracted EDP dependency
sources. **The pre-existing dirty RPI400 Logging working tree was never
reset, overwritten, or used to compile this qualification.**

- Clean pinned ESP-IDF/PioArduino build:
  `20261018T092422-384c757e`, PASS, exit code 0.
  Generated `sdkconfig.h` verified
  `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192`.
- Exact factory firmware SHA-256:
  `806fd3d4995ea5545c54a9340ebc691db9c25dccfdf674d8c251bd24b3feb9e9`,
  342,384 bytes. Independent physical ESP32-D0WD `write-flash` plus
  `verify-flash` and matched digest:
  `20261018T093122-9db1dbb2`, PASS.
- Independent board resets and native UART/CRC parsing:
  `20261018T093216-c4b23987` (cycle 1) and
  `20261018T093236-a348032a` (cycles 2, 3); all three
  18/18 functional markers, zero failed assertions, exact 24-byte
  EDLG Serial frame with independently verified CRC,
  one validated resource line, and `RESULT|PASS`.
- Actual Dedicated Thread native stack high-water margin:
  **5,036 bytes** in each cycle from **6,144 configured**,
  measured inside its worker callback, published after bounded
  Threading quiescence. This covers the tested task execution,
  not all downstream cleanup or any production work.
- Whole-app free 8-bit heap before/after/min respectively
  290,468 / 286,948 / 282,024 bytes on all three captures;
  `app_main` margin above 5 KB. No attribution of these
  figures to Logging alone.
- Independent clean Python suite
  `20261018T092757-a0b7f97e`, **30 tests PASS**;
  GCC + Clang C++20 host foundation
  `20261018T093004-0631a378`, both PASS.
  An initial exploratory wrong-script invocation
  `20261018T092925-d178b763` exited 127 because that
  nonexistent script name was supplied; reran the actual
  `run-host-tests.sh` successfully. Do not mask this red-first
  command-selection mistake.

No test here certifies full production async sink lifetime, power-loss
durability, true finite Memory blocking, full typed wire/schema or
enabled/Off resource delta. Those remain separately open under
Policy living handoff G1–G15.

## Test-only Off/Info site gate comparative code generation — 2026-10-18

Detached on RPI400: `20261018T094609-a2061881` PASS, exit 0.
New reusable fixtures `tests/G11CompileOutProof.cpp` and
`tests/run-g11-compile-out-proof.sh` compile with two exact
`EDP_LOGGING_G11_ENABLE_SITE` settings, **0 (Off)** and **1 (Info)**.
On GCC and Clang, Off executes no Clock/identity/argument capture,
whereas Info evaluates each exactly once; linker eliminates
the capture routine from Off binaries. `size` `.text`:
GCC Off/Info 1,900/2,116 bytes, Clang 1,895/2,055 bytes;
Xtensa C++ object `.text` 350/367 bytes.
This is isolated host/Xtensa **codegen/contract evidence**, NOT
a new physical-device run or complete firmware-sized logging delta.
The three clean G11 physical runs and measurements remain in the
previous section. Complete on-WROOM enabled/Off product-resource
qualification remains pending.


## G11 Off/On historical physical validation — 2026-10-18/19

The previous agent's independently recovered RPI400 detached jobs all PASS exit 0: dual build 20261018T174242-b1f983a7; Off flash 20261018T175617-b834f37f; Off physical captures 20261018T175653-45fc7b7c, 20261018T175733-4579e8bd, 20261018T175849-8146a776; On flash 20261018T175936-30bd4c3a; On captures 20261018T180015-bd57306d, 20261018T180053-b7f61941, 20261018T180123-99b7ab03; complete original 4MiB flash restore and independent preboot digest verification 20261018T180203-13405749; six-capture evidence reconciliation 20261018T180304-5a134b8b.

All six captures had 18/18 canonical markers and exactly **two** G11_WHOLE_FIRMWARE_SITE markers (the intentional NVS restart), each with selected=0 and all source counters=0 when Off, or selected=1 and all counters=1 when On. The extra G11 marker is not part of the 18 historical markers. Previously built source blob ccbdeb7921d57d54ee40f711986162d375655fde differs from the now safe-default-Off repository source. This is historical physical evidence **only for those original images**; newer source revisions must be revalidated before claiming equivalent on-device execution.

## Current-source G11 on-device qualification — 2026-10-19

The same new isolated-source Off and Enabled full firmware images were built and linked using PioArduino 6.2.0, ESP-IDF 5.5.5, Xtensa GNU 14.2.0 and identical generated SDK configuration, partitions and owner source. Manifest SHA-256: e05d1f2cfa3b72ce91f845740a9dea4022dc3a497ca7a58e8d03f4548b0f1035. New Off factory firmware SHA-256 5c422b40e5c2f969dc64895a05230c32b424e69d8f6c7d93170a594a58fff469; new Enabled SHA-256 2d58216be5ae79dbf8044c29965363cc240ed39ded25a82f97d9931e3585170b.
 
**Fresh actual ESP32-D0WD/WROOM physical validation**, detached RPI400 run 20261019T090011-c3c4eb91 **PASS exit0**, finished at 09:01:12 CEST: both images independently flashed and verified on-chip before normal boot, then three independent real UART physical captures per mode. Each of all 6 runs passed all 18 historical HIL markers, EDLG binary CRC, resource invariants and two separate (due NVS restart) strict source-site assertions. All three Off runs evaluated Clock/identity/argument zero times per boot; all three Enabled runs evaluated each exactly once per boot. Full-image size delta On−Off remains +128 bytes .text, 0 data, 0 bss. Actual test-only worker high-water stack margin remained 5,036 bytes; observed app_main margin Off 5,064, On 5,048 to 5,064 bytes, free8 before/after/min 290,460/286,940/282,016 bytes. Numbers cannot be generalised into a production resource budget. The run produced six separately retained UART logs and sha256 hashes under RPI400 /srv/espressio/scratch/gpt6-logging-20261019-g11/physical-qualify (NOT committed raw logs or firmware). Latest On test image was left programmed under user's explicit board-disposal permission; no prior-image restoration required.
 
The first detached two-build runner job 20261019T084501-d1695ba1 **reported FAIL exit2** solely because its own shell script was edited while the job was in progress; **both embedded ESP-IDF firmware builds themselves actually linked successfully**. The Python reconciliation from the stable script subsequently passed separately and produced the exact manifest digest above; this precursor wrapper failure is not misreported as PASS. Avoid edits to an actively executing script in future. No PowerLoss, production pipeline, multi-codec compile-out or G1–G15 final release is claimed.

## 2026-10-19 — G10 one-sink *real ESP32-WROOM* integration

The real ESP-IDF WROOM HIL now exercises two genuinely static G10
single-sink configurations **in addition to** its existing two-sink fanout:
an EDP-Memory-owned CBOR buffer accepted by EDP-Serial's bounded
copy-on-enqueue FIFO then independently transmitted and EDLG CRC-checked
after the source representation lease has been returned; and a second
EDP-Memory-owned CBOR buffer accepted by one archive-style synchronous
copying sink with no dummy Serial consumer. Each case checks exactly one
encode and one sink call, accepted count 1, empty owner lease, independent
payload ownership and complete pool teardown. The existing native
two-sink ordering, rejection, CRC and 18 canonical markers remain intact.

RPI400 real integrated ESP32-D0WD (WROOM) 2026-10-19:
- Actual PioArduino/ESP-IDF build **20261019T102408-3ed33bde PASS exit0**,
  source Git blob 41cab54885badfafc3f5d1aa611c58d473e7e630,
  entire factory image SHA256
  65e28a05e853f8266390be98948563928fb0aa8096948849efc21d50a40ed146.
- Physical write from 0x0 and **separate on-chip digest verification**
  **20261019T103019-e7ce3060 PASS exit0**.
- Three independent live reset + intentional NVS restart UART sessions
  **20261019T103056-344072ea PASS exit0**, finish 10:31:06 CEST.
  Every run 18/18 native functional markers, EDLG CRC, full-test
  resource checks and both G11 marker observations PASS; the canonical
  MEMORY_OWNED_ENCODING_SERIAL_COPY gate now also proves both single sinks.
- The six retained capture files/hashes live privately in
  /srv/espressio/scratch/gpt6-logging-20261019-g11/g10-one-sink-captures;
  capture-sha256 manifest digest
  5f10a96f545f4ac5ce5f410ef31df0b1a0b0f9a37bb64b8d21cc3cc364fc7562.
- Whole test bundle free8 min 282016 B, dedicated Thread stack remaining
  5036 B and app_main stack remaining 5048–5064 B. These do **not**
  constitute a complete Logging product resource budget.

**Limits:** The archival callback is an independently copied bounded
memory fixture, **not** a durable EDP-Persistence sink. Accepted Serial
FIFO bytes are not proof of end-user physical receipt. This one-sink
change does not complete asynchronous copy/lease/fork-join, governed
15-field CBOR/JSON or power-interruption durability; issue #1 remains open.


## 2026-10-19 — G4 approved child-field fresh physical WROOM proof

Host GCC/Clang canonical CBOR/JSON tests detached
`20261019T111050-c7675df7` **PASS exit0**. Verified all twelve valid
owner Clock enum pairs, invalid enum rejection, Field14 zero-only,
malformed arrays and canonical exact bytes.

Fresh WROOM ESP-IDF build `20261019T111337-b6f06252`
**PASS exit0**, complete factory image SHA256
`b7d4403cc6d8a732ff4eae1372f34ad458f38e9529d0a12d9012b325ceaf533c`.
Real ESP32 flash plus separately verified on-chip digest
`20261019T112033-d108dcb8` **PASS exit0**. Three independently
captured real host-reset/NVS-restart UART sessions detached
`20261019T112128-7c54b6e5` **PASS exit0**, retaining the original
18/18 functional markers, complete EDLG CRC and resource/G11 checks,
plus actual G4 CBOR/JSON pair round-trip and zero-only Field14 probe
inside GOVERNED_LOG_RECORD_VALIDATION. No external power cut, complete
15-field typed envelope, historical decode, or product budget is claimed.


## 2026-10-19 — fresh real ESP32 G1 native full-queue finite deadline

The integrated ESP32-D0WD native HIL now tests a *real* EDP-Threading
FreeRTOS wait against a genuine EDP-Clock monotonic deadline. Two
queue reservations occupy all slots, then a distinct producer invokes
AcquireRecordUntil (40 ms deadline, bounded wake retries) with actual
EDP-Memory pool and ordinary native mutex/provider. The test requires
DeadlineElapsed, MemoryAttempted=false, a fully empty scratch lease
and output Reservation, zero live Record allocations, and measured
elapsed time under **60 ms** (40 ms requested deadline + finite RTOS
scheduling margin). Both occupied queue slots must be successfully
reclaimed afterward, and the original 18 functional markers,
two-core wake tests, G4 child codecs, G10 sinks, UART EDLG/CRC,
G11 gating and resource validation must continue to pass.

Final stricter source firmware PioArduino/ESP-IDF build detached
20261019T125446-745ed1df **PASS exit0**, factory SHA256
e4bb59b63d1e0d3c03c7a17bf95bf68da43589184276dae330f340464e0b1f1b.
Real physical 0x0 write **and separately verified on-chip flash**
20261019T125557-e2d5a09b **PASS exit0**. Three new independent
host-triggered actual UART/NVS-restart cycles:
20261019T125636-ff55bc4e **PASS exit0**; measured deadline path
31,810 / 31,808 / 31,678 microseconds; all existing 18/18
functional gates, framing CRC, resources and G11 selection still
PASS. Privately retained capture manifest SHA256
aa860fef4bf708e7abafcb0a284274533a318a1452dbd4c584182cf0191be535.

Native real EDP-Memory+EDP-Threading host admission test suite separately
ran GCC and Clang, detached 20261019T125527-7f3a13e2
**PASS exit0**. A previous accepted physical image used a loose
250 ms assertion and a literal backslash-n test output; both
test-only evidence weaknesses were corrected, firmware rebuilt,
independently flashed, and three clean physical cycles repeated
before this final evidence was issued.

**Boundary:** This establishes one actual full-queue deadline
and no-Memory-borrow path only. It does not prove *all* G1
application producer policies, contended provider internal mutex,
all cancellation/shutdown races, external power-loss recovery, nor
a bounded Product-wide worst-case response distribution.



## 2026-10-19 — G2 Clock owner pair, real WROOM negative/recovery

G2 application-composed Clock source now preserves owner-issued
ProjectionStatus and TemporalQualityAssessment without Logging minting
new values. Host GCC and Clang generated-contract tests detached
`20261019T130909-8d20664a` PASS exit0.

Initial G2 real WROOM image physical run
`20261019T142256-5e601233` FAIL (4/18, repeated Core0
InstrFetchProhibited) because the added test called
`Clock::MonotonicNow()` before the full app's
`Clock::BindMonotonicClock()`. Decoded ELF addresses
`0x400de908` in governed validation and
`0x400d356f` in MonotonicNow; confirmed null early call.
Corrected helper uses fixed injected monotonic test evidence.

Final corrected ESP-IDF firmware build detached
`20261019T142627-5ed6a666` PASS exit0, factory image
SHA256 `ec074528b319a3ad6650545a9e14bbc5ef994c2eb5ade33b88dc678278e7d6f5`.
Real verified flash job
`20261019T142728-f73d9168` PASS exit0.
Three fresh independent native UART reboot/NVS-restart capture sessions
`20261019T142822-974150f9` PASS exit0,
all 18/18 existing markers, G2 owner sample, G4 real codecs, G1
finite deadline and prior resource/serial/fanout tests PASS.
Private capture manifest digest:
`3c8187e82aa82c5c5246cf0edaf0493091fdec7ff9d50f18d7ecd8512289d8d0`.

No physical mesh Clock synchronization or cross-revision actual
provider is asserted from synthetically injected sample values.


## 2026-10-19 — G1 real cross-core native queue-mutex contention

Added separate actual FreeRTOS two-core contention case: Core 1
takes the **same EDP-Threading OrdinaryMutexProvider** that
TransactionalRecordQueue uses; Core 0 calls AcquireRecordUntil
against one 40 ms EDP-Clock deadline with queue otherwise empty.
The test requires no Memory attempted or leased, no active
reservation, zero live records, mutex release by true Core 1 owner
and wall elapsed below 60 ms, without weakening prior full-queue
finite admission test.

Initial release build `20261019T144136-2eda367c`
PASS and verified flash `20261019T144239-a8670819` PASS,
but physical run `20261019T144335-e1d94c57` FAIL exit18
at 6/18 markers. Diagnostic rebuild
`20261019T144553-51d830d9` and verified flash
`20261019T144646-a6a69528` PASS; physical capture
`20261019T144726-47ef055d` FAIL exit18, revealing status
Admitted=0 at 5,722 us, MemoryAttempted=1, owner active,
one live record. Cause: test worker used
`vTaskDelay(pdMS_TO_TICKS(2U))`, which yielded **zero ticks**
on this FreeRTOS platform, so its 500-iteration holding period
ended in ~5 ms and it released mutex before the admission
deadline. This was an early-release test defect, not an
admission implementation violation.

Corrected holder and parent polling to
`vTaskDelay(1U)` (always one real native RTOS tick), retaining
finite retry count and cross-core release protocol.
Final corrected ESP-IDF/PioArduino firmware build
`20261019T144940-f5117240` **PASS exit0**;
factory SHA256
`9570e2b5214b4989f41d46e95942379b960a5b3101cc20beaa3aef968e7eaa71`.
Actual 0x0 ESP32 flash write + independent readback
`20261019T145043-4be56437` **PASS exit0**.
Three separate real UART/NVS-restart cycles
`20261019T145122-b0bb8241` **PASS exit0**;
actual mutex-contention wall elapsed each
**40,035 microseconds**, admission Status
DeadlineElapsed=1, queue result Busy=3, MemoryAttempted=0,
scratch empty, output inactive, Record::Live=0, worker
mutex released=1. All old 18/18 HIL checkpoints, EDLG CRC,
resources, G2 owner statuses, G4 child codecs and G11 Off
selection PASS. Private captured evidence SHA manifest digest
`ac2b902a5dc4e79583fc4d30aa94ac7947b1036f5bcbac8f0691324044b7c062`
under `/srv/espressio/scratch/gpt6-logging-20261019-g11/g1-native-mutex-realtick-captures`.
No new package version or persistent product release claim.

**Boundary:** One real queue mutex contention and one real full
queue deadline are now physically exercised; extensive mixed
Memory pool/Queue contention, provider failures, cancellation
and shutdown, power interruption and worst-case product
load are not yet certified.


## 2026-10-19 — G1 native real EDP-Memory exhaustion with queue rollback

Third independent real ESP32 finite-admission scenario: two actual
EDP-Memory dedicated ObjectPool leases hold every physical Record
instance, while the two-slot transactional queue remains EMPTY.
Third producer invokes AcquireRecordUntil on the SAME 40 ms
EDP-Clock absolute deadline, so it first reserves its Queue slot,
then waits for actual Memory capacity. Requires DeadlineElapsed,
MemoryAttempted=true, Memory result DeadlineElapsed, no scratch
lease, no remaining output reservation, two retained old objects
while waiting, then clean releases of both held leases via
AdmissionLifecycleCoordinator::ReleaseAndNotify, successful wake
publication and Record::Live=0. Elapsed must remain <60ms.

PioArduino ESP-IDF WROOM build
`20261019T145955-ae1d1ed2` **PASS exit0**,
factory SHA256
`ece445320cf50fc97bfaba339a7b24351544266f13b68ad3f89946c256dee2ee`.
Actual hardware write and independent on-chip verify-flash
`20261019T150100-3bfccd90` **PASS exit0**.
Three real independent UART/NVS-restart cycles
`20261019T150155-b01ce6e4` **PASS exit0**:
**36,863 µs each**, Status DeadlineElapsed=1,
Memory outcome DeadlineElapsed=2, both CoordinatedReleaseResult
ReleasedAndNotified=0, no live records, no output/scratch ownership.
All 18/18 prior HIL markers including G1 full queue and
two-core mutex, G2/G4, EDLG/CRC, resources, G11 OFF PASS.
Private evidence capture SHA manifest
`a14d7c49745219022b288fe73ddae06106ee3ca1af258c017e2e5dcb5541940b`
at `/srv/espressio/scratch/gpt6-logging-20261019-g11/g1-native-memory-exhaustion-captures`.

Scope limited to physical actual pool exhaustion and clean release;
no unpowered reset, adversarial late-grant provider, native Memory
mutex contention or every shutdown/cancellation interleaving claimed.


## 2026-10-19 — G1 real concurrent shutdown during Memory capacity wait

Additional true dual-core WROOM lifecycle test uses an independent
local EDP-Threading Wake provider + AdmissionLifecycleCoordinator
+ TransactionalRecordQueue so requesting shutdown does NOT
contaminate other HIL cases. Two actual EDP-Memory dedicated pool
leases hold all Record objects. A genuine Core1 producer enters
AcquireRecordUntil with a 3.5s bounded Clock deadline and reserves
Queue capacity, then blocks on unavailable Memory. Core0 observes
ActiveAdmissions>0 and invokes real ShutdownUntil with a separate
1.2s bounded Clock deadline. The test requires shutdown
Quiescent, zero active producers, joined notified worker with
ShutdownRequested and no output/scratch lease, actual held leases
released through the coordinator with wake publication, and
zero total live Records. All tests retain 18 canonical markers.

PioArduino ESP-IDF WROOM image compiled
`20261019T150854-1bd69c19` **PASS exit0**,
factory full SHA256
`6dfa67de9586eab6ae0dc49d4324d6fe65c069aae219d80dfa141512850eaa02`.
Real write + separately verified on-chip flash
`20261019T150959-42cb1357` **PASS exit0**.
Three independent physical UART/NVS-restart captures
`20261019T151045-90d672b0` **PASS exit0**;
initial waiting flag true, shutdown=Quiescent=0,
active admissions zero, worker.Success=true,
worker.Completed=true, task notification received,
all old Record leases released, live count zero.
Actual shutdown coordination measured 292 / 292 / 293
microseconds. All existing HIL 18/18 markers, G1 queue-full,
native queue-mutex, exhausted Memory timeout, G2/G4/G10/G11,
EDLG CRC and whole-bundle resource checks remained PASS.
Private evidence manifest SHA256
`e39384b12ce2e72c5e02801955a576f8e463b8cfa097c3f70dd02816f2b0a336`
at `/srv/espressio/scratch/gpt6-logging-20261019-g11/g1-shutdown-memory-wait-captures`.

Boundary: one pre-confirmed native memory-wait worker and
coordinated shutdown; not an exhaustive race/fuzz exploration
of late grants, failed native wake notification or
multiple simultaneous producer-slot interactions.


## 2026-10-19 — G1 native cross-core Memory grant before/after deadline

Added `g1_memory_grant_boundary.hpp` (included within Test in
`main.cpp`), creating a fresh independent bounded Queue,
EDP-Threading Wakes, AdmissionLifecycleCoordinator and native mutex
for each trial, sharing the initialized real EDP-Memory pool.
Two actual pool leases occupy all dedicated instances while the
third producer is spawned on ESP32 Core1. Core0 observes an active
admission before releasing capacity, or waits until the Core1
producer has timed out. Cases:

- **Before**: one leased object released by coordinator before the
  500ms deadline; producer MUST be Admitted with its expected value,
  then Abort the reservation without leak, and release remaining
  held objects.
- **After**: producer waits against 40ms Clock deadline while
  pool remains full, returns DeadlineElapsed without a lease or
  active Queue reservation; ONLY AFTER worker completes does Core0
  return capacity. Late available capacity must not resurrect a
  completed transaction.

Both require actual MemoryAttempted, task completion notification,
zero final live Record objects, zero ActiveAdmissions, both held
leases released/notified and empty; all 18 existing native markers
remain required. This tests two real release orderings, **NOT**
adversarial `GrantedAfterDeadline` inside the race window.

Initial build `20261019T162611-7acaf0d4` PASS but test log
included a literal newline escape; corrected source. First
incremental build `20261019T163216-59e579af` FAIL exit1 because
build invocation forgot pinned EDP_LOGGING_DEPENDENCY_ROOT; not
a source failure. Corrected full PioArduino ESP-IDF build
`20261019T163505-49528812` **PASS exit0**, factory image SHA256
`0639d7dd742041004e0636fde7b6c4f0cbe0b86dc96223536317544831db527c`.
Real physical flash and separate on-chip verification
`20261019T164114-6cda40fd` **PASS exit0**. Three fresh
independent native UART/NVS-restart cycles, now correctly labelled,
`20261019T164337-cf0128ff` **PASS exit0**, all existing
18/18 markers and CRC/resources/G11 each cycle.
Each cycle before-case result Admitted (status 0), observed
418us after Core0 release, no leaks; after-case
DeadlineElapsed (status 1), observed 30922us from the parent
measurement start, no leases/reservations leaked.
Private evidence capture manifest SHA256
`c104ec53258a67a83bb9dcf97aa175c1c4620b679df76592d8f3d9805f22b58a`;
captures remain in `/srv/espressio/scratch/gpt6-logging-20261019-g11/g1-grant-boundary-qualified-captures`.

No unexpected physical power interruption occurred, and the exact
sub-tick late-grant-after-deadline interleaving remains open.

## 2026-10-20 — real Memory grant returned after deadline (controlled race)

A new HIL-only `g1_injected_late_grant.hpp` wraps the **real native
EDP-Memory ObjectPool** with a deliberate FreeRTOS delay *after* a
successful NoWait grant and before returning the result to Logging.
The injected 80ms+tick delay exceeds the caller's 40ms absolute
EDP-Clock deadline, deterministically exercising the rare
`GrantedAfterDeadline` rollback, without altering production code.

The test requires exactly one actual native Memory acquisition,
`RecordAdmissionStatus::LateMemoryGrantReturned`,
`AdmissionWaitResult::GrantedAfterDeadline`,
`CoordinatedReleaseResult::ReleasedAndNotified`, successful capacity
wake publication, empty scratch and queue reservation, zero live
records and zero active admissions. This checks **source ownership
recovery**; it does not measure the natural frequency of the race.

Full PioArduino ESP-IDF WROOM build detached
`20261020T065504-1b27bcf4` PASS exit0; complete factory image SHA256
`39ffb6a9915567c3acf0fe1ffadd60bc349a7f0916b98e9aaf24179ed9739a06`.
Actual 0x0 flash write and independently matched on-chip digest
were performed by detached physical job
`20261020T070038-d3f11092` PASS exit0, finished 07:01:22 CEST.
Three independent real reset/NVS-restart UART cycles passed with
18/18 existing functional markers, EDLG CRC, resources and G11 probes.
Each measured 89,032 microseconds, status=7, memory=1,
release=0, wake=1, live=0, valid=1.
Private captures under `/srv/espressio/scratch/gpt6-logging-20261019-g11/g1-delayed-grant-captures-20261020`;
capture manifest SHA256
`feee38d9301df3cebddc89e18252430d13d85b0885cc59a39444b372735f26bd`.
No simulated Memory lease or fake production path was substituted.

**Scope:** Delayed Memory-return injection with a genuinely owned lease
proves an important G1 failure rollback, not random scheduling odds,
real unexpected power interruption, arbitrary native provider failures
or complete worst-case application admission timing.

## 2026-10-20 — native final Queue::AttachLease deadline violation

HIL-only `g1_delayed_attach.hpp` wraps a real EDP-Threading
OrdinaryMutexProvider; its first timed acquisition reserves a
queue slot normally and its second succeeds normally but delays
return by 80ms+FreeRTOS tick while holding the native mutex.
The **actual** EDP-Memory lease was granted on time, and after
attachment production `AcquireRecordUntil` MUST return
`DeadlineElapsed`, not `Admitted`, fully aborting the reservation
and reclaiming its owned Record.

Independent GCC+Clang owner-composed host regression detached
`20261020T071258-65de05a8` PASS exit0.
Authoritative native ESP-IDF WROOM complete factory firmware build
`20261020T071340-b6adac07` PASS exit0, SHA256
`b48d5ec3fcf27e4e4cdfd32a2c321fb25d7e8a47679ce4bd13022da17086e446`.
Actual write 0x0 and separately matched on-chip flash digest,
followed by three fresh independent UART/NVS-restart cycles in
`20261020T071458-cf925ae7` PASS exit0 (07:20 CEST).
All 18/18 existing functional checks, EDLG CRC, resources, G11
selector and previous G1–G10 gates remained PASS.
Each run reported native `G1_DELAYED_ATTACH|locks=2|status=1|
memory=0|queue=0|live=0|elapsed_us=89623|valid=1`.
Private captures `/srv/espressio/scratch/gpt6-logging-20261019-g11/g1-post-attach-captures-20261020`;
manifest SHA256 `788fa225af05529d6bd21b615fda7a389bbf4105494c8bd710153c623628898a`.
Fault injection is synthetic; actual physical ownership,
native lock and cleanup are genuine.


## 2026-10-20 08:38–08:42 CEST — native post-attachment cancellation

After the G1 post-AttachLease canonical deadline fix, a second test
exercises **cancellation becoming visible during** the final native
Queue::AttachLease. The HIL-only provider delegates to a *real*
EDP-Threading mutex and raises a test stop predicate on its second
successful timed Acquire, then introduces an 80ms + FreeRTOS tick
post-acquire delay. The actual EDP-Memory lease transfers to the
Queue and MUST be released by the normal Reservation abort path.
The expected result is `ShutdownRequested` (2), rather than
`Admitted`, even while the queue attachment returns Succeeded.
This is an injected stop predicate, not a claim that real lifecycle
ShutdownUntil or all inter-core stop races were exhausted.

Host real owner-composed GCC and Clang admission tests detached
`20261020T083848-50c62356` **PASS exit0**. Actual complete
PioArduino/ESP-IDF WROOM firmware build detached
`20261020T083937-a40a64b6` **PASS exit0**, factory SHA256
`360ef7ad8b732b32510e2f089318950a7a6df4e412b5b265a18da0cb0c7742c3`.
Real write at 0x0 followed by separately verified on-chip flash
digest `20261020T084050-323a7b0b` **PASS exit0**.
Three independent host-triggered native reset/NVS-restart UART cycles
`20261020T084135-8228e918` **PASS exit0**, each 18/18
original markers, full EDLG CRC/resource/G11 instrumentation.
Every cycle emitted `G1_CANCEL_DURING_ATTACH|locks=2|status=2|
memory=0|queue=0|live=0|elapsed_us=89563–89564|valid=1`;
no owner lease or admission was retained. Private capture files
at `/srv/espressio/scratch/gpt6-logging-20261019-g11/g1-final-attach-cancel-captures-20261020`,
checksum-manifest SHA256
`1bd6c5bde1d9de5a12e9d7aa72fe8296aa786d120982292d3e626fd183396f24`.
No arbitrary full-product timing, natural race probability,
unexpected external power cut or complete Logging release implied.

