# Dependency and provider contracts

The bare ESPressio_Logging.hpp umbrella requires only the standard C++20 foundation. Optional diagnostic and durable worker headers declare direct Memory, Clock, Threading, System, Serialisation, Localisation, BoundedTopology, Persistence and selected standard C++ portable concrete dependencies. An app must supply all direct EDP library roots; do not rely on transitive includes.

## Boundaries

The complete and conditional dependency matrix is maintained in the [dependency graph](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/feature/logging-v1/DEPENDENCY_GRAPH.MD). The opt-in owned argument arena uses stateless EDP-Memory ByteOperations with portable default.

The newest-source optional import and branch-pin review is maintained in the [G14 owner baseline](../docs/DEPENDENCY-OWNER-BASELINE.MD). The optional Serial output family is an EDP-Serial-to-Logging edge (never a mandatory Logging dependency); its Arduino concrete is not yet implemented. See the [G12 Serial integration contract](../docs/SERIAL-OUTPUT-INTEGRATION.MD) and [EDP-Serial-Arduino issue #1](https://github.com/ESPressio-Development-Platform/EDP-Serial-Arduino/issues/1).

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.


The [37-repository G14 source inventory](../docs/DEPENDENCY-OWNER-BASELINE.MD#2-november-2026--refreshed-37-repository-g14-source-audit) on 2 November 2026 covers the latest published branch tips and conditional direct source imports. It does not certify all application-selected provider combinations or generated transitive build graphs; tracked unrelated Mesh/Platform V2 enum findings are not Logging implementation dependencies. The owner README fixes and remaining audits are recorded in the dated Policy handoff.


Four current-source, SHA-pinned whole-application firmware builds passed on RPI400 on 2 November 2026: G7 strict 15-field NVS-acknowledged EDLS and G1 Serial/UART-integrated applications under ESP-IDF 5.5.5; finite-batch and managed-worker sample applications under Arduino-ESP32 3.3.12. Exact receipts and scope are in the [G14 owner baseline](../docs/DEPENDENCY-OWNER-BASELINE.MD#2-november-2026--current-source-native-and-arduino-selected-application-builds). They certify selected build/link combinations, **not** Arduino Serial provider implementation, real UART delivery, additional physical PowerLoss tests, a universal PlatformIO resolver topology, or every possible provider Composition.


For the optional typed diagnostic schema, **build-time** Localisation generation must use the `feature/logging-v1` diagnostic-`parameters` grammar; `Localisation/main` cannot generate it. Opt-in ClockEra encoding requires the `Clock/mesh_v1` published header surface, not `Clock/main`. The selected native G7 profile otherwise compiled with `Memory/main`, and separately validated already-generated schema consumption with `Localisation/main` runtime headers. See [current G14 branch qualification](../docs/DEPENDENCY-OWNER-BASELINE.MD#2-november-2026--typed-diagnostic-schema-generator-branch-qualification); this is an explicit application-selected optional dependency contract, not an automatic manifest upgrade.
