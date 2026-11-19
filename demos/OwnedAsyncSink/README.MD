# Independently owned asynchronous sink

This example composes Logging's AsyncCopyingTaskSink with one existing
EDP-Threading DedicatedWorkerLease and one EDP-Memory payload slot.
It submits the CBOR array [1, 2, 3], mutates the caller's original bytes,
and lets the Worker verify its independent copy. The example then checks
the final delivery result, returns the Memory lease and tears down both
runtimes.

The selected codec is CBOR, payload capacity is 32 bytes, Task capacity
is one and Worker stack capacity is 6144 bytes. The byte comparison and
copy use the application-selected Memory ByteOperations provider.
Bootstrap binds the existing canonical Clock to the ESP-IDF timebase.
Task admission uses AbandonImmediately and reports refusal explicitly.

## Building

PlatformIO installs the selected owners from the manifest's GitHub URLs. The
[dependency baseline](../../docs/DEPENDENCY-OWNER-BASELINE.MD) records the
qualified revisions, including the native Platform-ESP-IDF provider pin.
This example directly selects Clock, Memory, Threading, Platform,
Platform-Portable, Platform-FreeRTOS, Platform-ESP-IDF, System and
BoundedTopology. Use C++20.

| Target | Command |
| --- | --- |
| Arduino IDE | ./build.sh arduino-ide |
| PlatformIO Arduino-ESP32 | ./build.sh platformio-arduino |
| PlatformIO ESP-IDF | ./build.sh espidf |

The build helper accepts EDP_ARDUINO_CLI_COMMAND, EDP_PIO_COMMAND and
EDP_DEMO_JOBS to select installed tools and build parallelism.
The PlatformIO Arduino recipe fixes Arduino-ESP32 3.3.12 and the
ESP-IDF 5.5 library bundle. The Arduino IDE sketch is in
[Arduino_IDE](./Arduino_IDE). The two PlatformIO projects are in their
named target directories.

## Runtime result

A successful run prints EDP_LOGGING_ASYNC_DEMO|RESULT|0 at 115200 baud.
The typed Result enum in [AsyncDemo.hpp](./Arduino_IDE/src/AsyncDemo.hpp)
distinguishes clock, Memory, dispatch, completion, delivery and shutdown
failures.

The read gate ensures that the Worker examines its copy after the
caller mutates the source. Accepted Task admission, final callback
acceptance and confirmed Memory release are observed separately.
The payload is a small encoded CBOR value. The
[typed managed example](../TypedManagedBatchWorker/README.MD) demonstrates
the complete governed LogRecord and durable archive pipeline.

The example is one-shot. It retains static Bootstrap owners on any
uncertain outcome. Polling intervals use canonical Clock differences
and yield to the SDK scheduler. Their two-second checks apply between
owner operations. Native synchronization and release retain their
selected provider wait contracts. An application reconciles an
uncertain result before destroying these owners or starting another run.

The [async contract](../../docs/ASYNC-COPYING-TASK-SINK.MD) describes
concurrency and recovery requirements. Qualification receipts are kept
in the [acceptance matrix](../../docs/ACCEPTANCE-G1-G15.MD).

## Qualified execution

All three target builds passed on source
db0bdf797756709b51ed8c926af4f6a09671c4fa on RPI400:
ESP-IDF 20261119T075417-5f1e5abd, Arduino IDE
20261119T075931-d1708d46 and PlatformIO Arduino
20261119T075931-4c9d7d3a. Native image SHA-256:
98df2aac23247ccd52c12791e347808629152b0e608dac3740868ec78c5bc9ca.

Guarded WROOM execution 20261119T080249-d436698d passed with result 0.
The original full 4 MiB flash image was restored and verified, and the
board was powered off. The Arduino results are build qualifications;
the execution receipt applies to the ESP-IDF image.

## PlatformIO dependency setup

The two PlatformIO manifests declare GitHub URLs in lib_deps.
Keep this demo's repository-owned source and project assets together,
then run either command from the repository root:

```sh
pio run -d demos/OwnedAsyncSink/PlatformIO_Arduino
pio run -d demos/OwnedAsyncSink/PlatformIO_ESP-IDF
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
