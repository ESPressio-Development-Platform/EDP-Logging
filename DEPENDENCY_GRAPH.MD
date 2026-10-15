# EDP-Logging Dependency Graph

**Current branch scope:** only primitive C++20 Severity and AttemptSequence declarations are implemented. Their code has no direct EDP-* dependency or platform SDK dependency.

Current mandatory graph:

```text
EDP-Logging (Severity, AttemptSequence) -> C++20 standard library
```

**Planned, not implemented:** EDP-System (identity + Composition), EDP-Clock (time capture), EDP-Memory (pooled lifetime), EDP-BoundedTopology (FIFO), EDP-Threading (sequencer/coordinator/encoder Tasks), EDP-Localisation (governed schema), EDP-Serialisation (CBOR/JSON), EDP-Persistence (archive sink). EDP-Serial must remain a downstream **optional** EDP-Serial -> EDP-Logging sink integration, never a Logging -> Serial dependency. Exact mandatory, optional activation and transitive dependency tree will be updated when each direct include is introduced.

No `ESPressio-*` deprecated library dependency is permitted; the currently used source namespace `ESPressio::Logging` remains the active EDP API namespace.
