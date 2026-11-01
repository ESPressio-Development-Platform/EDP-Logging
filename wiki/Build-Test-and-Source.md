# Build and test qualification

Native GCC and Clang suites include negative compile cases, canonical wire, Memory/Threading integration, queue lifetime, EDLS capacity/rolling/recovery and Python media audits. Additional ESP-IDF/Arduino firmware builds and RPI400 relay-controlled hard power cuts are separate qualification dimensions.

## Boundaries

Use exact detached run receipts and the source commit recorded in the [qualification ledger](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/LOGGING-QUALIFICATION-STATUS.MD). Do not mark a build, flash, cut or durability proof PASS without its actual receipt.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.
