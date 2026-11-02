# Conditional dependency ownership and cross-repository audit (G14)

EDP-Logging's root C++20 umbrella intentionally has **no mandatory EDP dependencies**. Importing opt-in source, generated record, queue, codec, worker, persistence or host tools imposes their respective direct owner contracts. This document fixes the **reviewed source revisions and boundaries** for those profiles; it is not a global certification of every build combination across the ESPressio organisation.

## Source-based direct dependency inventory

A scan of the 42 production C++ headers of the current Logging feature branch found the following actual external imports. The empty default `library.json` dependency list is true only of the **minimal umbrella**; it is not a promise that all optional headers compile in isolation.

| Owner | Import conditions / examples |
| --- | --- |
| EDP-Clock | Admission deadlines, lifecycle, worker, bounded sealed batches, temporal projection and LogRecord Clock fields |
| EDP-Threading | Source/queue admission, task ownership, synchronization and coordinator |
| EDP-Memory | Producer pools, representation fan-out, copying sinks and opt-in owned argument `ByteOperationsContract` |
| EDP-Platform | Ordinary mutex/no-wait and lifecycle atomic provider abstraction |
| EDP-Platform-Portable | Opt-in concrete byte-copy provider for OwnedDiagnosticArguments only |
| EDP-BoundedTopology | TransactionalRecordQueue finite slot occupancy |
| EDP-BoundedTypes | Bounded EDLS payload, typed generated projection and approved fields |
| EDP-System | Governed record Type/Field identity and generated record schema |
| EDP-Localisation | Generated diagnostic message identity/argument schema (including direct source import for LogRecord) |
| EDP-Serialisation | Optional typed CBOR/JSON, ClockEra adapters and strict UTF-8 text validation |
| EDP-Persistence | Opt-in archive FileStorage/KeyValueStorage and bounded checkpoint |

**EDP-Serial is NOT a direct Logging source dependency.** The optional cross-domain flow is `EDP-Serial → EDP-Logging` when the Serial caller imports its frame pump. EDP-Mesh is not needed for local Logging; qualified synchronized Clock evidence can be supplied by an application separately.

## Current revision baseline of directly relevant owners

The following refs were read from GitHub branch listings and their latest commits on **2 November 2026**, selecting the branch with the latest committer timestamp among published branches for each repository. Manifest and source reads on the specified ref were used for owner and activation analysis. A branch label is not a release tag.

| Repository | Reviewed latest branch | Commit prefix |
| --- | --- | --- |
| EDP-Logging | `feature/logging-v1` | `6692e17c18e6` (before this documentation tranche) |
| EDP-Serial | `feature/logging-v1` | `cc3857fa7767` |
| EDP-Serial-Arduino | `main` | `99b5f53b558f` (stub) |
| EDP-Serial-ESP-IDF | `feature/logging-v1` | `2ae01782ab4e` |
| EDP-Serialisation | `feature/logging-v1` | `4a2f1dea0a2a` |
| EDP-Persistence | `main` | `6f7134bb945f` |
| EDP-Persistence-Arduino | `main` | `b4b49461fa08` |
| EDP-Persistence-ESP-IDF | `main` | `d3e1febe6565` |
| EDP-Clock | `mesh_v1` | `ad45b912d693` |
| EDP-Memory | `feature/logging-v1` | `2c91dc3fa812` |
| EDP-Threading | `feature/logging-v1` | `739eeeba2206` |
| EDP-System | `main` | `c69d4b082db5` |
| EDP-Platform | `main` | `ee3256624045` |
| EDP-Platform-Portable | `main` | `4890e115305f` |
| EDP-Platform-ESP-IDF | `mesh_v1` | `adc416403101` |
| EDP-Platform-Arduino | `main` | `c311694ea905` |
| EDP-Platform-FreeRTOS | `main` | `2a52ba81e850` |
| EDP-Localisation | `feature/logging-v1` | `8b826f4c9bec` |
| EDP-BoundedTypes | `main` | `1992b515b9a2` |
| EDP-BoundedTopology | `main` | `362042c3916d` |

## Manifest and owner conclusions

1. **Branch selection is observable and consequential.** EDP-Serial and EDP-Serial-ESP-IDF have their operative source on `feature/logging-v1`; their `main` branches contain only LICENSE and .gitignore. EDP-Serial-Arduino is a stub on `main`. Do not promise these capabilities from `main` or fabricate an Arduino concrete provider in Logging.
2. **Some manifests refer to earlier `main` owners.** EDP-Serialisation's operative feature manifest lists EDP-Memory at `#main`, while Logging's native optional Memory path was validated against its newer `feature/logging-v1`. EDP-Threading's feature manifest similarly references Clock and Memory `#main`, not all branches used by the Logging test appliance. Those are actual transitive branch-selection differences; an application requiring the feature-specific contracts must explicitly pin compatible sources. Do not update cross-repository manifests without a deliberate consumer-compatibility review.
3. **Storage and durability ownership remains correctly layered.** EDP-Persistence has the abstract FileStorage/KeyValueStorage contracts. EDP-Persistence-ESP-IDF `main` implements VfsFileStorage with qualified profiles and NvsKeyValueStorage with a PowerLoss atomic **key-value** commit; ordinary VFS FileStorage does not become generally interruption-atomic. EDP-Persistence-Arduino `main` has real FileSystemStorage/PreferencesKeyValueStorage code, not a stub. ESP-IDF concrete source uses existing EDP-Memory ByteOperations, not a Logging-owned memory duplicate.
4. **UTF-8 and byte copying are reused.** Latest EDP-Serialisation feature source owns constexpr `ValidateUtf8` and strictly rejects U+0000, and the EDP-Memory feature source owns `ByteOperationsProviderTraits`. Logging's opt-in `OwnedDiagnosticArguments` composes these providers. The lightweight core remains C++20-only.
5. **Concrete platform activation is app-specific.** EDP-Platform-ESP-IDF's latest `mesh_v1` manifest also imports EDP-Security and Mesh-era Clock; using it indiscriminately in a no-Mesh Logging build could pull unnecessary requirements. Application Bootstrap chooses the correct Platform and Threading provider profile rather than inheriting every latest provider feature.
6. **Reverse dependency direction was checked.** An offline mirror-source grep across 37 EDP repositories found one non-Logging direct import of Logging: EDP-Serial's optional frame pump. The mirror inventory can lag GitHub (notably Logging and Serialisation), so this is qualified evidence only; the EDP-Serial import and its branch SHA were separately verified from current GitHub source.

## Integration tests and unqualified boundaries

The optional Serial EDLG consumer and ESP-IDF concrete provider compiled and passed **8 GCC/Clang host executions** on 2 November 2026 (`20261102T103854-7cde01e7`). Earlier Logging GCC/Clang, 30,000-record two-run WROOM and true relay PowerLoss evidence remains scoped to the exact Bootstrap/provider revisions in [Logging Qualification Status](LOGGING-QUALIFICATION-STATUS.MD). The Serial test is not a new physical TX-wire qualification and cannot be used to guarantee Arduino output.

**G14 disposition:** current core Logging source ownership and the **20 listed directly relevant owner heads** have a documented conditional import/manifest baseline, with identified version-pin differences rather than unreviewed implicit deps. This is a substantive partial review, but not a comprehensive audit of every declaration, all 37 current GitHub worktrees, all transitive build graphs and the complete Wiki/source semantic content of other repositories. Such an organisation-wide declaration remains open and must not be inferred from this document.

For source and resource contracts use [Architecture](ARCHITECTURE.MD), [Owned Argument Byte Operations](OWNED-ARGUMENT-BYTE-OPERATIONS.MD), [Strict EDLS Acknowledgement](EDLS-ACKNOWLEDGED-RECOVERY.MD), [Serial integration](SERIAL-OUTPUT-INTEGRATION.MD) and the [root dependency graph](../DEPENDENCY_GRAPH.MD).


## 2 November 2026 — refreshed 37-repository G14 source audit

The following audited baseline supersedes the earlier 20-owner snapshot **for branch-tip/source discovery**, not its prior dated evidence. RPI400 git mirror refresh 20261102T120539-c2b7046c PASS/exit0; C++ source/include analysis on the most recent committed branch of all 37 official EDP repositories PASS/exit0. Two appliance-only temporary validation clones were excluded. Source-root scanning found 29 repositories with C/C++ files directly under src, plus the substantive application-level EDP-Labs (30 substantive repositories total); seven are stubs. Direct header import graph had **zero cycles detected**, not a proof for generated, transitive conditional SDK, Python tooling or application Bootstrap edges.

| Repository | Branch | SHA | Source | Docs | Wiki | Direct owner imports | Flags |
|---|---|---|---:|---:|---:|---|---|
| EDP-BoundedTopology | main | 362042c3916d | 5 | 4 | 18 | none | none |
| EDP-BoundedTypes | main | 1992b515b9a2 | 20 | 5 | 32 | EDP-Memory | none |
| EDP-Clock | mesh_v1 | ad45b912d693 | 37 | 9 | 48 | EDP-Platform, EDP-System | none |
| EDP-Command | mesh_v1 | 6002c66d71fd | 9 | 2 | 13 | EDP-Clock, EDP-Memory, EDP-Primitives, EDP-Serialisation, EDP-System, EDP-Threading | none |
| EDP-Configuration | main | fe7e45e9d62a | 0 | 0 | 0 | none | none |
| EDP-Demos | main | cad92d70c2f3 | 0 | 0 | 0 | none | none |
| EDP-Event | mesh_v1 | 764de48d16ee | 14 | 9 | 27 | EDP-BoundedTopology, EDP-BoundedTypes, EDP-Clock, EDP-Memory, EDP-Primitives, EDP-System, EDP-Threading | none |
| EDP-Labs | mesh_concurrency_v1 | 11d1e2c54ac1 | 0 | 8 | 89 | none | none |
| EDP-Localisation | feature/logging-v1 | 8b826f4c9bec | 12 | 12 | 47 | EDP-Memory, EDP-Persistence, EDP-System | none |
| EDP-Logging | feature/logging-v1 | aaa5729e5960 | 42 | 57 | 62 | EDP-BoundedTopology, EDP-BoundedTypes, EDP-Clock, EDP-Localisation, EDP-Memory, EDP-Persistence, EDP-Platform, EDP-Platform-Portable, EDP-Serialisation, EDP-System, EDP-Threading | none |
| EDP-Memory | feature/logging-v1 | 85631612175f | 20 | 8 | 34 | EDP-BoundedTopology, EDP-Platform, EDP-System | none |
| EDP-Mesh | mesh_v1 | 18ed68ceb294 | 79 | 72 | 88 | EDP-BoundedTopology, EDP-Clock, EDP-Command, EDP-Event, EDP-Memory, EDP-Persistence, EDP-Primitives, EDP-Radio, EDP-RemoteSystem, EDP-Security, EDP-Serialisation, EDP-State, EDP-System, EDP-Threading | none |
| EDP-Persistence | main | 6f7134bb945f | 10 | 5 | 22 | EDP-System | none |
| EDP-Persistence-Arduino | main | b4b49461fa08 | 3 | 2 | 15 | EDP-Memory, EDP-Persistence | none |
| EDP-Persistence-ESP-IDF | main | d3e1febe6565 | 5 | 3 | 16 | EDP-Memory, EDP-Persistence | none |
| EDP-Platform | main | ee3256624045 | 19 | 5 | 31 | EDP-System | none |
| EDP-Platform-Arduino | main | c311694ea905 | 3 | 1 | 15 | EDP-BoundedTypes | none |
| EDP-Platform-ESP-IDF | mesh_v1 | adc416403101 | 15 | 14 | 26 | EDP-Clock, EDP-Memory, EDP-Persistence, EDP-Platform, EDP-Security, EDP-System | none |
| EDP-Platform-FreeRTOS | main | 2a52ba81e850 | 6 | 2 | 18 | EDP-Platform | none |
| EDP-Platform-Portable | main | 4890e115305f | 6 | 3 | 18 | EDP-Memory, EDP-Platform | none |
| EDP-Primitives | serialisation_prerequisites | 939d134afff3 | 10 | 4 | 21 | EDP-System | none |
| EDP-Profiling | main | acc5b22900a9 | 0 | 0 | 0 | none | none |
| EDP-Radio | mesh_v1 | d547ede45e5f | 9 | 6 | 22 | EDP-Clock, EDP-RemoteSystem, EDP-System | none |
| EDP-Radio-NRF24 | main | 1315a0bec0a0 | 8 | 17 | 20 | EDP-Radio | none |
| EDP-Radio-Raw80211 | mesh_v1 | f2a231cd9ae4 | 8 | 7 | 18 | EDP-Radio | none |
| EDP-RemoteSystem | main | 0b12714c5f4e | 8 | 4 | 21 | EDP-BoundedTopology, EDP-Memory, EDP-System, EDP-Threading | none |
| EDP-Security | mesh_v1 | a7482cbc5f45 | 18 | 9 | 30 | EDP-Memory, EDP-Platform, EDP-System | none |
| EDP-Serial | feature/logging-v1 | cc3857fa7767 | 5 | 3 | 6 | EDP-Logging, EDP-System, EDP-Threading | none |
| EDP-Serial-Arduino | main | 99b5f53b558f | 0 | 0 | 0 | none | none |
| EDP-Serial-ESP-IDF | feature/logging-v1 | 2ae01782ab4e | 2 | 1 | 3 | EDP-Serial | none |
| EDP-Serialisation | feature/logging-v1 | 4a2f1dea0a2a | 24 | 13 | 38 | EDP-BoundedTopology, EDP-BoundedTypes, EDP-Clock, EDP-Localisation, EDP-Memory, EDP-Platform-Portable, EDP-System | none |
| EDP-State | mesh_v1 | c1a5ff555186 | 13 | 8 | 26 | EDP-BoundedTopology, EDP-BoundedTypes, EDP-Clock, EDP-Memory, EDP-Primitives, EDP-RemoteSystem, EDP-System, EDP-Threading | none |
| EDP-System | main | c69d4b082db5 | 27 | 10 | 41 | none | none |
| EDP-Threading | feature/logging-v1 | 269d3dbc9b21 | 42 | 5 | 56 | EDP-BoundedTopology, EDP-Clock, EDP-Memory, EDP-Platform, EDP-System | none |
| EDP-WiFi | main | 8658af9e8079 | 0 | 0 | 0 | none | none |
| EDP-WiFi-Arduino | main | 73b5e33de240 | 0 | 0 | 0 | none | none |
| EDP-WiFi-ESP-IDF | main | f4497c3ce667 | 0 | 0 | 0 | none | none |

**G14 V2 findings and status.** All detected source imports were mentioned in the owning repository's README or DEPENDENCY_GRAPH; the scan does not independently certify per-header transitive closure or actual build-manifest version pins. Root README discovery for two current owner docs was corrected and published: EDP-Memory/feature/logging-v1 85631612175f0c4af62ee2250748ba4ac1f5c6e2 ([Memory #17](https://github.com/ESPressio-Development-Platform/EDP-Memory/issues/17)) and EDP-Threading/feature/logging-v1 269d3dbc9b2143f5bd3046182d20db57d97e32a3 ([Threading #12](https://github.com/ESPressio-Development-Platform/EDP-Threading/issues/12)). The isolated RPI400 detached documentation QA 20261102T120149-377c9b53 PASS/exit0 confirmed all current focused docs in those owners were linked and all README-local links resolved. No C++ code changes, no physical retest in this tranche.

A syntax inspection confirmed existing implicit/combined-line result enum declarations on read-only Mesh work branches: tracked in [EDP-Mesh #34](https://github.com/ESPressio-Development-Platform/EDP-Mesh/issues/34) and [EDP-Platform-ESP-IDF #13](https://github.com/ESPressio-Development-Platform/EDP-Platform-ESP-IDF/issues/13). No Mesh or linked Platform source was mutated. Unlinked-doc candidates in Clock (2), Mesh (13), Platform-ESP-IDF (3), Radio-NRF24 (1) and Radio-Raw80211 (1) must be distinguished between **current normative public contracts** and historical evidence/snapshots before raising more defects; README link-list count alone is not proof of policy non-compliance.

**Qualification boundary:** this is a comprehensive branch/source **inventory** across all 37 official EDP repositories, and a targeted direct-include graph audit. It is **not** an exhaustive read of every implementation line, an independent target build of every possible Composition/provider combination, a complete check of all README/Wiki source symbols, or universal V2 certification. Cross-repository manifest pin/version compatibility and representative selected provider/build profiles remain further G14 work. Logging #15 remains OPEN until those can be appropriately bounded and verified. See the [new dated Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-02/2026-11-02-11-46-EDP-Logging-G14-Cross-Repository-Audit.MD).
