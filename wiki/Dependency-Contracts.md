# Dependency and provider contracts

The bare ESPressio_Logging.hpp umbrella requires only the standard C++20 foundation. Optional diagnostic and durable worker headers declare direct Memory, Clock, Threading, System, Serialisation, Localisation, BoundedTopology, Persistence and selected standard C++ portable concrete dependencies. An app must supply all direct EDP library roots; do not rely on transitive includes.

## Boundaries

The complete and conditional dependency matrix is maintained in the [dependency graph](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/DEPENDENCY_GRAPH.MD). The opt-in owned argument arena uses stateless EDP-Memory ByteOperations with portable default.

The newest-source optional import and branch-pin review is maintained in the [G14 owner baseline](../docs/DEPENDENCY-OWNER-BASELINE.MD). The optional Serial output family is an EDP-Serial-to-Logging edge (never a mandatory Logging dependency); its Arduino concrete is not yet implemented. See the [G12 Serial integration contract](../docs/SERIAL-OUTPUT-INTEGRATION.MD) and [EDP-Serial-Arduino issue #1](https://github.com/ESPressio-Development-Platform/EDP-Serial-Arduino/issues/1).

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.
