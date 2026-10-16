# RecordProduction — Mesh-independent bounded Logging

Status: partial G1/G10, version 0.1.0.

Source: logging/RecordProduction.hpp. Bootstrap composes an ordinary
EDP-Threading sequencer Mutex, monotonic/synchronized EDP-Clock capture
provider, EDP-Memory topology, EDP-BoundedTopology FIFO and sink callback.
No EDP-Mesh import, dependency or implementation exists in Logging.
Monotonic-only Clock is valid. Optional Mesh-referenced synchronized
Clock coherence belongs to the application's Clock/Mesh integration.

ProduceRecordUntil issues a non-wrapping ticket and captures time ONCE
before bounded queue/Memory waits, then commits an exclusive FIFO record.
DrainOneRecord borrows an EDP-Memory lease to one bounded sink callback and
attempts explicit native release. Sink Accepted, Rejected, Partial and
ProviderFailure never imply flushed, physically received or durable.
Failed native release retains exclusive source ownership for Bootstrap.

GCC and Clang host tests, 3x physical ESP32-D0WD test app runs with
real FreeRTOS/Memory, EDP-Serialisation CBOR and CRC verified EDLG UART
output PASS. Source sample payload is a TEST-ONLY positional tuple;
the permanent System governed TypeIdentifier and full canonical LogRecord
are still outstanding. Consult docs/RECORD-PRODUCTION-OWNERSHIP.MD
and tests/hil/g1-espidf/VALIDATION.MD.

## Bounded fanout — DrainOneRecordToSinks

Drains one native Memory-owned source to 2..8 application-supplied
noexcept synchronous sinks in argument order and returns
RecordFanoutOutcome<TResultCount>. Source is the underlying
RecordDrainOutcome; Submissions is a fixed array of native sink
statuses; Attempted, Accepted, Unaccepted are exact counts.
FullySubmittedAndReclaimed() requires all sinks accept and native
Memory reclamation succeeds. Each sink is called exactly once,
including after another sink rejects; no automatic retry. Borrowed
source contents become invalid after the final sink and reclamation.
This is not an asynchronous retained-buffer fanout, nor proof of
durability, physical wire receipt or full LogRecord root identity.
