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
