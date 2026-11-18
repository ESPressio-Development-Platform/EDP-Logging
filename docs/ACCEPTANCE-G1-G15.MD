# EDP-Logging G1–G15 acceptance and V2 owner review

**Status:** Logging and its Serial companions are integrated on main. The 18 November evidence below records the latest source and validation. The 13 November checkpoint records the prior ByteOperations and UART qualification. The earlier G14 feature audits remain dated evidence. All versions remain `0.1.0`. This is a **requirement-to-evidence reconciliation**, not certification that all gates or V2 requirements are complete. The original gate definitions and locked D1–D13 design are in the [15 October living architecture handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-10-15/2026-10-15-11-46-EDP-Logging-Architecture-Living-Handoff.MD). Their 2026-10-15 blocker labels are historical: later source and evidence supersede them. The current [qualification ledger](LOGGING-QUALIFICATION-STATUS.MD) and [immutable capacity contract](IMMUTABLE-PERSISTENCE-BYTE-CAPACITY.MD) provide concrete test receipts.

For the inspected newest working branches, optional provider activation and manifest-pin differences, see [G14 conditional owner and transitive baseline](DEPENDENCY-OWNER-BASELINE.MD). It now includes a 37-repository source/manifest inventory, 12 dependency-branch-pin comparisons, representative owner contract tests and four independently successful current-source ESP-IDF/Arduino builds; these do not certify all provider combinations or all 37 repositories for every V2 source convention.

## Reconciled original gates

| Gate | Original acceptance concern | Current scoped source/evidence | Unresolved or deliberately excluded |
| --- | --- | --- | --- |
| G1 | End-to-end bounded Memory admission and safe cancellation | Existing Logging queue, tracked lease owner, AdmissionLifecycleCoordinator, bounded producer and dedicated Threading coordinator, including newer explicit WaitUntil/atomic provider interfaces, pass relevant GCC/Clang integration, native worker and finite drain tests | Direct native EDP-Memory finite waits are **not** assumed safe solely because the composed NoWait/wake route works. Exact application/task/provider closure requires its own contract and test topology |
| G2 | One coherent canonical Clock + synchronized projection, ClockEra and uncertainty | Canonical EDP-Clock monotonic coordinates and optional coherent temporal transfer/qualification are present and tested; native managed fixture uses the canonical monotonic provider | Do not imply every Bootstrap synchronized Clock+Era implementation supplies atomic, correctly correlated acquisition; G2 is only qualified for specifically tested providers |
| G3 | Governing heterogeneous generated diagnostic arguments and portable codecs | Generated Localisation schema and owned argument source, typed CBOR writer and all-15-field native managed worker/three-target examples have finite test evidence | Do not infer arbitrary custom kinds, unissued wire tags or schema provenance not actually validated by the selected generator/catalogue |
| G4 | Governed LogRecord root TypeIdentifier and stable FieldIds | Root identity was issued by EDP-System; 15-field typed record/wire source and generated integration are present, with malformed/golden tests | Retain exact governance, compile-time schema/strong typed discrimination and record-level validation, not generic object-memory serialization |
| G5 | Diagnostic schema fingerprint, translations and trustworthy historical artefacts | EDP-Localisation-generated signatures and independently pinned schema-file SHA tests exist; runtime typed arguments have proven native samples | Schema fingerprints alone do not reconstruct original text or ContractFamilyFingerprint. Exact historic wording requires separate trustworthy deployment/pack provenance |
| G6 | Exact EDLS/EDLG framing and bounded scan | Canonical headers, frame CRCs, sealed EDLC whole-segment count/length/CRC, candidate scan and strict invalid-tail rejection tested under host/native profile | No inference that the underlying filesystem provides arbitrary interruption-atomic file replacement |
| G7 | Qualified persistence provider and physical PowerLoss preservation | NVS atomic acknowledgement with independently checked immutable LittleFS segments, replay and GPIO17 external power-cut profiles; protected history is not retired before successor verified/ACKed | Generic LittleFS VFS FileStorage remains Restart retention / None interruption atomicity. Arbitrary exact Flash write/GC interruption and broad device lifetime are **not** established |
| G8 | Canonical reversible strong-value CBOR/JSON representation | Core-field adapters, generated CBOR, JSON scalar, malformed message/ticket/enum tests and raw-media original-field comparison are present | Only the exact supported schema/kinds/providers tested are covered; no private native structure may be treated as a public wire representation |
| G9 | Historical diagnostic presentation provenance | Host tooling uses independently pinned diagnostic schema and ContractFamily evidence. Binary stdin supports an external capture process and independent raw evidence file. | The capture process configures and owns the serial endpoint. Historic wording is available only with trusted artefact provenance. |
| G10 | Concurrent producers, bounded FIFO admission and sink/reclaim outcomes | 30,000 scheduled sources over 300s at 100/s plus phase records were retired/durable on a named 160MHz ESP32 managed worker run; fail/pressure/backpressure and exact queue tests have receipts | Five-minute finite-profile proof is **not** indefinite throughput, every hardware configuration, every asynchronous sink or unbounded fairness guarantee |
| G11 | Actual constrained RAM/stack/latency/flash/resource costs | WROOM-class finite native profile logs bounded buffers, min-heap, task stack headroom, ACK latency and partition outcomes | PICO-V3-02, all release configurations, allocation amplification, flash wear and lifespan remain separate. No 2-year assurance required or authorised |
| G12 | Owned Serial domain, concrete providers and integration | EDP-Serial and EDP-Serial-ESP-IDF are integrated on `main`. Both passed independent post-main tests. Physical WROOM G1 passed 20/20 with independently captured CRC-valid EDLG on UART0. See [Serial integration contract](SERIAL-OUTPUT-INTEGRATION.MD) | Generic Serial-Arduino and USB/CDC are deferred. Bootstrap must ensure exclusive ownership of the selected UART. |
| G13 | Optional raw LogRecord wire-output mode | Deliberately **excluded** by D1–D13 architecture: portable CBOR/JSON is the defined representation; no dump of native object memory | Only a new explicit specification could authorise a portable raw format; its absence is not a release defect |
| G14 | Correct branches, repo topology and dependency pins | The main integration is complete for the selected owners and Serial companion profiles. The current workflow refs and [owner baseline](DEPENDENCY-OWNER-BASELINE.MD) record the validated topology. The earlier 37-repository inventory remains dated evidence. | Full cross-repository V2 semantics and untested provider mixes remain outside this qualification. |
| G15 | Authorised executable appliance and physical proof | RPI400 explicitly available, lock controlled, detached GCC/Clang/native and independent GPIO17 physical evidence present in retained run histories | An appliance connection alone does not prove data-contract PowerLoss behavior. Keep reset reasons, controller proof, per-record CRCs and raw-media for each claimed cut |

## Existing measured application profile

The five-minute native offered-load sample used 30,000 scheduled records at 100/s, with 30,129 accepted phase records durably completed and a finite scheduled drain of 300.949900 s. The last 6,000 ACK latencies are a sample, **not** all-run percentiles. The subsequent immutable-byte-capacity configuration used 28,000 sink-owned logical bytes; the measured peak was 27,088 bytes, all 2,129 admitted records completed, and independent LittleFS/NVS/original-field audit passed. These are separate finite runs and configurations, not a single combined indefinite production guarantee.

Application Bootstrap chooses persistent storage and an immutable byte capacity. The budget includes the successor reservation and fixed ACK representation but **not** provider-internal filesystem allocation/GC/NVS overhead. Production deployments with substantial logging should preferably use separate storage rather than ESP-integrated Flash. There is no operational-lifespan or flash-endurance assurance and no platform-wide prescribed retention amount.

## Affected-repository ownership and V2 audit as of 1 November

| Owner / selected role | Required boundary | Audit position on 1 November |
| --- | --- | --- |
| EDP-Logging / feature/logging-v1 | Diagnostic record, queue, worker integration, strict EDLS transaction, docs and tests | Predicate Rule19b migration, selected typed-result Rule19 and all observed enum Rule2 corrections published; **remaining whole-source convention/documentation/reuse audit open** |
| EDP-System / main | Issued type identity and compile-time Composition | Current Composition framework read; keep owner-issued 15-field root identity, no local replacement |
| EDP-Memory / feature/logging-v1 | ObjectPool leases and ByteOperations | Existing `CopyBytes` contract now reused by stateless provider in optional source capture and rehydration; no duplicate generic copy, no new API; opt-in provider is compile-time composed |
| EDP-BoundedTypes / main | Bounded bytes and string ownership | Existing bounded types and ownership checked; do not reproduce their capacity semantics |
| EDP-Clock / mesh_v1 | Canonical monotonic time, optional synchronized quality/era | Reuse monotonic coordinate; no local alternate timeline |
| EDP-Threading / feature/logging-v1 | Ordinary mutex, wait/deadline, Task/coordinator | Use existing coordinator and canonical deadline, no parallel task/timer/queue |
| EDP-Localisation / feature/logging-v1 | Generated diagnostic schema and historical text provenance | Generated type/count/fingerprint source used; dirty managed checkout untouched and historic exact wording not inferred |
| EDP-Serialisation / feature/logging-v1 | Canonical CBOR/JSON and UTF-8 validation | Strict canonical validator now extracted/reused with optional Logging capture; both branches verified, original codec acceptance unchanged |
| EDP-Persistence / main | FileStorage and KeyValueStorage offer profiles | No generic interruption-atomic upgrade. All Storage classification remains provider-specific |
| EDP-Persistence-ESP-IDF / main | LittleFS VFS + NVS concrete binding | Independent actual cut/ack/raw-media evidence preserved; no autoformat or unknown durability guarantee |
| EDP-Serial / feature/logging-v1 and EDP-Serial-ESP-IDF / feature/logging-v1 | Optional complete EDLG frame pump, bounded frame FIFO and ESP-IDF byte provider | [Current G12 provider ownership/qualification](SERIAL-OUTPUT-INTEGRATION.MD); Arduino provider and base-branch promotion remain open |
| EDP-Platform and platform concretes / respective heads | Explicit concurrency, atomic and native SDK boundary | Reuse approved provider abstractions, retain SDK confinement and explicit provider selection |
| Other touched dependency/identity owners | Correct ownership, dependency graph, Wiki, CI, code convention | Full repository-by-repository certification **not yet performed**; GitHub history and pinned test dependencies must be reconciled first |

## 1 November 2026 strict UTF-8 consumer reconciliation

Following explicit user approval and an all-37-repository consumer audit, optional OwnedDiagnosticArguments::AddUtf8 delegates to the sole strict EDP-Serialisation validator and rejects embedded NUL; AddBytes remains unchanged for binary input where generated schema permits it. The Serialisation default JSON/CBOR text domain, error offsets, canonical encodings, and all versions are unchanged. Cross-library changed-source GCC/Clang and target evidence must be checked on the exact commits before this is considered complete. The original human decision below is historical and resolved for this specific change.

## 1 November 2026 V2 Rule19 and Rule19b public/Detail corrections

As explicitly authorised for source-breaking V2 compliance, Logging has updated the remaining discovered Boolean predicate and compile-time generated-schema names to begin with `Is` (including historical EDLS predecessor validity, key identity, transactional ticket/phase state and generated diagnostic schema qualification). Source, tests and applicable Wiki references were migrated together; no compatibility aliases retain the former unprefixed spellings. `GeneratedLogRecordWireProjection` now returns its new dedicated `GeneratedValueConversionResult` (`Succeeded=0`, `OwnerRejected=1`) instead of using a Boolean to represent the fallible strong-Type adapter; the calling encoder requires the explicit successful status. Negative tests and external source searches are recorded in the Policy living handoff. These source corrections do not grant new persistence durability or runtime physical-test claims; acceptance remains scoped to the verified builds and profiles.

## 1 November 2026 historical physical acceptance checkpoint

The complete post-V2 GCC/Clang matrix (68/68), 178 Python offline tests, Xtensa compile, governed generated-schema wire and whole-application G11 off/on linked comparison have passed. A fresh near-capacity GPIO17 power-cut test passed with 8.619s OFF / POR confirmed and independent raw-media/NVS original 15-field validation of 128 committed records. **However, final 30,000-record 100 rps physical workload `20261101T143310-918c289b` failed** at ~13,000 published sources because three 100 ms producer queue-admission deadlines elapsed (`AdmissionFailed/DeadlineElapsed/CapacityUnavailable`, Memory not attempted). Logging remains **not production ready** until the long-tail queue/worker/storage service source is measured, corrected/qualified, a subsequent exact five-minute physical run and raw LittleFS/NVS audit pass, and the original device image restoration and Policy V2 evidence closure are verified. See [qualification ledger](LOGGING-QUALIFICATION-STATUS.MD). Do not call a timeout a corrupted durable acknowledgment or accept a shortened workload as five-minute proof.

## Historical 1 November acceptance and decision points

- **V2 component reuse/UTF-8 (resolved):** user-approved strict EDP-Serialisation validator is reused in optional Logging source capture. Embedded U+0000 is rejected by AddUtf8 and remains available as generated-schema-qualified binary using AddBytes; both source changes and all cross-library GCC/Clang and two ESP32 firmware builds passed on 1 November. Historical alternative permissive policy is NOT in force.
- **V2 Memory operations (1 November resolution):** the private OwnedDiagnosticArguments Store and owned scalar/identity rehydration loops have been replaced with the existing stateless EDP-Memory ByteOperations CopyBytes provider contract; no Memory API extension or retained provider state. Mutating owned capture is runtime-only instead of previously constexpr, preserving all typed results/capacity, with explicit application-selectable stateless provider and portable default. Independent native GCC/Clang, substitution/negative and target checks remain in the qualification ledger; this resolves the earlier duplicated copy loop design concern.
- **Documentation and policy:** Current-tense descriptions in the root README, older architecture and coordinator documents, dependency graph, Wiki and the large historical Component Reuse catalogue need further factual reconciliation. The dated historical reports and negative test receipts must be retained.
- **Acceptance status:** Issues #1, #12 and #15 remain open. No universal production certification, indefinite throughput, generic FileStorage PowerLoss or two-year flash-endurance assurance is claimed.

All validated compiler/test/source commits and the latest branch/ref inventory are checkpointed in the [Policy Logging living handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-10-29/2026-10-29-10-36-EDP-Logging-PostRetirement-Scan-Qualification.MD). This matrix must evolve whenever actual code, qualification or public contract changes.


## 2 November 2026 — physical G10/G11 evidence supersession and remaining release work

The historical 1 November negative five-minute checkpoint was accurate for the earlier source. However, two later current-source full 100 records/sec WROOM physical 30,000-record runs 20261101T193528-695b02cf and 20261101T194232-bc98b8ae PASS with all 30,129 source/idle records retired and durable, zero unexpected errors, explicit 252-slot/254-object bounded topology, minimum free heap 134060/134084 bytes and genuine, unmodified-UART independent LittleFS/NVS 125 retained original records checked across all 15 fields, ACK472 and verified NVS page/entry/blob CRCs. They directly support scoped G10/G11 and the ordinary durability part of G7/G15 on this selected WROOM/Flash configuration; they do NOT establish indefinite no-drop, lifespan assurance, generic FileStorage power atomicity or arbitrary application capacities. Do not erase prior failed run receipts.

Final release gates still require publishing the exact tested source and HIL fixtures, fresh post-change controlled GPIO17 cut qualification, final current-source compiler/host regression, V2 ownership/docs reconciliation, and independent restoration of the original test appliance Flash image. G12 optional Arduino Serial provider remains a separate owner/stub scope, not a reason for Logging to duplicate its component; G13 portable raw-object dump remains explicitly excluded. Version 0.1.0 remains unchanged.


## 2 November 2026 — verified optimized PowerLoss, host and strict raw-media acceptance

The published feature/logging-v1 code at 9fd4956a195d83e15cc53dad08d7743688a59833 passed two genuine full 100 records/sec physical WROOM tests: 20261101T193528-695b02cf and 20261101T194232-bc98b8ae, each with 30,129 accepted, retired and durable records, 125 independent retained originals checked across all 15 fields and strict LittleFS/NVS ACK472 with page/entry/blob CRCs. Heap and stack remained finite with minimum free heap 134060/134084 bytes. The same current source passed 66 GCC/Clang host script invocations 20261102T083638-7e077e59 and 137 Python offline tests 20261102T084111-ff0df6ef. Six specialized firmware/xtensa-specific scripts were not part of the 66.

Actual optimized PowerLoss physical capacity-cut firmware built PASS 20261102T083816-1d0455ef. The first two attempts 20261102T084902-173395cc and 20261102T085423-20f22423 FAILED before relay switching, each reaching first 64 acknowledged sources but timing out the second half of initial 128-record pressure drain; preserve these real negative receipts. Test-only read-only pressure-drain instrumentation was added without changing the strict finite 15sec gate, source code or coordinator scheduling. The subsequent physical GPIO17 test 20261102T090006-e5a4b0db PASS exit0: durable ACK2, 128 protected records, safe near-full storage (pending 52, native replace status12, free 4096 bytes), genuine 8.260sec OFF, independently observed ROM PowerOnReset and app reset reason1, contiguous protected generations1,2, and post-recovery all15-field verification.

The first offline raw-media audit wrapper 20261102T094814-92f69ff8 exited1 after creating the real raw LittleFS/NVS images and extracted files, but the actual constituent integrity audits were each separately verified PASS. A consolidated, independently detached rerun 20261102T094916-cd7e45da PASSED exit0, verifies original raw-image SHA digests, 128 actual original 15-field records and genuine NVS namespace batchack generation2 page CRC, entry CRC, blob CRC. Additional independent LittleFS extraction 20261102T094958-b9fe17ac PASS and byte-identical retained slot0 and slot1 to first extraction. Raw Flash images and original UART remain unchanged. This is a real relay PowerLoss result, not a reset imitation.

**Qualification scope:** Finite bounded structured Logging with application-selected storage media, queue capacity and immutable logical byte limit is production-capable for the specifically qualified ESP32/strict LittleFS+NVS profile. It does NOT guarantee global/release-wide G1–G15 closure for all providers, arbitrary FileStorage PowerLoss atomicity, indefinite 100rps operation, all Serial options, Flash wear/lifespan, or all Clock providers. Final V2 owner/documentation compliance and any non-core optional integration remain separate review obligations. All versions remain 0.1.0.


## 2 November 2026 — G14 all-owner source inventory checkpoint

The [current 37-repository G14 branch-tip and source-import audit](DEPENDENCY-OWNER-BASELINE.MD#2-november-2026--refreshed-37-repository-g14-source-audit) detected no include-level repository cycle, and did not find any direct source import absent from both its owner's README and dependency graph. Existing EDP-Memory/Threading normative README omissions were corrected in committed owner heads 85631612175f and 269d3dbc9b21 with independent link/diff QA PASS. Mesh-related explicit-enum formatting defects were inspected and tracked in Mesh #34 and Platform-ESP-IDF #13 without changing Mesh branches. This does NOT complete all optional provider and transitive pin build matrices, every V2 source convention or Wiki symbol qualification. Logging #15 remains open; all versions remain 0.1.0, and previous finite WROOM/POR evidence is unchanged.


## 2 November 2026 — four selected current-source native/Arduino application builds and Wiki source contract

The [current G14 full owner baseline](DEPENDENCY-OWNER-BASELINE.MD#2-november-2026--current-source-native-and-arduino-selected-application-builds) records all four fresh factory-image build receipts on the exact selected 19-repository current source: G7 strict 15-field NVS ACK/EDLS ESP-IDF **20261102T173948-b50ec8a2 PASS**, G1 whole native ESP-IDF/Serial UART integration **20261102T174658-d119edbc PASS**, configurable finite batch PlatformIO/Arduino **20261102T174928-fc9b3c9a PASS**, and typed Memory/Threading worker PlatformIO/Arduino **20261102T175144-a2c5f0f0 PASS**. No new physical test is implied. The Wiki source navigation (42 production units, 62 pages) and local links passed **20261102T174300-2080a14c** after correcting stale LogRecord Field11/14/current generated full envelope wording. Versions 0.1.0, G12 Arduino Serial provider still deferred, and global V2/SDK/provider certification not claimed.

Twenty Logging C++ headers were adjusted for confirmed spacing/namespace-documentation requirements only: the initial 17-header mechanical style repair and three final comment-only namespace closure repairs. C++ noncomment lexical-token identity for all 20 passed `20261102T181642-d3876923`, all 68 ordinary GCC/Clang host scripts passed `20261102T180310-df949ec8`, and the independent formatted-source G7 ESP-IDF firmware built a factory image successfully `20261102T175916-090dba97`. These are targeted V2 corrections, **not** an assertion of all remaining method/member V2 text-policy compliance or a fresh physical PowerLoss execution. Publication is reported only after a remote source-tree readback verifies the tested commit. The 37-repo scan is inventory plus selected tests, not a declaration that all other owners' source lines have been checked individually.


## 2 November 2026 — precise G14 conditional branch compatibility boundary

Full typed-Logging build failure reproduction narrowed the required owner branches without rewriting global dependency manifests. `EDP-Localisation/main` generator fails to parse generated diagnostic `parameters` (**20261102T181046-c14c3719 FAIL before native compilation**), while `EDP-Clock/main` lacks the `ClockEra.hpp` public header required by the opt-in Serialisation ClockEra adapter (**20261102T181126-798c1f8e FAIL at native compile**). `EDP-Memory/main` is independently compatible when the current Localisation generator and `Clock/mesh_v1` are used (**20261102T181510-583eeea8 PASS full ESP-IDF firmware**). Previously generated descriptor headers from Localisation/feature/logging-v1 can also compile with the Localisation/main runtime headers in that exact profile when `src/main.cpp` is forcibly rebuilt (**20261102T182433-cb3220f7 PASS**); the older generator, however, cannot itself regenerate the source schema.

Therefore a consuming application using generated diagnostic parameters must explicitly select the **feature/localisation generator** and the newer **ClockEra owner profile** until governed branch/version promotion; the existing `EDP-Serialisation` `library.json` `Localisation#main` transitive pin alone does not establish that generator capability. This is an opt-in feature assembly limitation, not an unqualified failure of the minimal Serialisation API, Clock or Memory main contracts. Resolving automatic manifest version selection for arbitrary consumers requires separate human-approved branch/dependency migration and cross-consumer compatibility tests; no such broad pin change is performed in this Logging G14 tranche. All versions remain 0.1.0.


The unresolved optional branch-pinning decision is tracked by [EDP-Serialisation #11](https://github.com/ESPressio-Development-Platform/EDP-Serialisation/issues/11); do not close G14 source/owner-policy acceptance based solely on the four passing latest-owner firmware builds. A stable-manifest-only generated source build has two real negative receipts, whereas the narrowed runtime and Memory/main profiles have positive receipts as described above.


## 3 November 2026 — G14 target compatibility, lexical V2 cleanup, and unresolved boundary

**Target build qualification on currently published feature source (documentation-only Wiki changes).** RPI400 compiled the exact real-owner application composition from a SHA-pinned 19-owner snapshot under ESP-IDF 5.5.5: full G1 Logging/Serial source and JSON/CBOR integration `20261102T174658-d119edbc` **PASS exit0**, and G7 15-field typed schema, strict NVS-ACK + LittleFS target `20261102T173948-b50ec8a2` **PASS exit0**. Both shipping-style demonstration applications built in each of their three documented compile targets, all **PASS exit0**:

| Application | Arduino IDE / ESP32 core 3.3.12 | PlatformIO Arduino / 3.3.12 | PlatformIO ESP-IDF / 5.5.5 |
| --- | --- | --- | --- |
| `ConfigurableBoundedEdlsBatch` | `20261103T091853-6bb78326` | `20261102T174928-fc9b3c9a` | `20261103T091204-adde52aa` |
| `TypedManagedBatchWorker` | `20261103T091531-e8e7e011` | `20261103T091200-f52ba756` | `20261103T092234-60d5e692` |

These are **fresh build-only** receipts, not an independently executed runtime, a fresh GPIO17 power cut, or a proof that an application using arbitrary PlatformIO automatic manifest pins is qualified. They also do not create an EDP-Serial-Arduino provider; that separately selected endpoint and its hardware requirements remain under `EDP-Serial-Arduino#1`.

**New V2/code-style corrective candidate.** Source `feature/logging-v1` had a demonstrable Rule 16 whitespace inconsistency across several optional headers. A constrained isolated 35-header *formatting-only* correction preserves exactly the same Clang 19 C++20 lexical tokens for every changed header, including the original `>>=` shift-assignment tokens. Initial naive whitespace automation split two operators and was **rejected** after real host compile failures; the source candidate was repaired without publishing the fault. Full Clang lexer comparison `20261103T094530-9536139a` **PASS exit0** after final namespace indentation restoration, `git diff --check` PASS. The corrected, isolated candidate passed all **34** selected GCC C++20 host entrypoint scripts in `20261103T093633-79a3395f` **PASS exit0** (including historical Localisation archive; the two earlier matrix failures due to missing test-harness paths and compiler args are preserved). Xtensa ESP-IDF 5.5.5 typed strict-ACK build of the corrected candidate `20261103T093540-3d521574` **PASS exit0**; exact final-indent target recheck `20261103T094444-fec1058b` **PASS exit0**. No change to algorithms, binary API, record fields, serialised bytes, dependencies, branch naming or package version is intended.

**Focused Wiki contract quality.** A source/Wiki path and link regression `20261103T094441-98622295` **PASS exit0** finds all 42 production source units linked into 62 repository-local Wiki pages and no broken local Wiki/docs links. Expanded the architectural and member-level references for the especially safety-critical `TransactionalRecordQueue` and `PowerLossEdlsSnapshots`, covering result enum semantics, public/private methods, ownership, lifecycle, finite capacity and error handling. **However:** file-level index coverage and two detailed pages do **not** establish exhaustive `V2/WIKI_REFERENCE.MD` per-member traceability of all 42 production headers. The wider independent heuristic found 132 *candidate* missing names, including many clear provider-call false positives; each real omission must still be classified and corrected. Some remaining `V2/CODE_CONVENTION` requirements (notably all multi-argument call layout and every member's VS Code documentation comments) are likewise not established by a lexical whitespace change alone.

**Release determination:** The specifically documented, finitely bounded WROOM/ESP-IDF LittleFS+NVS Logging configuration has prior real 100 records/sec offered-load and external GPIO17 PowerLoss evidence, distinct from these new builds. It is a **production-capable, scoped profile**, not unconditional universal `production-ready` status. Retained logical byte capacity is application-selected at Bootstrap/initialisation and immutable afterwards; rollback requires preserving complete acknowledged generations until verified successor/ACK. No unlimited storage, generic VFS PowerLoss atomicity, workload-independent sustained throughput, flash service-life guarantee, full G12 Arduino endpoint, or whole-organisation V2 certification is claimed. Continue G14 full semantic policy and remaining source/Wiki assessment before closing Logging issue #15.

**Historical 2 November G14 source-specific blockers:** `20261102T181046-c14c3719` and `20261102T181126-798c1f8e` genuinely failed under **then-current** Localisation/main and Clock/main sources. The original issue and the exact negatives are retained. They **must not be presented as defects of the subsequently promoted main heads**. See the following 9 November requalification.

## 9 November 2026 — fresh promoted-main optional G3/G4 generator, ClockEra and strict EDLS acceptance

Live GitHub `Localisation/main` `8b826f4...`, `Clock/main` `9a27b12...`, `Serialisation/main` `f36e615...` now publish their previously feature-only generator/`ClockEra.hpp` contracts. GitHub Logging `feature/logging-v1` source tree `6060b3cf9705edc199f396aa415f2bf9a1fdabf4` was exactly matched with an untouched isolated RPI400 checkout. Fresh detached **`20261109T090236-f9d01ff1` PASS exit0, eight of eight individual cases**, exercised current promoted-main generated diagnostic parameters/ClockEra and RuntimeIncarnation; GCC and Clang ClockEra typed CBOR/JSON positive plus compile-negative; two independent GCC/Clang generated canonical CBOR/EDLS integration builds and strict NVS-ACKed Memory/Threading Task integration; historical Localisation schema bridge. All cases carry individual PASS rows and per-case logs in the 9 Nov isolated recovery scratch, no old FAIL result rewritten. Reference source/links `20261109T090831-20ed9df2` **PASS exit0**, 42 sources, 62 Wiki pages, zero broken reference/source links.

**G14 interpretation:** the two former `main` source incompatibilities are no longer release blockers for these exact current-owner host compositions. Not demonstrated by this gate: arbitrary PlatformIO manifest auto-resolution, all ESP-IDF or Arduino native profiles, every provider/runtime composition, renewed physical GPIO17 durability, or full per-member V2 Wiki compliance. G12 Arduino Serial is still a deliberately deferred provider. [Current detailed dependency findings](DEPENDENCY-OWNER-BASELINE.MD#9-november-2026--promoted-main-generator-and-clockera-dependency-requalification) and [Policy/main living handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-09/2026-11-09-08-46-EDP-Logging-Recovery-And-Reintegration.MD) govern any subsequent acceptance or `main` promotion. Version `0.1.0` unchanged.


## 12 November 2026 — latest source, dependency-main and publication gate truth

This dated section **supersedes older present-tense November 1–2 remaining-merge claims**, without deleting their correctly recorded negative test receipts. The selected WROOM strict LittleFS+NVS 30,000 offered-record/100 records/sec/300 sec physical profiles `20261101T193528-695b02cf` and `20261101T194232-bc98b8ae` passed, with 30,129 records actually accepted/retired/durable, 125 original retained 15-field records verified from independent raw media, NVS ACK472 and bounded measured heap. The separate `20261102T090006-e5a4b0db` real external GPIO17 8.260sec OFF/POR-confirmed protected-record power-cut test and independent raw LittleFS/NVS CRC evidence **PASSED**, preserving protected 128 records and strict ACK2. The earlier timeout and wrapper failures remain *real* negatives for earlier configurations; they do not erase later scoped positive qualification. No universal indefinite throughput, generic LittleFS VFS FileStorage power atomicity, Flash service-life guarantee or all-provider release certification is inferred.

Dependency-first publication is now **complete for the selected mandatory/conditional owners consumed by Logging**: `EDP-Clock/main=9a27b12d5938dbd1cb26af541a489531a848ba25`, `EDP-Localisation/main=8b826f4c9becfa34eb3a885361f70033f8ff3f22`, `EDP-Serialisation/main=f36e615720ce093a03482ae710afe129d031a551`, `EDP-Platform-Portable/main=267411bf319a269883d2273928ad19fc05e2f324`, `EDP-Memory/main=9c5a994eb1e101b80a6fb0e1357c0add0e3a38c4`, and `EDP-Threading/main=1c5ff530c7051d0ccb9480c9a46fbb3cd9424750`. The Memory/source feature branch and all prior main revisions were retained rather than force overwritten. The post-Memory-main independent native/host suite `20261110T141859-4bbe5666` **PASS 79/79** (including five Memory demos for both PIO targets). The subsequent post-Threading-main exact revised owner suite `20261110T144522-0b07e433` **PASS 81/81** finished 10 November 15:28:44 CET, including all three Logging demo families, both PIO Arduino/ESP-IDF targets, complete selected 62 Logging GCC/Clang and six foundation suites. Full raw per-case log/TSV paths and exact Git branches appear in [Policy/main living handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-09/2026-11-09-09-29-EDP-Logging-Dependency-First-Reintegration.MD).

**Logging feature provenance before morning main promotion.** At that checkpoint, `feature/logging-v1` contained the candidate and `main` still pointed to the stub. After the 9–10 Nov independent source-tested production baseline `8d2b307a8a1db1aa5596abc904cd0da1f3c032cd`, later V2-release changes introduced public README/dependency/docs/Wiki, a `CHANGELOG.md`, and two Framing demo PlatformIO manifests selecting the already qualified PIOArduino `d41d8383706dc146fd91a0e1d3c59a915a4d493e` git revision. The 12 November V2 Wiki corrections include declaration-level status/members for EDLS strict ACK, generated task delivery, bounded owned arguments, generated wire/record validation and source-specific Python tooling. The `tools/` changes affected Python module docstrings. The exact published-main and Python results are recorded below.

**Scoped exclusions:** direct native finite/Forever blocked EDP-Memory waiter failed-relock safety remains unqualified under [Memory #16](https://github.com/ESPressio-Development-Platform/EDP-Memory/issues/16); Logging's bounded producer instead uses Memory NoWait + EDP-Threading/Clock wait. G12 EDP-Serial and EDP-Serial-ESP-IDF were optional feature-branch outputs at this checkpoint. Their later main promotion is recorded in the Serial integration checkpoint. The independent Arduino Serial concrete provider remains deferred. G13 native-memory raw dump remains deliberately excluded. The 42 source headers/62 Wiki pages are navigationally present; full V2 declaration/member semantic quality and source conventions must be assessed and corrected, not inferred from a file count. All versions remain **0.1.0**, and no new release tag is authorised by this ledger.
## 12 November 2026 — published-main and V2 audit disposition

This section **supersedes earlier same-file claims that Logging/main was a stub or that post-main tests were pending**. `EDP-Logging/main` was safely promoted to code-equivalent `9fea51b0bdebcd8a4206628b1e1def81ff620381` and independently tested: `20261112T092223-3f2f828e` **83/83 PASS** (68 GCC/Clang Logging tests, eight optional downstream Serial feature-branch host tests, six PlatformIO Arduino/ESP-IDF builds, one Python suite); after final host-tool comments/description updates, **40 Python tests passed**. The first GitHub HTTPS preflight was aborted without running source tests; its negative receipt is retained. Exactly 42 production C++ file blobs matched their tested GitHub references. All EDP library versions remain **0.1.0**, no release tag.

All G1–G15 requirements now have documented *configuration-scoped* source/test evidence, explicit caveats, or an intentional exclusion (G13 native-memory raw dump). This matrix does **not** close every provider, target, latency/resource/wear, PowerLoss timing, or V2 compliance gate. At that source-main checkpoint, the Serial host tests used feature branches. The later [Serial integration checkpoint](SERIAL-OUTPUT-INTEGRATION.MD) records both companion promotions and WROOM UART evidence. Some Wiki pages have received declaration-level, semantic repair; the **entire 42-header reference corpus has not been independently certified for every method/member**. Memory #16 direct failed-relock waiter remains excluded; the selected Logging NoWait coordination is separately qualified. Keep Logging #15 open for exhaustive V2 and release-governance closure. Earlier finite 100-rps physical and independent GPIO17 powercut/raw-media outcomes are *historical real tests*, not retested by this 12 Nov host/native run.

Raw receipts: `/srv/espressio/scratch/gpt6-logging-closure-20261109-1115-m8q2/receipts/logging-postmain-20261112-retry/`; detailed [living Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-09/2026-11-09-09-29-EDP-Logging-Dependency-First-Reintegration.MD).

## 12 November 2026 — strict EDLS predecessor revalidation

The EDLS predecessor check in `0b79c4dc51a08ca908a0ef56774e9abb8ac195d1` keeps provider failure, invalid baseline, unclean readback, header change and verified-prefix change as distinct internal outcomes. Public fail-closed statuses and the protected retirement sequence are preserved. RPI400 `20261112T205343-fb4dc4a5` passed all ten archive host checks across GCC and Clang. The exact candidate built with the pinned PioArduino ESP-IDF 5.5.5 strict typed fixture in `20261112T205447-17599123`.

Physical RPI400 run `20261112T210501-b2a64228` cut the integrated WROOM supply for **8.273 s** after the complete generation 3 candidate and before its NVS acknowledgement. The controller and new boot epoch verified the cut, and ROM and ESP-IDF reported power-on reset. The pre-cut watermark was ACK2. The fresh boot recovered the protected history and completed through ACK4. Independent readback of the LittleFS partition verified exactly generations 3 and 4, all 15 typed fields and the EDLS/EDLG CRCs. Independent NVS media parsing verified the page, entry and blob CRCs, ACK4 and incarnation 7. The original 4 MiB flash image was restored and verified, and the target was powered off. This is the strict typed ACK-gap configuration. The earlier finite 100 records/s profiles retain their own evidence and limits.

The Logging source was promoted to `8fb097ca26c9d4f034c57c539fd5eaed97394b27` on `main` and the retained feature branch. The two later commits document ticket, temporal and structural record declarations. Host runs `20261112T210132-c42a63dd` and `20261112T210828-c695280e` passed 16 selected checks, including matching public type layouts, generated diagnostic contracts and typed archive integration. Published-main run `20261112T211616-9b0f35f0` verified all 42 Logging source blobs, 19 test and runner blobs and five EDP-Serial main source blobs. It passed 14 GCC/Clang checks across archive inspection, protected rolling, strict ACK, batching, generated worker integration, temporal qualification and Serial fan-out. [Logging #15](https://github.com/ESPressio-Development-Platform/EDP-Logging/issues/15) remains open for the release-wide V2 review. Exact revisions, harness retry details and scoped evidence are recorded in the [living Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-12/2026-11-12-11-28-EDP-Logging-Serial-Release-Recovery.MD).

## 13 November 2026 — release-readiness checkpoint

Both Logging branches include the typed queue borrow contract introduced by
`d6056ed0e463e05d512f90283bc13fe405a71ec6`. Callers inspect `Status()` and use
`Lease()` only on success. The cross-repository audit found two active caller
files, both in Logging's tests. EDP-Serial consumes the EDLG frame contract.
The queue GCC, Clang, UBSan and Xtensa checks, native G1 build, and downstream
Serial frame-pump/FIFO tests passed. The three demonstration families
compiled and linked across all nine Arduino IDE, PlatformIO Arduino and
PlatformIO ESP-IDF combinations. The [living Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-13/2026-11-13-08-38-EDP-Logging-Serial-Qualification.MD)
records the exact revisions and detached run IDs.

Run `20261113T110737-c123431b` passed ten GCC and Clang checks covering the
coordinator quantum, Memory NoWait admission, wake and release publication,
admission shutdown and producer coordination. These are host checks. The
strict LittleFS/NVS, GPIO17 PowerLoss and UART receipts above govern the
physical scope. The complete V2 reference review remains tracked in
[Logging #15](https://github.com/ESPressio-Development-Platform/EDP-Logging/issues/15).

The full host matrix on exact baseline `c80a80aae52362c79af1443bdf68897fca590d6c`
completed in RPI400 run `20261113T110345-92db528b`. All 34 GCC and 34 Clang
test runners passed, along with all 40 Python tests. The later diagnostic
API migration has its own focused qualification receipt in the handoff.

### EDLG and EDLS ByteOperations migration

Logging `5d854d7c7d6d96af0041c9943e50e129f9e5df36` and downstream Serial
`b021ff65fb405937ce2d097d17abe0d59422556e` were published to both active
branches. The opt-in writer borrows the selected Memory byte-copy
provider. Archive copies use the same owner contract and reject overlapping
frame and archive workspace before recovery. The default Logging umbrella
and reader remain dependency-free.

RPI400 run `20261113T115314-5fbcbbf0` passed 68 GCC/Clang runners and 40
Python tests. `20261113T123917-dd8fc7b0` verified the published refs and
passed four Serial pump/FIFO tests. The physical UART run
`20261113T121301-dffea777` passed 20/20, including an independent EDLG CRC
and the WROOM resource and G11 site checks. A fresh verified 4 MiB backup
preceded programming. The original image was restored and verified, and
the board was powered off. The separate strict LittleFS/NVS relay-cut and
finite 100-record/s receipts govern the storage claims.


All nine Logging demo target builds and all six downstream Serial demo
target builds passed on the ByteOperations pair. The three
`FramingAndInstrumentation` results are in `20261113T115541-ab83105e`.
The initial paired run then resolved the wrong Logging sibling in one
PlatformIO build. A corrected sibling checkout in `20261113T121711-ce7e6c43`
passed both Logging managed demos and both Serial demo families on
Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF.


The strict LittleFS/NVS fixture linked from Logging `5d854d7` in
`20261113T125037-f7525d43`. The first physical run,
`20261113T125128-a24d7661`, confirmed a power-on reset after 8.267 s off and
correct firmware replay. Serial noise interrupted the independent LittleFS
capture. The original flash was restored and verified.

Repeat `20261113T131942-75630da5` cut at the complete generation 3
candidate with NVS ACK2. The external controller measured 8.283 s off.
The new boot epoch and ROM and ESP-IDF reset reasons confirmed a power-on
reset. Independent LittleFS readback verified generations 3 and 4, all 15
original typed fields, and the archive and frame checksums. Independent
NVS readback verified page, entry and blob CRCs, ACK4 and incarnation 7.
The raw read used fixed-size chunks and baud-rate fallback.
The original full 4 MiB image was restored and verified. The target was
powered off before the job completed with exit 0.


## 18 November 2026 — EDLS readback and declaration verification

Logging 8d5474d499a0ba00a1fcdd1b88175560a59454da routes oldest-eligible
rotation and sealed successor readback through the existing stateless
EDP-Memory ByteOperations provider. It preserves the two readback
statuses and the predecessor's typed header and prefix checks. The new
negative fixtures store a different but CRC-valid frame after native write
acceptance. Both publication paths return ReadbackUnconfirmed, and
oldest-eligible rotation independently revalidates the protected newest
slot. The exact source passed 34 GCC runners, 34 Clang runners, and
40 Python tests in RPI400 runs 20261118T112133-adf36814 and
20261118T112133-1e85e5c4.

The selected 11 EDP domain manifests form a closed, acyclic dependency
graph. The default Logging umbrella and package manifest have no
mandatory EDP library imports. For the optional native scopes,
EDP-Platform-ESP-IDF remains pinned to
adc4164031016c200ad88d742fc4f05c86d66833 for the qualified Clock and atomic
provider. The other 15 selected owner revisions match their refreshed
main refs. The direct native EDP-Memory blocked-waiter failure case
continues under Memory #16. Logging admission uses the NoWait and
Threading/Clock bounded coordination profile.

The latest declaration review corrected the severity predicate,
generated archive adapters, and selected bounded admission, queue,
snapshot and wire conversion descriptions. All 42 production headers
have indexed branch-local source links in the 62-page repository Wiki.
The documentation pass resolved all 558 local links in the Wiki, README,
dependency graph and focused docs. The per-declaration semantic review
of the remaining Logging and affected owner surfaces is tracked in
[Logging #15](https://github.com/ESPressio-Development-Platform/EDP-Logging/issues/15).
The exact Xtensa syntax and compile-out checks passed in
20261118T113942-907bb482. The prior named physical results above apply to
their recorded source and configuration.
