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


## 2 November 2026 — manifest-pinned versus latest-owner GCC contract comparisons

A source/manifest comparison of all 37 latest published repository refs found **12 repositories** with at least one declared dependency ref pinned to a branch other than the newest-published working branch. These are potential profile-consistency checks, **not automatically defects**; stable main may intentionally be the contracted dependency for parts of the public surface. The bare Logging umbrella has zero mandatory dependencies; its optional headers and applications explicitly choose their own Composition/dependency closure.

| Repository | Differing manifest branch pins (declared → latest published) |
|---|---|
| `EDP-BoundedTypes` | EDP-Memory: `main` → `feature/logging-v1` |
| `EDP-Command` | EDP-Primitives: `main` → `serialisation_prerequisites`, EDP-Memory: `main` → `feature/logging-v1`, EDP-Threading: `main` → `feature/logging-v1`, EDP-Clock: `main` → `mesh_v1`, EDP-Serialisation: `main` → `feature/logging-v1` |
| `EDP-Event` | EDP-Primitives: `main` → `serialisation_prerequisites`, EDP-Memory: `main` → `feature/logging-v1`, EDP-Threading: `main` → `feature/logging-v1`, EDP-Clock: `main` → `mesh_v1` |
| `EDP-Localisation` | EDP-Memory: `main` → `feature/logging-v1` |
| `EDP-Mesh` | EDP-Primitives: `main` → `serialisation_prerequisites`, EDP-Threading: `main` → `feature/logging-v1`, EDP-Memory: `main` → `feature/logging-v1`, EDP-Serialisation: `main` → `feature/logging-v1` |
| `EDP-Platform-ESP-IDF` | EDP-Memory: `main` → `feature/logging-v1` |
| `EDP-Platform-Portable` | EDP-Memory: `main` → `feature/logging-v1` |
| `EDP-RemoteSystem` | EDP-Memory: `main` → `feature/logging-v1`, EDP-Threading: `main` → `feature/logging-v1` |
| `EDP-Security` | EDP-Memory: `main` → `feature/logging-v1` |
| `EDP-Serialisation` | EDP-Memory: `main` → `feature/logging-v1`, EDP-Localisation: `main` → `feature/logging-v1` |
| `EDP-State` | EDP-Primitives: `main` → `serialisation_prerequisites`, EDP-Clock: `main` → `mesh_v1`, EDP-Memory: `main` → `feature/logging-v1`, EDP-Threading: `main` → `feature/logging-v1` |
| `EDP-Threading` | EDP-Clock: `main` → `mesh_v1`, EDP-Memory: `main` → `feature/logging-v1` |

**Executable representative compatibility, not inferred:** RPI400 isolated pinned-source GCC C++20 tests executed both profiles independently.

- **Declared main-pinned profile:** detached `20261102T172609-bb90efdd` **PASS exit 0**. `EDP-Serialisation/feature/logging-v1` `4a2f1dea` with `EDP-Memory/main` `5ee3b20`, `EDP-Localisation/main` `2829791`, `EDP-Clock/main` `dbc8e8c`, plus main System, Platform, BoundedTypes, BoundedTopology and Portable: strict UTF-8, heterogeneous CBOR/JSON, strong identity adapters and expected compile-fail regressions PASS. `EDP-Threading/feature/logging-v1` `269d3dbc` with those same main dependencies: foundation, ordinary mutex and fail-closed mutex GCC suites PASS.
- **Latest-owner feature profile:** detached `20261102T172741-d84bb408` **PASS exit 0**. Same owner source, except `EDP-Memory/feature/logging-v1` `8563161`, `EDP-Localisation/feature/logging-v1` `8b826f4`, `EDP-Clock/mesh_v1` `ad45b912`; exactly the same Serialisation and Threading GCC suites PASS.

**Limit:** These are selected owner-level GCC host test entrypoints, not an Arduino/ESP-IDF application build of every optional header, a multi-library integration run under the automatic PlatformIO dependency resolver, any physical HIL evidence, a Clang/sanitizer full matrix for this tranche, or a guarantee that branch pins can safely be rewritten. In particular, `EDP-Serial` frame-pump imports Logging/Threading and `EDP-Localisation` file-pack imports Persistence are conditional owners, intentionally not mandatory imports of their minimal umbrella. Explicitly preserve their application-level direct dependency obligations.

Future G14 scope: validate specific representative native/Arduino build compositions against both declared manifests and feature-pinned working branches, audit source/Wiki semantic contracts and direct dependency declarations, then determine whether a manifest migration is justified by *actual* consumer compatibility evidence. No production C++ or manifest changed in this tranche, all versions `0.1.0`.


## 2 November 2026 — current-source native and Arduino selected application builds

The **19-owner SHA-pinned working snapshot** for this integration build is preserved at the authorised RPI400 in `/srv/espressio/scratch/gpt6-g14-cont-20261102-c71a/checkout-pins.json`. It consists of the current 2 November published owner working branches, with no transitive branch resolution silently selected by PlatformIO; the projects expose their actual bounded owner includes and application Bootstrap providers. The separate `lib_ldf_mode=chain+` Arduino demos resolve their ESP32 platform libraries (notably FS and LittleFS) while consuming the checked-out EDP source directly. These executions **validate the selected profiles only**, not the default `library.json` manifest resolution of every possible feature, arbitrary consumer version closure, physical transmission, or all SDK choices.

| Application / profile | 2 Nov RPI400 detached build | Result | Linker RAM | Program Flash |
|---|---|---|---:|---:|
| G7 15-field Localisation/Serialisation CBOR + strict NVS-ACK, immutable LittleFS EDLS (`esp32dev-typed-strict`, ESP-IDF 5.5.5) | `20261102T173948-b50ec8a2` | **PASS exit 0** | 18,096 B | 256,113 B |
| G1 full native composition including EDP-Serial frame FIFO + ESP-IDF UART concrete, Memory, Threading, generated root (`esp32dev`, ESP-IDF 5.5.5) | `20261102T174658-d119edbc` | **PASS exit 0** | 28,760 B | 405,273 B |
| Finite ConfigurableBoundedEdlsBatch PlatformIO Arduino-ESP32 3.3.12 | `20261102T174928-fc9b3c9a` | **PASS exit 0** | 22,956 B | 330,920 B |
| TypedManagedBatchWorker PlatformIO Arduino-ESP32 3.3.12 (Memory/Threading, generated CBOR, durable sink) | `20261102T175144-a2c5f0f0` | **PASS exit 0** | 48,020 B | 378,660 B |

The generated Localisation fixture was produced by the owning EDP-Localisation toolchain, not handwritten or permanently committed. All four combinations built and linked actual factory firmware images on RPI400; **none were flashed or executed on physical hardware in this tranche**. Prior approved WROOM electrical PowerLoss/throughput receipts remain a separate, earlier source-specific category of evidence. Arduino Serial output integration is **not** inferred from an Arduino Logging/LittleFS build: `EDP-Serial-Arduino` is a separate unfinished concrete owner.

The current-source Wiki audit (2 November, detached `20261102T174300-2080a14c`) discovered 42 production source units with matching indexed references and 62 local Wiki pages, and reported **zero** broken local Wiki/docs links or missing reference source traceability after the corrected `LogRecord` semantic opening and generated wire projection source reference. This provides source/page/navigation consistency; exhaustive declaration-by-declaration type/member/method prose assessment remains a separate deeper V2 gate.

## 2 November 2026 — current-owner V2 source spacing normalization and test limits

Seventeen existing Logging implementation headers carried confirmed V2 Code Convention spacing and namespace-scope formatting problems (including 48 control statements without a space before their argument list). The proposed source-only normalization was performed in a separate immutable baseline clone under `v2-style-review`, without new public symbols, version changes or deliberate control-flow changes. A private source-level lexical proof comparing comment-free C++ tokens for all 17 touched files passed `20261102T175604-ffe11ea0` (**PASS, zero differences**). The first automated lexer probe `20261102T175541-1d6a4c70` gave one **false positive** due to its treatment of `TSlots-1U` as a fused signed numeric token; explicit sign normalization resolved the *tooling* discrepancy, not a production bug. GCC C++20 foundation, GCC UBSan and Clang native strict-NVS-ACKed generated Memory/Threading Task host suites passed detached `20261102T175641-4e198416` (**PASS exit0**). The separate clean ESP-IDF 5.5.5 G7 strict typed/NVS-ACKed firmware rebuild of the formatted 17-header source **PASSED exit 0** on RPI400 detached `20261102T175916-090dba97` (7 min 5 sec, complete factory image). The separately rebuilt factory image SHA **differs** from the earlier exact-source firmware; builds under independent work directories are not claimed byte-identical or automatically physical equivalent. The complete ordinary GCC/Clang host-script matrix of the formatted source **PASSED 68/68, zero failures**, detached `20261102T180310-df949ec8`. Two explicitly independent target-only scripts remained excluded from this host count and have their own target receipts. A final three-file **namespace comment-only** correction was then applied to issue-bearing headers `DiagnosticArgumentEncoding.hpp`, `OwnedDiagnosticArguments.hpp` and `LogRecordApprovedFieldsSerialisation.hpp`, completing the Rule1b namespace marker sweep; the complete source now has **20 changed C++ headers**, with **zero non-comment C++ token changes** (detached `20261102T181642-d3876923` PASS exit0) and zero open namespace-closing comment candidates after normalising fully qualified namespace comments. G4/G3 targeted rechecks after this final comment-only addition are recorded separately. These are concrete V2 spacing/namespace corrections, *not* proof that every remaining V2 method/member documentation nuance or unrelated repository is globally certified. Code style cleanup alone does not prove every member/method has complete V2 documentation; the exhaustive Rule 3–5/9 review remains open.

**G14 remaining:** target-compatible selected provider completion, source method/comment policy assessment, and explicit separation of Logging-core completion from global 37-repo V2 governance and optional Serial-Arduino provider selection. All library versions remain `0.1.0`.


## 2 November 2026 — typed diagnostic schema generator branch qualification

A genuine **negative tooling compatibility** test was observed on 2 November, detached RPI400 `20261102T181046-c14c3719` **FAIL exit 1** before any native compilation. The G7 generated 15-field typed logging firmware fixture deliberately selected `EDP-Memory/main`, `EDP-Clock/main`, and `EDP-Localisation/main` while keeping the current Logging and Serialisation feature source. The older Localisation catalogue generator rejected its diagnostic `parameters` property with `ToolError: ... unknown properties: parameters`. Independently inspected `EDP-Localisation/main` owner generator `catalogue.py` permits only `{id,symbol,status,localisation,context,notes}` at that location; feature/logging-v1 explicitly permits and parses `parameters`. This is **not** a firmware compile failure, runtime persistence problem, or evidence that `EDP-Memory/main` or `EDP-Clock/main` are invalid.

The generated typed diagnostic-schema application **requires the newer EDP-Localisation generator and current EDP-Clock ClockEra contract** as explicit conditional dependencies until that generator API is promoted or an explicitly specified new version/profile is published. The base manifest for EDP-Serialisation/feature/logging-v1 currently points to `EDP-Localisation#main`; that transitive pin alone cannot build the optional generated diagnostic schema from source. An explicit application-selected owner generator version/branch is mandatory for this profile. Do not silently rewrite all consumers' manifest pins without downstream compatibility qualification. The second mixed-profile native build `20261102T181126-798c1f8e` (feature Localisation, but `EDP-Clock/main` and `EDP-Memory/main`) **FAILED exit 1** in actual ESP-IDF C++ compilation: the optional EDP-Serialisation `ClockEraAdapters.hpp` imports `clock/ClockEra.hpp`, which does not exist in EDP-Clock/main. The current `EDP-Clock/mesh_v1` owner supplies that header. Again this is a **source profile compatibility failure**, not a runtime persistence result. The third, narrowed profile with **feature Localisation + Clock but `EDP-Memory/main`** successfully compiled and linked the real strict 15-field ESP-IDF firmware: detached `20261102T181510-583eeea8` **PASS exit 0**, 18,096 B linked RAM / 256,113 B program Flash. Thus `EDP-Memory/main` satisfies this exact selected application build; the previous two failures cannot be attributed to Memory. A further **incremental forced C++ source recompile** tested actual `EDP-Localisation/main` runtime headers against diagnostic schema files generated earlier with `EDP-Localisation/feature/logging-v1`; the unmodified generated headers and `Clock/mesh_v1`/`Memory/main` profile compiled PASS, detached `20261102T182433-cb3220f7` **exit 0**, with its prior `main.cpp.o` preserved aside to force a real `src/main.cpp` compiler invocation. The preliminary `pio run` `20261102T182341-4bbd009b` also passed but **did not actually recompile main.cpp**, so is not used as independent C++ compatibility evidence. The runtime header profile is compatible in this focused target test, whereas the *generator* cannot be substituted with the earlier main branch. The actual runtime-header compatibility test is independently recorded above, rather than inferred from generator success or the earlier cached incremental build. This does not certify every generated-schema consumer in all SDK targets.


**Owner-governed decision:** [EDP-Serialisation issue #11](https://github.com/ESPressio-Development-Platform/EDP-Serialisation/issues/11) tracks whether to retain explicit consuming-application optional generator/ClockEra pins, change feature-branch transitive dependency manifests after cross-consumer tests, or promote required owners to governed stable branches. No automatic pin migration or unrelated consumer compatibility guarantee is implied.


## 3 November 2026 — representative full application and V2 source profile

The 2 November source/manifest-pin matrix remains valid as a branch-tip **discovery**, not an automatic dependency override. Fresh 3 November RPI400 builds against the verified exact 19-owner newest-source snapshot passed: G1 whole firmware with EDP-Serial/ESP-IDF UART provider `20261102T174658-d119edbc`, G7 real 15-field typed/strict NVS-ACK LittleFS firmware `20261102T173948-b50ec8a2`, and all six Arduino IDE / PlatformIO Arduino / PlatformIO ESP-IDF demo targets as listed in [G1–G15 acceptance](ACCEPTANCE-G1-G15.MD#3-november-2026--g14-target-compatibility-lexical-v2-cleanup-and-unresolved-boundary). The tested source is selected from the explicit verified checkout pins, **not** a claim that PlatformIO's `library.json` dependency resolver always selects those feature heads.

The subsequent isolated V2 source lexical/formatting correction preserves all 35 affected headers' exact Clang token streams and passed GCC 34/34 host scripts and ESP-IDF G7 target compile (receipts in [acceptance](ACCEPTANCE-G1-G15.MD)); the concrete owner requirements for optional file, Clock, Threading, Memory, Serial and generated code are unchanged. This qualified profile does not automatically cover arbitrary application Bootstrap providers, extra peripherals, hardware Serial capture, every transitive SDK pin, flash wear, or every source member's Wiki description. Any future direct dependency migration must be demonstrated against its *actual* consumer/target build rather than justified solely by the most recent branch-tip date.

**Historical 2 November negative branch probes preserved:** at the specific *then-current* main SHAs, Localisation's generator rejected `parameters` and Clock/main lacked `ClockEra.hpp`. These negative receipts are valid for their dated source profiles but **are not statements about the current 9 November main branches**. See the current-main positive qualification and remaining narrow limits below.

## 9 November 2026 — promoted-main generator and ClockEra dependency requalification

The human-approved 3–4 November main reintegration promoted the required optional source contracts without rewriting application manifests: `EDP-Localisation/main` **`8b826f4c9becfa34eb3a885361f70033f8ff3f22`**, `EDP-Clock/main` **`9a27b12d5938dbd1cb26af541a489531a848ba25`** and `EDP-Serialisation/main` **`f36e615720ce093a03482ae710afe129d031a551`**, all still `0.1.0`. Verified live GitHub main: Localisation's `catalogue.py` now recognises the typed `parameters` grammar, Clock exposes `src/clock/ClockEra.hpp`, and Serialisation `library.json` names Localisation `#main`.

The **exact published Logging feature source tree** `6060b3cf9705edc199f396aa415f2bf9a1fdabf4` (GitHub `feature/logging-v1` `a759d282f8e8495361c6e3b451a329a2568bae41`, identical to independently tested staged tree) was compiled/tested on RPI400 with the three current promoted main SHAs. Detached **`20261109T090236-f9d01ff1` PASS exit0, 8/8 actual checks**: owner-generated typed diagnostic schema (including ClockEra and RuntimeIncarnation), GCC/Clang ClockEra positive and deliberately failing wrong-kind compile-negative, GCC/Clang generated canonical CBOR-to-immutable EDLS, GCC/Clang EDP-Memory + EDP-Threading Task/strict NVS-ACKed archive integration, and historical Localisation schema Python tests. Raw per-case receipts: `/srv/espressio/scratch/gpt6-logging-recovery-20261109-0846-r7q2/receipts/main-owner-compat.tsv`. A second read-only reference audit `20261109T090831-20ed9df2` **PASS exit0**: 42 production source units, 62 Wiki pages, zero broken/missing local source/document references. This is navigation/link validation, **not complete per-member semantic V2 documentation**.

**Current interpretation:** the previous specific default-main generator/header blockers are **resolved for the actually tested dependency closure**. Do **not** claim every PlatformIO auto-resolved package graph, optional Bootstrap mix, or ESP-IDF firmware target has been requalified solely by these host tests. See [EDP-Serialisation #11](https://github.com/ESPressio-Development-Platform/EDP-Serialisation/issues/11) and [9 Nov Policy recovery](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-09/2026-11-09-08-46-EDP-Logging-Recovery-And-Reintegration.MD). Do not alter consumer manifests or delete retained source branches as a consequence of this finding.


## Current published owner reconciliation — 10 November 2026

This section **supersedes old “feature must still merge” assertions only for the repos listed here**; the dated 2 November audit above remains historical provenance. The following GitHub branch heads were independently checked during the dependency-first reintegration campaign:

| Actual library owner | Published active branch | Exact reviewed commit | Qualification / limitation |
| --- | --- | --- | --- |
| EDP-Localisation | `main` | `8b826f4c9becfa34eb3a885361f70033f8ff3f22` | Current generated diagnostic `parameters` contract, host/GCC/Clang source-owner integration |
| EDP-Clock | `main` | `9a27b12d5938dbd1cb26af541a489531a848ba25` | Canonical ClockEra, native input contract for optional record fields; not a general Mesh time sync guarantee |
| EDP-Serialisation | `main` | `f36e615720ce093a03482ae710afe129d031a551` | Generated CBOR/JSON adapters and strict typed wire, source-breaking predicate migrations applied |
| EDP-Platform-Portable | `main` | `267411bf319a269883d2273928ad19fc05e2f324` | Narrow raw resource/byte-copy roots, GCC/Clang Debug/Release 16/16; 12 relevant native demo targets, **not** blanket Portable atomic/exception provider qualification |
| EDP-Memory | `main` | `9c5a994eb1e101b80a6fb0e1357c0add0e3a38c4` | NoWait pool coordination + 5 three-target demo families. Direct finite/Forever waiter under provider failure **not safe-qualified**, [Memory #16](https://github.com/ESPressio-Development-Platform/EDP-Memory/issues/16) remains open |
| EDP-Threading | `feature/logging-v1` (at time of this entry) | `1c5ff530c7051d0ccb9480c9a46fbb3cd9424750` | Kept separate until post-Memory main hosted and native regression; `main` not yet promoted |
| EDP-Logging | `feature/logging-v1` (at time of this entry) | `8d2b307a8a1db1aa5596abc904cd0da1f3c032cd` | Pending final G1–G15, public demo/docs/Wiki and downstream post-main closure |

**Evidence:** The original 9 November [Policy living handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-09/2026-11-09-09-29-EDP-Logging-Dependency-First-Reintegration.MD) contains raw detached RPI400 suite IDs including independent **16/16 native Memory/Logging demo** PASS `20261109T114835-9efcac49`, per-repo SHA guards and previous real physical WROOM cut/wear limitations. The 10 November follow-up is independently rebuilding post-Memory-main consumer host/native code; its result must be checked before this entry is interpreted as release acceptance. `EDP-Serial` and `EDP-Serial-ESP-IDF` full source remain on their feature branches and are downstream optional consumers, **not** dependencies of Logging.
