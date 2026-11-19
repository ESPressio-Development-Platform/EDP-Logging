# Typed managed batch worker

This finite ESP32 application queues nine complete generated LogRecords, then
runs one existing EDP-Logging DedicatedCoordinator through actual EDP-Memory,
EDP-Threading, canonical Clock, EDP-Serialisation, LittleFS and NVS.

The three projects share Arduino_IDE/src/WorkerDemo.hpp:

| Target | Project |
|---|---|
| Arduino IDE / Arduino-ESP32 | Arduino_IDE/Arduino_IDE.ino |
| PlatformIO / Arduino-ESP32 | PlatformIO_Arduino/platformio.ini |
| PlatformIO / ESP-IDF | PlatformIO_ESP-IDF/platformio.ini |

All EDP library versions remain 0.1.0. Place the declared dependency owners beside
EDP-Logging; see DEPENDENCY_GRAPH.MD in the repository root. This application also
reuses the container example's identity Bootstrap header. No copied codec,
storage implementation, Memory allocator, queue or worker is introduced.

## Build and provision

Run bash demos/TypedManagedBatchWorker/build.sh all, or select espidf,
platformio-arduino or arduino-ide. EDP_PIO_COMMAND,
EDP_ARDUINO_CLI_COMMAND and EDP_DEMO_JOBS select installed build tools and
parallelism. The build calls the existing EDP-Localisation generator through
the shared deterministic diagnostic fixture; generated outputs stay ignored.

The shared container header supplies EDP_DEMO_DWELL_NS, default 100000000 ns.
This example requires 100 ms through 1 second so its isolated four-record
batches and one partial have the stated segment counts. Other batch
configurations, including zero dwell, have different archive-generation counts.
The public batch API retains its independent compile-time bounds and
ConfigureDwell contract.

Explicitly provision the supplied 2 MiB partition layout and a valid empty
edplfs LittleFS image at 0x170000, length 0x90000, before first use.
Normal startup never formats a failed mount or erases NVS. Identity advancement
uses the existing container Bootstrap, independently committed/read back before
SystemIdentity construction. One immutable facade is bound before source
construction; each queued Record owns its values and carries no facade pointer
or live diagnostic borrow. The attempt counter starts fresh only for that new
durable incarnation.

## What the application proves

Nine sources publish before the worker starts. The RAM receipt must report
published=9 and durable=0. The worker's source callback uses the existing
generated encoder, copied representation and strict typed delivery. The same
ordinary coordinator services the batch and waits for its canonical deadline.

Eight records form two four-record segments. The ninth source retires while
only eight are durable; the RAM_RETIRED receipt makes that distinction explicit.
With no subsequent input, the existing service path commits the idle partial.
Success requires all nine sources retired and durably completed, confirmed
cooperative worker stop/quiescence and real Memory/Threading teardown.

Bootstrap then reconstructs the original source coordinates, identity, message,
severity and generated arguments and compares canonical encodings for every
retained record. The exact latest two segments hold source tickets 5 through 9,
five records with all fifteen permanent field bindings. Normal reboot advances
incarnation 1 to 2 and segment ACK 3 to 6.

Failures print a typed stage and retain ownership resident. There is no implicit
flush, uncertain-commit retry, unsafe task kill or success before confirmed
completion. This is normal-boot application evidence, not a performance,
endurance or electrical-cut claim.

## Canonical atomic selection

The pinned Arduino SDK disables compiler hardware atomics globally. The existing
AdmissionLifecycleCoordinator accepts an explicit existing Platform AtomicWord32
provider for its own lifecycle words. This example selects the ESP-IDF provider,
which validates internal-DRAM placement and uses native CAS. SDK flags stay
unchanged. The original standard-atomic default still rejects an unsupported
always-lock-free configuration. See the lifecycle atomic-binding contract.

## Appliance evidence

All three builds passed:
- Arduino IDE: 20261031T000001-60bb41f1.
- PlatformIO Arduino: 20261031T000435-4f6cb63b.
- ESP-IDF: 20261031T000434-d45f5476.

All three actual image/filesystem verifications and two-boot executions passed
20261031T001504-eee39309, completed 00:16:53 CET on 31 October. Each target
emitted both RAM distinctions, exact replay and incarnation 1->2 / ACK 3->6.
The runner stays in ROM/stub through full image and filesystem verification,
then starts the application. Backup defaults full; an explicit authorised
disposable opt-in was used for these tests. Target independently verified OFF.

Image SHA256:
- Arduino IDE: 5a6e7e040622a9e76dd6d8f52f6a1c9b7b93bf722443166ae195651e6476a44d.
- PlatformIO Arduino: 9fe9cd767778e233295f37cfb4edd11b5f66d220bce100831d69154940add549.
- ESP-IDF: 8929fca1e85831cd4cee0693013c6f4b96d2c532eaebada0c9a965cf9d5c5466.

GCC/Clang UBSan, both lifecycle atomic bindings and contended scope/drain tests:
000433-77e1c128 PASS. Producer/queue/real-Memory/typed-Task/VFS regressions:
000929-5a3dd6bc PASS. Controller/media negatives: 001153-47d3672e PASS, 102 tests.
Earlier compile and harness-initialisation failures are preserved in the living
handoff. All execution used RPI400, with no GitHub Actions coverage dependency.
Private UART and images remain on the appliance.

## PlatformIO dependency setup

The two PlatformIO manifests declare GitHub URLs in lib_deps.
Keep this demo's repository-owned source and project assets together,
then run either command from the repository root:

```sh
pio run -d demos/TypedManagedBatchWorker/PlatformIO_Arduino
pio run -d demos/TypedManagedBatchWorker/PlatformIO_ESP-IDF
```

PlatformIO downloads the libraries; sibling library checkouts and local
dependency path overrides are unnecessary. Arduino-ESP32 selects the
qualified3.3.12 framework and ESP-IDF5.5 library bundle. Project-owned
partition files and build hooks are resolved within the demo itself.

The native Platform-ESP-IDF provider is selected by GitHub commit
87f9a71918be0ac606b5acc2479204fbe45b77db, whose provider source matches the
earlier qualified adc4164 snapshot. Its manifest selects Clock/main.
The separate native provider main reintegration remains outstanding;
this explicit pin does not claim it is already merged.

PlatformIO's preparation hook generates the schema using the downloaded
Logging fixture tool and Localisation owner. No external generator checkout
or manually supplied generated-header include path is needed. The Arduino
IDE helper retains its documented separate schema-generation setup.
