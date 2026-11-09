# Framing and Instrumentation — minimal Logging core demo

This is a **buildable, provider-free C++20** EDP-Logging exercise. It demonstrates compile-time severity gates (including provably disabled argument evaluation), a single-context nonwrapping attempt ticket, a complete bounded EDLG frame for a two-byte illustrative JSON payload, strict borrowed frame inspection, and fail-closed CRC rejection after payload corruption.

The tiny JSON payload `{}` is only an illustrative *already-encoded payload* used to exercise framing. **It is not an EDP-Localisation-generated, System-governed, 15-field LogRecord.** Do not use this simple framing example as evidence of schema validity, durable archive acknowledgement, filesystem PowerLoss guarantees, multi-producer concurrency or clock synchronization. Those capabilities are demonstrated and qualified separately by [Typed Managed Batch Worker](../TypedManagedBatchWorker/README.MD) and [Configurable Bounded EDLS Batch](../ConfigurableBoundedEdlsBatch/README.MD).

All storage is stack/static, with a 64-byte frame array and no runtime heap, task, FileStorage/KeyValueStorage, Platform Mutex or hidden provider. The output `written` changes only after complete frame publication. The returned `EdlgFrameView` borrows the local encoded payload inside `frame` and must never outlive it. A corrupt payload is rejected via `ChecksumMismatch`; CRC detects accidental corruption, **not** authenticity.

## Targets

- `Arduino_IDE/Arduino_IDE.ino` — install EDP-Logging with the configured C++20-capable Arduino-ESP32 core; inspect `EdpLoggingFramingDemoResult` with a debugger, where zero indicates the checks passed.
- `PlatformIO_Arduino` — run `pio run -d demos/FramingAndInstrumentation/PlatformIO_Arduino`. `EdpLoggingFramingDemoResult` is a volatile result sink, zero on success.
- `PlatformIO_ESP-IDF` — run `pio run -d demos/FramingAndInstrumentation/PlatformIO_ESP-IDF`. `app_main` stores the check outcome in the externally debugger-visible `EdpLoggingFramingDemoResult` for debugging.

**Build success does not prove the application executed on hardware.** The examples have deliberately no serial transport or persistent output; the higher-level full Logger demos own those facilities. The selected `EDP-Logging` package is the relative sibling source of each PlatformIO application. C++20 and zero mandatory EDP library dependencies apply to this minimal umbrella. All package versions remain 0.1.0.
