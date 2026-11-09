# Build and test qualification

Native GCC and Clang suites include negative compile cases, canonical wire, Memory/Threading integration, queue lifetime, EDLS capacity/rolling/recovery and Python media audits. Additional ESP-IDF/Arduino firmware builds and RPI400 relay-controlled hard power cuts are separate qualification dimensions.

## Boundaries

Use exact detached run receipts and the source commit recorded in the [qualification ledger](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/LOGGING-QUALIFICATION-STATUS.MD). Do not mark a build, flash, cut or durability proof PASS without its actual receipt.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.

## 9 November 2026 — additional minimal core demonstration

The newly maintained `demos/FramingAndInstrumentation/` family contains separate Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF applications, all using the **existing C++20-only** `ESPressio_Logging.hpp` umbrella (no EDP concrete dependencies). It checks compile-time severity gating including zero evaluated work when disabled; one single-context `AttemptSequence` allocation; EDLG bounded framed JSON payload; strict borrowed `ReadEdlgFrame` view; and CRC rejection of a corrupted frame. The JSON bytes `{}` **are not** the generated 15-field diagnostic LogRecord: no translation, CBOR, serial transport, archive persistence or NVS ACK is claimed. See [minimal demo contract](../demos/FramingAndInstrumentation/README.MD). Each target must actually be compiled before qualification; a checked-in source tree or successful compile alone never proves firmware executed on an ESP32.
