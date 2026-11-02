# Public and opt-in API

The public foundation is Severity, AttemptSequence, EDLG/EDLS framing and compile-time instrumentation. Explicit optional headers expose governed record assembly, Memory/Threading queue ownership, bounded worker/sinks, native Persistence checkpoint and immutable EDLS batch acknowledgement. An accepted source record is not a durable receipt.

## Boundaries

Use the [Reference Index](Reference-Index) for every header, and the [G1–G15 matrix](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ACCEPTANCE-G1-G15.MD) to distinguish qualified application profiles from unsupported universal claims.

Serial byte output is not a mandatory Logging API: EDP-Serial optionally consumes the canonical EDLG frame writer and owns frame pumping, FIFO arbitration and native provider selection. Its accepted-byte result is not durability or remote receipt. See [G12 Serial output integration](../docs/SERIAL-OUTPUT-INTEGRATION.MD); the concrete Arduino owner remains a stub.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.
