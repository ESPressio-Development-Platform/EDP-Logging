# Reference — `src/ESPressio_Logging.hpp`

**Classification:** public C++20 entry point. [Reviewed source](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/73e39775eb0d4bd5398e479ae931aa42c47ab22a/src/ESPressio_Logging.hpp).

This aggregation header makes the six dependency-free foundation headers available in one include. It introduces no declarations of its own.

| Header | Facility |
| --- | --- |
| [Severity](Reference-logging-Severity) | Ordered diagnostic severity values and compile-time filtering. |
| [AttemptSequence](Reference-logging-AttemptSequence) | Width-specific tickets and non-wrapping issuance within an application incarnation. |
| [EdlgFrame](Reference-logging-EdlgFrame) | Bounded framing of already encoded CBOR or JSON. |
| [EdlgFrameReader](Reference-logging-EdlgFrameReader) | Verification and borrowed view of one complete frame. |
| [EdlsSegment](Reference-logging-EdlsSegment) | Portable EDLS segment header vocabulary and read/write operations. |
| [Instrumentation](Reference-logging-Instrumentation) | Lazy diagnostic site invocation selected at compile time. |

The include closure uses the C++20 standard library. Applications explicitly select the separate producer, queue, codec, storage and worker headers and their [conditional dependencies](../DEPENDENCY_GRAPH.MD). The [framing and instrumentation demo](../demos/FramingAndInstrumentation/README.MD) uses this entry point without other EDP libraries.
