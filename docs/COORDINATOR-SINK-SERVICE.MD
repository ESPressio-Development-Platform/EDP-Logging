# Ordinary sink service on the existing dedicated coordinator

Status: feature/logging-v1, version 0.1.0. Host-qualified on RPI400 with
GCC, Clang and UBSan. The explicit native O2/500ms/160MHz profile also
passes the finite managed-worker workload and actual-media audit described
below; other configurations and remaining reliability gates stay separate.

## One owner and one canonical deadline

An existing synchronous source sink may optionally implement
noexcept ServiceCoordinator() returning CoordinatorSinkServiceOutcome.
The coordinator polls after every healthy bounded quantum, including
BudgetConsumed, so continuous input does not suppress service.
Queue claim/reclaim failures retain their existing fatal behavior.

Ready may return an optional Clock::MonotonicTimestamp NextDeadline.
When input is empty, the same EDP-Threading ThreadContext waits until the
earliest sink deadline. With no deadline the existing finite WaitFor
fallback applies. Bootstrap must bind batch and Threading waits to the
same canonical monotonic origin. No timer, second worker, queue, signal,
clock provider or allocator is created. Service time and scheduler
lateness are separate from the configured accumulation dwell.

DurableSinkCompletions is a newly confirmed, consumed-once record delta
for that sink. The atomic coordinator report sums these per-sink deltas.
Two sinks completing the same record count twice. RecordsRetired and
AllSinksAccepted still describe source reclamation and submission only.

RecoveryRequired increments SinkServiceFailures and exits with
SinkServiceRecoveryRequired. The coordinator does not retry uncertain
publication. Stop does not implicitly flush or make pending RAM durable;
Bootstrap must choose its explicit bounded shutdown disposition.

## Generated typed batch delivery

The existing TypedCborProtectedEdlsTaskDelivery accepts an explicitly
configured BoundedEdlsBatch target. It reuses the same generated schema,
all typed identity checks, canonical re-encoding and CRC frame writer.
A successful callback copies the frame into bounded RAM and reports
BufferedForBatch. Task Accepted, source retirement and independent copy
release therefore do not imply file or NVS durability.

On FlushRequired the incoming frame was not consumed: the callback
flushes the prior batch once and reoffers only after a confirmed ACK.
Both that path and ServiceCoordinator accumulate only the archive's
confirmed AcknowledgedRecords, including acknowledged pending-retirement
segments. The next service poll consumes the accumulated delta once.
An uncertain file/NVS result emits no ACK for that publication, leaves
the batch recovery-required and refuses automatic retries.

This callback is noncopyable and nonmovable for batch targets because
it owns consumable completion deltas. Legacy nonbatch targets retain
their existing value semantics and do not expose ServiceCoordinator.

The typed delivery takes encoded bytes, whereas DedicatedCoordinator
calls source-record sinks. Bootstrap's existing source callback must
encode with EncodeOnceAndSubmitToSinks and forward ServiceCoordinator
to the delivery. Both operations must run in the same serialized
ordinary owner. Do not poll an asynchronously executing Task delivery
from a different thread without an explicit owner synchronization
contract. The optional callback alone supplies no cross-thread safety.

## Evidence and reusable runners

RPI400 detached 20261030T153923-67d59a82 passed GCC and Clang with
UBSan: actual Memory queue, canonical earliest absolute idle wait,
service under continuous input, per-sink counting, fatal recovery and
wait-provider failure. Run tests/run-coordinator-sink-service-tests.sh
with the compiler as its first argument.

Detached 20261030T154316-d9e41c72 passed GCC and Clang with UBSan:
actual Memory pool and Task handle ABI, generated full typed CBOR,
real POSIX-backed VFS, atomic NVS fake, RAM versus durable completion,
50ms exact partial deadline, count-pressure flush, consumed-once
deltas, zero dwell, commit-with-lost-response and no retry. Independent
file reopen verified final original typed records and rejected input
was absent. Run
tests/run-generated-edls-batch-service-integration-tests.sh with CXX.
These are host control-flow/provider tests, not physical power-loss
or native scheduling evidence. All execution was on RPI400, with no
GitHub Actions coverage dependency.

## Finite queue contention (2026-10-30)

[Bounded queue transitions](BOUNDED-QUEUE-TRANSITIONS.MD) adds an explicit
CoordinatorQueueLockWait compile-time opt-in on the same coordinator.
An unclaimed timeout still services sinks and preserves queued sources.
Unconfirmed retirement never replays a sink.


## Native managed-worker evidence

RPI40020261030T180038-622f5b3b PASS uses the same DedicatedCoordinator,
generated full typed encoder/delivery, real Threading threads, separate
existing source/representation Memory topologies, LittleFS and NVS.
Explicit -O2/500ms/160MHz with assertions enabled completes128 pressure
sources, one idle partial and2000 sources offered by two50Hz producers;
all2129 retire and durably complete before finite teardown.
The no-later-input partial ACK is717825us after occurrence with500ms dwell.
The final raw-media audit verifies every original field of92 retained
records and independent official NVS CRC ACK35. Native README records
actual command proof, hashes, latency/resource limits and earlier failures.
No new timer/worker/Memory API or weakened typed validation was needed.

## Full typed three-target example

[demos/TypedManagedBatchWorker](../demos/TypedManagedBatchWorker/README.MD)
provides Arduino IDE, PlatformIO Arduino and ESP-IDF projects sharing the same
application. All three builds and actual two-boot tests pass on RPI400:
20261031T001504-eee39309. The example explicitly observes RAM publication,
retirement while an idle partial remains volatile, durable completion,
exact original fifteen-field replay and real Threading/Memory teardown.
Canonical lifecycle atomic selection supports the Arduino SDK's compiler
atomic configuration without changing SDK flags or adding a worker.
