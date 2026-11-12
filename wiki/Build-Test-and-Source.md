# Build and test qualification

Native GCC and Clang suites include negative compile cases, canonical wire, Memory/Threading integration, queue lifetime, EDLS capacity/rolling/recovery and Python media audits. Additional ESP-IDF/Arduino firmware builds and RPI400 relay-controlled hard power cuts are separate qualification dimensions.

## Boundaries

Use exact detached run receipts and the source commit recorded in the [qualification ledger](../docs/LOGGING-QUALIFICATION-STATUS.MD). Do not mark a build, flash, cut or durability proof PASS without its actual receipt.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.

## 9 November 2026 — additional minimal core demonstration

The newly maintained `demos/FramingAndInstrumentation/` family contains separate Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF applications, all using the **existing C++20-only** `ESPressio_Logging.hpp` umbrella (no EDP concrete dependencies). It checks compile-time severity gating including zero evaluated work when disabled; one single-context `AttemptSequence` allocation; EDLG bounded framed JSON payload; strict borrowed `ReadEdlgFrame` view; and CRC rejection of a corrupted frame. The JSON bytes `{}` **are not** the generated 15-field diagnostic LogRecord: no translation, CBOR, serial transport, archive persistence or NVS ACK is claimed. See [minimal demo contract](../demos/FramingAndInstrumentation/README.MD). Each target must actually be compiled before qualification; a checked-in source tree or successful compile alone never proves firmware executed on an ESP32.
## 12 November 2026 — current Logging main post-merge verification

Published `EDP-Logging/main` code-equivalent baseline `9fea51b0bdebcd8a4206628b1e1def81ff620381`, branch version `0.1.0` with no tag. Host/native job `20261112T092223-3f2f828e` PASSED **83/83**: 34 GCC + 34 Clang Logging tests, eight optional downstream Serial/Serial-ESP-IDF feature host tests, and all three demo families compiled/linked for both PlatformIO Arduino-ESP32 and ESP-IDF, plus Python. Exact final Python docstring/CLI description changes passed 40 tests. The separate 9–10 Nov and earlier physical WROOM finite-rate and genuine relay PowerLoss receipts remain distinct from these build-only 12 Nov results.

Current [qualification status](../docs/LOGGING-QUALIFICATION-STATUS.MD), [acceptance matrix](../docs/ACCEPTANCE-G1-G15.MD) and [Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-09/2026-11-09-09-29-EDP-Logging-Dependency-First-Reintegration.MD) distinguish exact tested profiles, historical negatives, pending full V2 source member audit and optional providers. The RPI400 lock was absent and MCU power OFF at final verification.
