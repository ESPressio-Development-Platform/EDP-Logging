# EDP-Logging Demonstrations

Each demonstrable family supplies Arduino IDE, PlatformIO Arduino-ESP32 and PlatformIO ESP-IDF source projects, using C++20 and explicit application-owned provider wiring where required.

- [Framing and Instrumentation](./FramingAndInstrumentation/README.MD) — minimal provider-free compile-time severity and attempt identity, bounded EDLG frame/reader and corruption rejection. An illustrative JSON payload **does not constitute a governed LogRecord**.
- [Typed Managed Batch Worker](./TypedManagedBatchWorker/README.MD) — generated fifteen-field LogRecord, bounded queue, dedicated Threading worker, CBOR, EDLS, checked file and strict NVS ACK/profile.
- [Configurable Bounded EDLS Batch](./ConfigurableBoundedEdlsBatch/README.MD) — application-selected finite EDLS batch size and dwell, with its own small CBOR frame and explicitly qualified concrete persistence. It is a bounded batch/retention demonstration, **not** the separate complete generated 15-field worker example.

A successful build is not physical execution or power-cut evidence. For exact bounded native and electrical test qualifications, consult [Logging qualification](../docs/LOGGING-QUALIFICATION-STATUS.MD) and [G1–G15 acceptance](../docs/ACCEPTANCE-G1-G15.MD). The application determines retention capacity and where logs are stored; no flash-life warranty, generic filesystem atomicity or unlimited throughput is implied.
