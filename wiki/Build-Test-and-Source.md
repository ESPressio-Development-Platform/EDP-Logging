# Build and test qualification

The [19 November release assessment](../docs/ACCEPTANCE-G1-G15.MD#current-release-assessment--19-november-2026)
supersedes the historical open-work statements below. Current qualification
passed all 68 Logging host runners,40 Python tests,eight consuming Serial
cases and all12 demo builds. The current retained-claim and owned-async
WROOM executions, final five-minute workload and independent raw-media
receipts are recorded there with exact executable revisions and scope.
Declaration/reference and compiler-control reconciliation is complete
for this selected release. No hosted Actions execution is claimed.

Native GCC and Clang suites include negative compile cases, canonical wire, Memory/Threading integration, queue lifetime, EDLS capacity/rolling/recovery and Python media audits. Additional ESP-IDF/Arduino firmware builds and RPI400 relay-controlled hard power cuts are separate qualification dimensions.

## Boundaries

Use exact detached run receipts and the source commit recorded in the [qualification ledger](../docs/LOGGING-QUALIFICATION-STATUS.MD). Do not mark a build, flash, cut or durability proof PASS without its actual receipt.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.

## 9 November 2026 — additional minimal core demonstration

The newly maintained `demos/FramingAndInstrumentation/` family contains separate Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF applications, all using the **existing C++20-only** `ESPressio_Logging.hpp` umbrella (no EDP concrete dependencies). It checks compile-time severity gating including zero evaluated work when disabled; one single-context `AttemptSequence` allocation; EDLG bounded framed JSON payload; strict borrowed `ReadEdlgFrame` view; and CRC rejection of a corrupted frame. The JSON bytes `{}` **are not** the generated 15-field diagnostic LogRecord: no translation, CBOR, serial transport, archive persistence or NVS ACK is claimed. See [minimal demo contract](../demos/FramingAndInstrumentation/README.MD). Each target must actually be compiled before qualification; a checked-in source tree or successful compile alone never proves firmware executed on an ESP32.
## 12 November 2026 — current Logging main post-merge verification

Published `EDP-Logging/main` code-equivalent baseline `9fea51b0bdebcd8a4206628b1e1def81ff620381`, branch version `0.1.0` with no tag. Host/native job `20261112T092223-3f2f828e` PASSED **83/83**: 34 GCC + 34 Clang Logging tests, eight optional downstream Serial/Serial-ESP-IDF feature host tests, and all three demo families compiled/linked for both PlatformIO Arduino-ESP32 and ESP-IDF, plus Python. Exact final Python docstring/CLI description changes passed 40 tests. The separate 9–10 Nov and earlier physical WROOM finite-rate and genuine relay PowerLoss receipts remain distinct from these build-only 12 Nov results.

Current [qualification status](../docs/LOGGING-QUALIFICATION-STATUS.MD), [acceptance matrix](../docs/ACCEPTANCE-G1-G15.MD) and [Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-09/2026-11-09-09-29-EDP-Logging-Dependency-First-Reintegration.MD) distinguish exact tested profiles, historical negatives, the then-pending V2 source member audit and optional providers. The RPI400 lock was absent and MCU power OFF at final verification.

## Maintained workflow dependency selection

Host foundation, optional ClockEra and ESP-IDF firmware workflows use `main` for EDP-System, EDP-Localisation, EDP-Serialisation, EDP-Memory, EDP-Threading and EDP-Clock where each job selects those owners. The previous workflow refs for Memory, Threading and Localisation resolved to the same commits at this checkpoint. System, Serialisation and Clock advanced only documentation or ignore rules relative to the previous refs. The native fixture's specific ESP-IDF Platform commit remains an application-profile selection. The optional Serial abstract and ESP-IDF companion checkouts also use their verified `main` refs.

The existing RPI400 host and native receipts apply to their recorded checkout topology. The workflow changes here correct branch selection. Hosted workflow execution is tracked independently.

The [G12 integration ledger](../docs/SERIAL-OUTPUT-INTEGRATION.MD) records the RPI400 physical UART fixture and companion post-main runs. The G1 workflow generates its diagnostic schema with the existing Localisation tool before compiling the fixture. The host fan-out fixture uses the Portable SpinLock and the native fixture uses the ESP-IDF SpinLock.


The public demo manifests select the PIOArduino ESP32 platform and C++20.
See the [Arduino-ESP32 framing manifest](../demos/FramingAndInstrumentation/PlatformIO_Arduino/platformio.ini)
for the qualified pin and build flags. The native workflow runs all eight
PlatformIO demo combinations. RPI400 also compiles the four Arduino IDE
sketches using the Arduino CLI and ESP32 core.


## Reproducing the selected Arduino profile

The four PlatformIO Arduino demo manifests explicitly select
Arduino-ESP32 3.3.12 and its ESP-IDF 5.5 library bundle alongside the
recorded PIOArduino platform revision. The framing recipe uses the same
pins as the two managed demo families. Preserve both package selections
when reproducing the qualified build.

Concurrent RPI400 native and demo builds use distinct
PLATFORMIO_BUILD_CACHE_DIR locations. A shared SCons cache replacement
failure was observed during the first final sweep. The isolated-cache
retry and its exact run IDs are recorded in the current Policy handoff.

## Owned asynchronous delivery example

[OwnedAsyncSink](../demos/OwnedAsyncSink/README.MD) adds the fourth
three-target family. Its common Bootstrap selects one existing Memory
payload slot and Threading Dedicated Worker. The Worker checks its copy
after source mutation; Bootstrap verifies terminal delivery, exact lease
release and runtime teardown. All three targets built on source
db0bdf797756709b51ed8c926af4f6a09671c4fa. The native WROOM execution
20261119T080249-d436698d passed with result 0 and verified restoration
of the original full flash image. The native workflow covers eight
PlatformIO combinations; RPI400 also built all four Arduino IDE sketches.
See the demo README for exact receipts and [async ownership](../docs/ASYNC-COPYING-TASK-SINK.MD)
for the provider-wait and failure-preservation contracts.
