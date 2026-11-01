# Dependency and provider contracts

The bare ESPressio_Logging.hpp umbrella requires only the standard C++20 foundation. Optional diagnostic and durable worker headers declare direct Memory, Clock, Threading, System, Serialisation, Localisation, BoundedTopology, Persistence and selected standard C++ portable concrete dependencies. An app must supply all direct EDP library roots; do not rely on transitive includes.

## Boundaries

The complete and conditional dependency matrix is maintained in the [dependency graph](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/DEPENDENCY_GRAPH.MD). The opt-in owned argument arena uses stateless EDP-Memory ByteOperations with portable default.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.
