# Architecture and ownership

Logging owns source record capture, bounded queue/coordinator, sink fanout, EDLG/EDLS framing, and immutable archive acknowledgement. System owns schema and identities; Clock owns time coordinates; Localisation owns diagnostic catalogue; Serialisation owns canonical wire; Memory and Threading own allocation and execution; Persistence owns storage provider contracts. All concrete platform providers are chosen by application Bootstrap.

## Boundaries

The current normative source of this design is the [architecture document](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/docs/ARCHITECTURE.MD).

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.
