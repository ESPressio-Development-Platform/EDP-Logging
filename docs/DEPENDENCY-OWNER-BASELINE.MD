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
