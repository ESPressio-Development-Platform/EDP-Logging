# Serial output integration and qualification (G12)

This contract explains how an application can emit EDP-Logging's portable EDLG frames through the *existing* EDP-Serial owner. Serial output is optional: logging, finite EDLS retention and strict storage acknowledgement do not require a UART, CDC, console, USB transport or an EDP-Serial dependency.

## Owner and dependency direction

- `EDP-Logging` owns the canonical `logging/EdlgFrame.hpp` writer and framed record format. Its default umbrella and `library.json` have no Serial dependency.
- `EDP-Serial` (`main`) owns the abstract Composition `ByteStreamWriter` capability, bounded `TryWriteComplete`, the optional `serial/EdlgFramePump.hpp`, and the `serial/BoundedEdlgFrameQueue.hpp`. Only its **optional frame pump and queue** import Logging; the default Serial umbrella depends directly on EDP-System, not Logging.
- `EDP-Serial-ESP-IDF` (`main`) owns `UartFifoByteStreamWriter` for an explicitly configured ESP-IDF UART whose driver TX-buffer size is zero. It adapts the selected native driver to EDP-Serial's *nonblocking* byte endpoint. It does not create a second frame encoder.
- The application Bootstrap owns the actual UART pins/baud/driver/endpoint lifetime, exclusive endpoint access, the ordinary Threading Mutex, the Platform state SpinLock, queue sizing, output worker and error policy. It must explicitly declare all direct source dependencies of the selected optional feature.

## Source and runtime boundaries

1. A Logging producer supplies an already encoded payload and its canonical codec; EDP-Serial's `EdlgFramePump` uses Logging's existing frame writer to make one **complete** owned EDLG frame.
2. The bounded frame pump retains its unsent suffix across partial native writes, `WouldBlock` and provider failure; it does not retry indefinitely or claim remote reception.
3. The optional frame queue serializes **only participants of one queue** through the existing EDP-Threading Mutex, preserving frame boundaries and bounded committed order. A separate Bootstrap-owned Platform SpinLock protects brief depth and poison snapshots. Direct console/printf output or a second queue on the same UART can still interleave bytes; the application must enforce endpoint exclusivity.
4. A native `Accepted` count is the exact number of bytes accepted into the provider/FIFO, **not** electrical transmission, remote host receipt, filesystem durability or a PowerLoss commitment. Data already accepted before failure cannot be rolled back.
5. The UART writer's low-level `uart_tx_chars` contract depends on zero TX buffering and an installed driver. The concrete provider must not be used as a universal adapter to any Arduino `Stream`, whose write operation may block or expose different acceptance semantics.

## Implementation locations and activation

| Repository / ref | Current owned implementation | Activation and qualification |
| --- | --- | --- |
| EDP-Serial `main` @ `707455b5a577` | Abstract byte endpoint, EDLG frame pump and bounded FIFO | Optional source dependency of the selected Serial application |
| EDP-Serial-ESP-IDF `main` @ `11bca276fa6a8` | Nonblocking native UART TX FIFO byte writer | Optional ESP-IDF provider, including the qualified IDF-backed Arduino toolchain |
| EDP-Serial-Arduino `main` @ `99b5f53b558f` | No source beyond LICENSE/.gitignore | **Not implemented**; no Arduino Serial-specific endpoint contract or concrete provider is available |
| EDP-Logging `main` | Canonical EDLG frame writer, optional source/sink integration tests | There is intentionally no `Logging → Serial` production dependency |

## Verified tests and limits

On 2 November 2026, detached RPI400 run `20261102T103854-7cde01e7` passed **eight** GCC/Clang executions: EDP-Serial's abstract byte endpoint, opt-in complete-frame pump, bounded multi-producer frame queue and ESP-IDF native UART FIFO provider (with fake IDF). All tests use the actual selected Logging EDLG header; none duplicates the codec.

On 12 November, [the recovery handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-12/2026-11-12-11-28-EDP-Logging-Serial-Release-Recovery.MD) records the exact pre- and post-main checks. The physical ESP32-WROOM G1 fixture `20261112T120312-447090b1` passed all 20 checks. It used the native Platform-ESP-IDF SpinLock, Threading Mutex and UART FIFO writer, and the independent host capture verified the exact EDLG payload and CRC. This qualifies the fixture's exclusive UART0 ownership and selected application profile. The application still controls any required TX drain and host acknowledgement.

On 13 November, the published Serial predicate migration passed the RPI400 GCC/Clang consumer checks, native UART builds and a fresh isolated G1 WROOM build. The physical UART0 repeat passed all 20 checks with independently captured EDLG payload and CRC. The original 4 MiB flash image was restored and verified before powering the integrated target off. The [living handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-13/2026-11-13-08-38-EDP-Logging-Serial-Qualification.MD) records the exact Logging and Serial commits for each result.

The generic [EDP-Serial-Arduino provider](https://github.com/ESPressio-Development-Platform/EDP-Serial-Arduino/issues/1), USB CDC transport, independent console writers and other target profiles remain separate. A supported Arduino-ESP32 build can select the IDF-backed provider only when its SDK and driver profile provide the same UART contract.

See [EDLG checkpoint](EDLG-KEYVALUE-CHECKPOINT.MD), [architecture](ARCHITECTURE.MD), [acceptance matrix](ACCEPTANCE-G1-G15.MD) and the [dependency graph](../DEPENDENCY_GRAPH.MD).
