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


## Entry snapshot safety (2026-10-17)

`ProduceRecordUntil` now captures one qualified timestamp and an
owned finite tuple of scalar/enum source values at instrumentation
entry, **before** even the sequencer mutex can block. A failure after
this source timestamp reports `TimeCaptured=true` even without an
issued ticket; it does not imply any accepted or emitted record.
Use the generated schema-owned LogRecord assembler for bounded text,
bytes and complex strong parameter Types rather than borrowed spans or
pointer-bearing aggregates in the generic constructor. This partial
G1 producer still requires full application Bootstrap composition.


## Bounded coordinator quantum (2026-10-17)

`logging/CoordinatorQuantum.hpp` provides
`DrainCoordinatorQuantum<N>(queue, callerOwnedClaim, sinks...)` for
2..8 statically composed synchronous sinks and at most `N` records.
Typed `CoordinatorQuantumStatus` discriminates empty, consumed
budget, claim failure, uncertain reclaim and an unresolved prior claim.
It preserves per-sink accepted/rejected counts and release-notification
anomalies. The executor is intentionally **not** a worker, wake
provider or detached Thread: Bootstrap must supply the actual
EDP-Threading Dedicated Thread and stop/join lifecycle. The current
scope is finite synchronous fanout, not persistence durability.

## Topology-owned dedicated worker (2026-10-17)

The optional `logging/DedicatedCoordinator.hpp` is an
EDP-Threading `ThreadContext&` callable suitable for binding using
`BindDedicatedThread<Identity>`. It composes bounded source
quanta, native ThreadContext finite wake, per-sink report accounting
and a distinct cooperative exit. Bootstrap owns the actual
`StaticThreadingRuntime`, stable Queue, Memory provider, sinks, native
execution and target wake. No secondary scheduler or worker pool is
created. The callback does NOT prove end-to-end production
LogRecord serialization, task stack limits, power-loss durability,
or async encoded-buffer lifetime. See
[Dedicated Coordinator](../docs/DEDICATED-COORDINATOR.MD).
