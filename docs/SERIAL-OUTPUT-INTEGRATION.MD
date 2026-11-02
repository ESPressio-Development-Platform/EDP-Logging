# Serial output integration and qualification (G12)

This contract explains how an application can emit EDP-Logging's portable EDLG frames through the *existing* EDP-Serial owner. Serial output is optional: logging, finite EDLS retention and strict storage acknowledgement do not require a UART, CDC, console, USB transport or an EDP-Serial dependency.

## Owner and dependency direction

- `EDP-Logging` owns the canonical `logging/EdlgFrame.hpp` writer and framed record format. Its default umbrella and `library.json` have no Serial dependency.
- `EDP-Serial` (`feature/logging-v1`) owns the abstract Composition `ByteStreamWriter` capability, bounded `TryWriteComplete`, the optional `serial/EdlgFramePump.hpp`, and the `serial/BoundedEdlgFrameQueue.hpp`. Only its **optional frame pump and queue** import Logging; the default Serial umbrella depends directly on EDP-System, not Logging.
- `EDP-Serial-ESP-IDF` (`feature/logging-v1`) owns `UartFifoByteStreamWriter` for an explicitly configured ESP-IDF UART whose driver TX-buffer size is zero. It adapts the selected native driver to EDP-Serial's *nonblocking* byte endpoint. It does not create a second frame encoder.
- The application Bootstrap owns the actual UART pins/baud/driver/endpoint lifetime, exclusive endpoint access, Threading synchronization, queue sizing, output worker and error policy. It must explicitly declare all direct source dependencies of the selected optional feature.

## Source and runtime boundaries

1. A Logging producer supplies an already encoded payload and its canonical codec; EDP-Serial's `EdlgFramePump` uses Logging's existing frame writer to make one **complete** owned EDLG frame.
2. The bounded frame pump retains its unsent suffix across partial native writes, `WouldBlock` and provider failure; it does not retry indefinitely or claim remote reception.
3. The optional frame queue serializes **only participants of one queue** through the existing EDP-Threading mutex, preserving frame boundaries and bounded committed order. Direct console/printf output or a second queue on the same UART can still interleave bytes; the application must enforce endpoint exclusivity.
4. A native `Accepted` count is the exact number of bytes accepted into the provider/FIFO, **not** electrical transmission, remote host receipt, filesystem durability or a PowerLoss commitment. Data already accepted before failure cannot be rolled back.
5. The UART writer's low-level `uart_tx_chars` contract depends on zero TX buffering and an installed driver. The concrete provider must not be used as a universal adapter to any Arduino `Stream`, whose write operation may block or expose different acceptance semantics.

## Implementation locations and activation

| Repository / ref | Current owned implementation | Activation and qualification |
| --- | --- | --- |
| EDP-Serial `feature/logging-v1` @ `cc3857fa7767` | Abstract byte endpoint; EDLG frame pump; bounded FIFO | Optional; `main` currently contains only scaffolding |
| EDP-Serial-ESP-IDF `feature/logging-v1` @ `2ae01782ab4e` | Nonblocking UART FIFO byte writer | Optional ESP-IDF/Arduino-underlying-IDF provider; `main` currently scaffolding |
| EDP-Serial-Arduino `main` @ `99b5f53b558f` | No source beyond LICENSE/.gitignore | **Not implemented**; no Arduino Serial-specific endpoint contract or concrete provider is available |
| EDP-Logging `feature/logging-v1` @ the selected Logging revision | Canonical EDLG frame writer, optional source/sink integration tests | There is intentionally no `Logging → Serial` production dependency |

## Verified tests and limits

On 2 November 2026, detached RPI400 run `20261102T103854-7cde01e7` passed **eight** GCC/Clang executions: EDP-Serial's abstract byte endpoint, opt-in complete-frame pump, bounded multi-producer frame queue and ESP-IDF native UART FIFO provider (with fake IDF). All tests use the actual selected Logging EDLG header; none duplicates the codec.

Previous physical ESP32 checkpoint and Serial HIL in the Logging acceptance ledger are configuration-specific and **do not** prove an Arduino `HardwareSerial` adapter, USB CDC delivery, arbitrary concurrent console writers, electrical UART TX drain, or simultaneous multiprovider output. A host fake-IDF pass is not a physical wire-capture result.

## Tracking

The missing Arduino provider and physical integration work is tracked in [EDP-Serial-Arduino issue #1](https://github.com/ESPressio-Development-Platform/EDP-Serial-Arduino/issues/1). The existing abstract and ESP-IDF feature owners have their own tests; Logging source itself requires no new Serial dependency.

## Remaining acceptance gate

**G12 is partial.** To claim universal Arduino/ESP-IDF Serial completeness, `EDP-Serial-Arduino` must implement an explicitly qualified nonblocking endpoint in **that owning repository** (not in Logging), and demonstrate the application's exact native stream/driver selection, no interleaving, finite resource and complete frame delivery on the intended hardware. The selected Arduino serial interface, supported boards, and its actual nonblocking acceptance guarantee must be resolved before designing that concrete adapter. No compatible behavior should be invented by wrapping a potentially blocking write call. Branch selection for work in that currently stub repository is independently governed by V2/BRANCHING.MD.

Meanwhile the currently tested ESP-IDF provider and abstract frame pump remain opt-in on their existing feature branches. Do not promote their behavior or dependency pins to their stub `main` branches, and do not hold the separately qualified **file-backed** Logging core hostage to unrelated serial output options.

See [EDLG checkpoint](EDLG-KEYVALUE-CHECKPOINT.MD), [architecture](ARCHITECTURE.MD), [acceptance matrix](ACCEPTANCE-G1-G15.MD) and the [dependency graph](../DEPENDENCY_GRAPH.MD).
