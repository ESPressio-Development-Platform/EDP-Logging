# Configurable bounded EDLS batch

This finite ESP32 application demonstrates the existing BoundedEdlsBatch
container and PowerLossAcknowledgedEdls archive with actual LittleFS and NVS.
It uses small unsigned values encoded by EDP-Serialisation CBOR. These payloads
demonstrate the container; they are not generated fifteen-field LogRecords.
The separate native managed-worker fixture exercises that complete typed path.

All library versions remain 0.1.0. Place EDP-Logging beside the dependency
repositories listed in DEPENDENCY_GRAPH.MD. The three required projects share
Arduino_IDE/src/BatchDemo.hpp; no copied batch, codec, storage provider or worker.

| Target | Project |
|---|---|
| Arduino IDE / Arduino-ESP32 | Arduino_IDE/Arduino_IDE.ino |
| PlatformIO / Arduino-ESP32 | PlatformIO_Arduino/platformio.ini |
| PlatformIO / ESP-IDF | PlatformIO_ESP-IDF/platformio.ini |

## Configure and build

The shared header defaults EDP_DEMO_DWELL_NS to 100000000 (100 ms).
Change that default or supply the compiler definition, for example
-DEDP_DEMO_DWELL_NS=500000000LL. The example permits 0 through 1000000000 ns.
The production API separately fixes maximum records, framed bytes and dwell
ceiling at compile time, then selects the dwell using ConfigureDwell.
Changing it requires an empty batch; zero means immediate service eligibility.

Run bash demos/ConfigurableBoundedEdlsBatch/build.sh all from EDP-Logging,
or select espidf, platformio-arduino or arduino-ide. EDP_PIO_COMMAND,
EDP_ARDUINO_CLI_COMMAND and EDP_DEMO_JOBS select installed build executables
and finite parallelism. Arduino IDE/CLI needs Arduino-ESP32 3.3.12 and
C++20 plus the same sibling-owner include paths supplied by build.sh.
The PlatformIO projects pin the qualified pioarduino platform revision.
The custom partition table is supplied for all three targets.

## Provision and run

Use a disposable target and explicitly provision the declared 2 MiB layout.
Before the first run, provision a valid empty LittleFS image in the edplfs
partition at 0x170000, size 0x90000. Normal startup never formats a failed
mount, erases NVS, silently seeds a watermark for existing history, or deletes
unrelated files. Firmware flashing does not itself create a valid filesystem.

The Bootstrap derives a demo device identifier from the chip's permanent MAC
with a fixed demo namespace prefix. A separate committed CBOR counter in NVS
namespace batchdemo advances once per boot and is independently read back
before that incarnation is exposed. It fails on corruption or exhaustion.
This is application example wiring, not a general identity service/provider.

The demo first selects a one-second dwell and submits four complete frames.
Their RAM admissions must leave the segment watermark unchanged. Service then
acknowledges the full four-record segment. Next it explicitly selects the
configured EDP_DEMO_DWELL_NS, offers one partial record and receives no further
input. The ordinary Bootstrap yields until the canonical Clock deadline, then
services that partial batch. A dedicated coordinator application forwards the
same deadline to its ThreadContext::WaitUntil instead.

Expected successful output starts EDP_BATCH_DEMO|PASS and includes incarnation,
configured dwell, four full-batch records, one idle record and the segment ACK.
A later boot advances incarnation and recovers the same strict archive before
appending new segments. Two segments are retained, with one vacant physical
successor name; retained record count depends on frames in those segments.

Any uncertain archive result ends this finite example without a retry or an
implicit destructor flush. A configured accumulation dwell does not bound
storage, NVS commit, scheduling or total acknowledgement latency. This example
does not claim FileStorage power-loss atomicity or production readiness.

## Appliance validation

All three target builds PASS in RPI400 detached 20261030T183532-8bfb4a6b.
All three actual firmware/image verifications and two-boot executions PASS in
20261030T184352-e1b78bb7, completed 18:45:29 CET on 30 October.
Each target independently advances incarnation1->2 and segment ACK2->4 with
the selected100ms partial dwell, four full-batch records and one idle record.
Controller/media regressions:95 tests PASS in20261030T182456-7134fda1.
No GitHub Actions execution supplies coverage.

Verified image SHA256:
- ESP-IDF:02ba7704d390b7cf1ccb0d543c98afc674ccb44b86cda863dc4fa94a8d951675.
- PlatformIO Arduino:9a68816512f9de433641e02f18266b9acff9755c292f96ae2ff0f94ffbc8d796.
- Arduino IDE:8b3637fde82dc037508be85fa7c2ca987ef4f46b5c2f640046d8d3c06988cc90.

Earlier PlatformIO Arduino builds182126-65df7d9d and183314-8fc4079d FAIL
because LittleFS library compilation lacked the selected framework's FS
header path. configure_framework.py now supplies that existing SDK path;
the framework and libraries are explicitly pinned. No vendor source changed.

Physical sequence183658-a9cd81ef remains FAIL: ESP-IDF and PlatformIO Arduino
passed both boots, but full Arduino IDE image verification failed before its
test boot. Diagnostic184231-5ed19b7f PASS found exactly217 differing bytes,
all in NVS0x9000..0x917d, and verified every immutable firmware component.
The appliance esptool subcommand resets after each operation, allowing startup
to change NVS before the next independent image comparison. The corrected
runner uses the existing controller's with-power command, remains in ROM/stub
through full image AND filesystem verification, then explicitly starts the
application. It preserves the full verification criteria. The successful
fresh repeat184352-e1b78bb7 qualifies all three targets with that ordering.
Backup defaults full; disposable requires an explicit authorised opt-in.
Private UART/images and failed receipts remain on RPI400; target exits OFF.
These are normal-boot demonstrations, not electrical-cut or endurance proof.


## Immutable application byte budget

The archive accepts a trailing byteCapacity constructor argument before initialisation. BoundedEdlsBatch respects the resulting smaller per-segment share during admission and deadline service. The bound includes EDLS files, the temporary successor and the 40-byte ACK value; provider-internal physical allocation overhead needs a separate application reserve. See [consumer and library-expansion contract](../../docs/IMMUTABLE-PERSISTENCE-BYTE-CAPACITY.MD).

The shared three-target demonstration explicitly selects 640 logical bytes. This is an example application choice, not a platform default.
