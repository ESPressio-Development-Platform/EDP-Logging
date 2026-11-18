# Dependency and provider contracts

The bare ESPressio_Logging.hpp umbrella requires only the standard C++20 foundation. Optional diagnostic and durable worker headers declare direct Memory, Clock, Threading, System, Serialisation, Localisation, BoundedTopology, Persistence and selected standard C++ portable concrete dependencies. An app must supply all direct EDP library roots; do not rely on transitive includes.

## Boundaries

The complete and conditional dependency matrix is maintained in the [dependency graph](../DEPENDENCY_GRAPH.MD). The opt-in owned argument arena uses stateless EDP-Memory ByteOperations with a Portable default. The EDLS empty-slot successor and oldest-eligible rotation use the same contract for frame assembly and exact-byte readback comparisons where their publication profiles require them.

The newest-source optional import and branch-pin review is maintained in the [G14 owner baseline](../docs/DEPENDENCY-OWNER-BASELINE.MD). The optional Serial output family is an EDP-Serial-to-Logging edge (never a mandatory Logging dependency); its Arduino concrete is not yet implemented. See the [G12 Serial integration contract](../docs/SERIAL-OUTPUT-INTEGRATION.MD) and [EDP-Serial-Arduino issue #1](https://github.com/ESPressio-Development-Platform/EDP-Serial-Arduino/issues/1).

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.


The [37-repository G14 source inventory](../docs/DEPENDENCY-OWNER-BASELINE.MD#2-november-2026--refreshed-37-repository-g14-source-audit) on 2 November 2026 covers the latest published branch tips and conditional direct source imports. It does not certify all application-selected provider combinations or generated transitive build graphs; tracked unrelated Mesh/Platform V2 enum findings are not Logging implementation dependencies. The owner README fixes and remaining audits are recorded in the dated Policy handoff.


Four current-source, SHA-pinned whole-application firmware builds passed on RPI400 on 2 November 2026: G7 strict 15-field NVS-acknowledged EDLS and G1 Serial/UART-integrated applications under ESP-IDF 5.5.5; finite-batch and managed-worker sample applications under Arduino-ESP32 3.3.12. Exact receipts and scope are in the [G14 owner baseline](../docs/DEPENDENCY-OWNER-BASELINE.MD#2-november-2026--current-source-native-and-arduino-selected-application-builds). They certify selected build/link combinations, **not** Arduino Serial provider implementation, real UART delivery, additional physical PowerLoss tests, a universal PlatformIO resolver topology, or every possible provider Composition.


**9 November 2026 promoted-main diagnostic qualification:** EDP-Localisation `main` at `8b826f4` now owns the `parameters` generator grammar, EDP-Clock `main` at `9a27b12` now owns `ClockEra.hpp`, and EDP-Serialisation `main` at `f36e615` retains the typed adapter. RPI400 source-tree-exact generated-G3/G4 integration and strict NVS-ACKed EDLS **passed all eight selected GCC/Clang/Localisation host checks** in `20261109T090236-f9d01ff1`. The 2 November negative tests for the earlier main commits remain valid historical source-provenance evidence; they must not be treated as currently missing contracts. [New 9 November requalification](../docs/DEPENDENCY-OWNER-BASELINE.MD#9-november-2026--promoted-main-generator-and-clockera-dependency-requalification) and [2 November original negative tests](../docs/DEPENDENCY-OWNER-BASELINE.MD#2-november-2026--typed-diagnostic-schema-generator-branch-qualification) distinguish the exact revisions.

**Limit:** these are explicit owner-selected profiles. Automated PlatformIO manifest resolution, arbitrary consumer dependency closures and untested native target combinations are not yet certified; no dependency manifest or package version was changed. `EDP-Serial-Arduino` remains an optional independent owner, not supplied by Logging.

The opt-in Serial integration tests are direct consumers of the EDP-Serial FIFO. The host fixture owns an EDP-Platform-Portable state SpinLock. The ESP-IDF G1 fixture owns an EDP-Platform-ESP-IDF state SpinLock and an EDP-Serial-ESP-IDF UART writer. Each queue borrows a separate short state SpinLock and an ordinary EDP-Threading Mutex for its lifetime. This selection belongs to the test application; consumers of the Logging production headers have no reverse Serial dependency.

### 18 November 2026 validation selection

The [selected owner revisions](../docs/DEPENDENCY-OWNER-BASELINE.MD#18-november-2026--reconciled-release-validation-selection)
record current branch tips and the exact native Platform-ESP-IDF pin.
The downstream EDP-Serial EDLG pump uses the current Memory-backed
writer contract at Serial `b021ff6`. The empty-slot EDLS successor uses
Memory ByteOperations for exact frame comparison in both the single-frame
and sealed profiles. The single-frame path uses the whole-file capture
already verified by archive inspection.
