# Generated LogRecord → canonical CBOR → finite EDLS archive

**Implementation**: `logging/GeneratedLogRecordEdlsArchive.hpp`. Optional existing G3/G4/G7 integration; all component versions remain **0.1.0**.

## Ownership, capacity and ordering

The existing generated EDP-Localisation diagnostic message contract selects exact positional types. The existing `ProjectGeneratedLogRecord` validates the source LogRecord (all 15 approved permanent FieldIds, identical permanent System TypeIdentifier, schema fingerprint, source identity, severity, record facts) and reconstructs each owned argument under that generated contract. The EDP-Serialisation owner measures and writes one **canonical CBOR TypedEnvelope**, including original permanent identifier and typed Field13 values, into Bootstrap/worker-supplied bounded bytes. The existing Logging EDLG framing then writes the checked CRC32C envelope. Finally the already-qualified protected N+1 EDLS archive owner publishes that exact EDLG frame into an unused physical filename, verifies the new generation and all still-protected predecessors, and only then retires the oldest eligible segment.

`PublishGeneratedLogRecordToProtectedEdls<TMessageSchema,TProtectedSlots,TMaximumPayload,TMaximumSegment>(record, projection, positionalScratch, cborScratch, edlgScratch, FileStorage, physicalPaths, deviceIdentifier, runtimeIncarnation)` is a **synchronous worker-side** composition helper, not a new worker, pool, storage provider or wire encoder. Both output scratch spans, the typed projection and generated argument scratch are caller owned; they must remain alive for the entire call, not alias, and may be provisioned through the existing Bootstrap EDP-Memory owner. Record/device incarnation must exactly match the physical EDLS header arguments. The source and generated Localisation fingerprint must match. No hidden allocation, unbounded queue, task, or direct ESP-IDF import is introduced.

The compile-time protected segment count N is a **finite** retention maximum and exactly N+1 physical filenames are required. Complete EDLG payload bounds and complete EDLS segment bounds are compile-time configuration. Indefinite history growth is forbidden. A physically exhausted LittleFS partition may refuse a new immutable candidate; the preceding acknowledged retained history is not deleted. This does not determine the future production partition reserve; physical free-block/GC/wear requirements are separate acceptance gates.

## Results

`GeneratedEdlsPublishStatus` distinguishes `Published`, `SourceIdentityMismatch`, `ProjectionRejected`, `CborRejected`, `CborCapacityExceeded`, `FramingRejected`, and `ArchiveRejected`. `GeneratedEdlsPublishOutcome` retains exact nested `GeneratedLogRecordProjectionOutcome` and `EdlsProtectedRollingOutcome`, plus encoded CBOR/EDLG byte counts. Even `Published` means logical complete readback / protected successor ordering **only**; it does not upgrade `FileInterruptionAtomicity::None` or assert physical power-loss durability of a typed production record without separate hardware testing.

## Verification

New `tests/GeneratedEdlsArchiveIntegrationTests.cpp` plus `run-generated-edls-archive-integration-tests.sh` generate a **real owner-produced** EDP-Localisation schema, compile with actual EDP-System/Clock/Serialisation/Memory/Persistence VFS dependencies, and test multiple finite rollovers with independently reopened filesystem instances. They inspect each persisted EDLG payload and independently invoke the owner CBOR TypedEnvelope deserializer to check all permanent identity/argument fields, not merely EDLG/EDLS CRCs. Negative tests refuse changed device identifier/runtime incarnation, too-small CBOR scratch and changed schema fingerprint *without mutating the archive*. GCC, Clang and sanitised results are tracked by immutable RPI400 run IDs in the Policy handoff.

The remaining blockers for a full production release are the actual queue/worker/pluggable sink lifetime integration under saturation, physically qualified **this full typed payload** on ESP32-WROOM during hard VDD interruption, unambiguous torn-candidate recovery, finite physical write reserve / garbage collection and wear budgets, and outstanding G1–G15 acceptance gates. Existing synthetic and short EDLG native hard-cut results do not automatically qualify all longer typed payloads.


## 2026-10-26 — existing EDP-Memory / Threading Task handoff (G10)

Optional `logging/GeneratedEdlsTaskAdapters.hpp` adds only two adapter types for the
**already-existing** owner pipeline. `GeneratedLogRecordCborEncoder` takes
existing generated schema-qualified source `BasicLogRecord`, caller-owned
`GeneratedLogRecordWireProjection` and `OwnedPositionalScratch`, and emits
EDP-Serialisation `TypedEnvelope` CBOR directly into the existing
`EncodeOnceAndSubmitToSinks` EDP-Memory buffer. The existing
`AsyncCopyingTaskSink` makes an independently owned, finite EDP-Memory copy
before dispatching an existing EDP-Threading `Task<RecordSinkSubmission>`;
Task admission is **not** archive durability.

The Task's application-composed destination is
`TypedCborProtectedEdlsTaskDelivery`. It verifies exact canonical CBOR
re-encoding (rejecting trailing bytes/noncanonical alternatives), generated
Localisation message and fingerprint, permanent typed field identity,
incarnation, DeviceIdentifier, attempt and declared sequence width; then
uses existing EDLG framing and `PublishEdlsProtectedRollingSuccessor`.
This callback is synchronous on the existing Task worker and never retains
its borrowed CBOR span. Return `Accepted` ONLY when the protected archive
commits and verifies the generation; otherwise `Rejected` for untrusted
input or `ProviderFailure` for archival failure. `LastCompletion` and
`LastArchive` are terminal **Task-owned** evidence, read only after
Task completion/consumer-owner synchronization (not atomic). The adapter
allocates no new worker, queue, pool, codec or concrete FileStorage.
The caller supplies exclusive FileStorage ownership across scan/write/retire,
separate bounded frame scratch, static N+1 path array, projection scratch
and stable lifetime of every provider and task handle.

Host generated-schema real VFS tests execute the encoder and Task delivery
callback, including invalid codec, malformed CBOR, canonical trailing-byte
rejection and correctly checked finite generation rollover. Prior
`AsyncCopyingTaskSinkTests.cpp` independently qualifies real EDP-Threading
Task lifecycle, Memory-copy ownership, cancellation, backpressure and release;
combination under actual production scheduling/physical crash remains a
release gate, not inferred from distinct host validations. WROOM worker stack,
physical scratch and GC/wear bounds also remain open; versions remain 0.1.0.

The native WROOM 4096-byte payload test found the former stack-backed nested
archive path unsafe at main task stack 8192B. Production archive workers
**MUST use** the `WithScratch` typed publication and Task delivery adapters,
provisioning external `EdlsHeaderBytes+TMaximumPayload+20U` archive scratch
separate from live EDLG source-frame bytes. Both original convenience APIs
are retained for compatibility but do not carry an ESP32 high-payload stack
budget promise. See [protected rolling](EDLS-PROTECTED-ROLLING.MD).

## 27 October 2026: complete typed hardware VDD and independent physical decoder

After an initial test-fixture failure (native HALT TYPED_PREPARE; **no VDD cut**, run 20261026T131102-e6b39b9f FAIL), the generated HIL fixture now initializes an empty BasicLogRecord for each emitted generation, as required by the source-assembly contract. The corrected exact native ESP32-WROOM firmware was compiled successfully in run 20261026T131821-14f5f33a and subjected to the genuine GPIO17-controlled physical VDD cut **20261027T114439-028890f5 PASS exit0**: 8.254s independent OFF interval, new ROM POWERON_RESET and ESP-IDF reset=1, CRC/typed recheck of immutable g1/g2/g3, recovery of pending retirement only after successor verification, final finite retained g3/g4. Firmware checked all 15 declared fields and the generated argument values on both sides of reset.

**Independent physical readback**: RPI400 run 20261027T115023-aead4879 read 589824B at physical partition offset 0x170000, raw SHA256 **2754ea8b07e51d0756b39035ca864fd9465d1f1c047edca972f4374e266dd263**. The initial offline auditor correctly exited FAIL because its assumed wire fixture was WRONG: a never-present optional synchronized timestamp (FieldId 7) is canonically *omitted*, and std::array<uint8_t,16> diagnostic fingerprint (FieldId 12) serializes as a sixteen-element numeric array, not a CBOR byte string. No physical data, firmware or production serialization contract was modified to appease the auditor. The independent auditor has since been corrected to require exact mandatory FieldIds 0..14 except absent 7, explicit per-field values, and immutable owner-generated 16-byte fingerprint **[197,113,70,141,242,104,131,221,93,63,240,70,7,164,83,169]**. Rerunning against the SAME raw physical dump produced **OFFLINE_TYPED_EDLS_AUDIT=PASS oldest=3 newest=4 fields=15**, and the full 50 host offline negative/controller tests PASS. Original auditor FAIL transcript remains preserved; do not relabel it. Both host-verified full typed 15-field source ownership and on-media envelope are now qualified at this one inter-operation cut boundary, NOT at write/GC interruption.

**Actual existing owner lifecycle**: additional test and CI runner integrate a genuine Bootstrap-created EDP-Memory ObjectPool, OwnedRepresentationFanout canonical TypedEnvelope encoder, AsyncCopyingTaskSink using EDP-Threading Task handles, and post-completion EDLS physical archive. The deterministic test executor intentionally delays Task completion, verifies the source may disappear without invalidating copied CBOR, verifies no durable ACK at Task admission, then reopens actual filesystem across 16 finite generations. GCC 20261027T114556-29d189da, Clang 20261027T114556-af142881, GCC UBSan 20261027T115520-1d7f297c all PASS. **This tests the real Task handle ownership under a deferred test scheduler**, not a fully scheduled/free-running on-target worker and not saturation throughput.

Open production blockers remain cut-during-actual-write/GC behavior, provenance-authorized automated torn-candidate handling, real flash reserve/endurance measurement, full native worker scheduling/saturation/stack and G1–G15 release closure. Existing Persistence FileStorage interruption guarantees remain none/may-modify. All EDP versions remain 0.1.0.
