# G10 asynchronous copying Task sink — bounded independent owner (partial #11)

## Scope

`logging/AsyncCopyingTaskSink.hpp` introduces a **separate, bounded
application-composed source-independent sink** for a single CBOR or JSON
representation. It is an optional G10 tranche: **not** the complete product
or durable Persistence output. It does not change `0.1.0`, the default
umbrella, the canonical LogRecord, or EDP-Mesh.

One `AsyncCopyingTaskSink<Codec,MaxBytes,Pool,ByteOperations,Executor,Sink>`
instance owns **no** native executor, queue, synchronization mechanism, memory
pool, or in-flight record container. Bootstrap instead supplies:

- A real `EDP-Memory::ObjectPool` for `OwnedEncodedPayload<MaxBytes>`, one
  stable caller-owned ObjectPoolLease, and a `EDP-Memory::ByteOperations`
  provider selected in application Composition.
- A stable `std::optional<EDP-Threading::Task<RecordSinkSubmission>>` owned
  by Bootstrap, and one configured EDP-Threading dispatcher using
  `TaskDispatchPolicy::AbandonImmediately` (no internal capacity wait).
- A stable synchronous final-delivery callback accepting borrowed
  `(EdlgCodec,span<const byte>)`; the callback MUST finish its own use of
  that span before it returns. Its code may use app-composed EDP-Serial or
  EDP-Persistence owners, but these are not dependencies of Logging.
- An existing EDP-Threading OrdinaryMutex or comparable externally composed
  serialization for `Submit`, `TryFinalize`, cancellation and owner reuse if
  these operations can be called from different concurrent contexts.

The public `AsyncOwnedDeliveryWork<Codec,MaxBytes,Sink>` callable can be
named **before** the adapter is instantiated, including as the exact
`EDP-Threading::DedicatedWorkerLease<TaskIdentity,...>` task identity.
Alternatively an ordinary TaskExecutionFacility can be selected. Bootstrap
owns its bounded Worker/TaskRecord/stack resources; Logging creates none.

## Single-record lifecycle and typed outcomes

1. The parent `EncodeOnceAndSubmitToSinks` retains its existing one-encode
   and 1–8 callback guarantees. A selected callback invoking
   `AsyncCopyingTaskSink::operator()` can report `Accepted` *only* after
   it acquired a separate native EDP-Memory lease, copied **all** bytes
   using EDP-Memory ByteOperations, and received a valid EDP-Threading
   Task handle from a nonwaiting dispatch. This is **Task admission**, not
   final sink acceptance, transmission or durable retention.
2. The worker's `AsyncOwnedDeliveryWork` contains just a pointer to that
   separately leased payload and the stable final sink. It cannot retain
   the parent source-record reference or parent encoded span.
3. While its Task is Queued/Running, a second submission to the same adapter
   returns `Busy`/Rejected; the independent payload lease is not freed.
   Another sink instance can finish before or after it, without changing
   its ownership.
4. Application Bootstrap waits or arranges notifications using the *existing*
   EDP-Threading Task mechanisms; `TryFinalize` itself never polls,
   blocks, creates clocks or sleeps. It sees a queued/running Task as Pending
   and retains the lease. Only a terminal Task may lead to EDP-Memory release.
5. A Completed Task's `TakeResult` supplies the **actual per-sink**
   `RecordSinkSubmission` (Accepted, Rejected, Partial, ProviderFailure).
   A cancelled terminal Task reports `CancelledAndReleased`. Task handle
   result interest is retired explicitly before the native Memory release.
   A failed or uncertain native `Release` is `ReclaimUnconfirmed` and
   leaves the caller-owned lease present for **explicit** `RetryReclaim`
   after terminal Task state, never a silently discarded native claim.
6. `RequestCancellation` is only an EDP-Threading cancellation request.
   A running Task need not be terminal immediately; Bootstrap must still
   join/wait and retain the lease until `TryFinalize` has observed completion.
   The Task, lease, Memory runtime, delivery callback and adapter references
   must outlive all asynchronous work and final reclaim attempts.

The API rejects a mismatched codec, empty or oversized payload, exhausted
Memory capacity, busy per-sink owner and failed Threading dispatch without
pretending success. Dispatch rejection attempts an explicit native release.
No retries, eviction, task polling, double ownership or hidden allocations
are introduced by Logging itself. Pool capacity and Task boundedness are
chosen by the application; the simple example has **one in-flight item per
sink**. This is independent from EDP-Serial's already-implemented
copy-on-enqueue FIFO, which remains a valid synchronous callback instead.

A native callback returning Accepted is **not** proof of physical UART
delivery, EDP-Persistence PowerLoss retention, or an external receiver's ACK.
The eventual final delivery status must be preserved separately from
the parent producer's successful admission.

## Present validation and remaining gates

`tests/AsyncCopyingTaskSinkTests.cpp` uses real EDP-Memory topology/pools,
real EDP-Memory ByteOperations, the EDP-Threading public Task and
TaskDispatchResult ABI, and an intentionally **synthetic deferred executor**
that drives terminal Task transitions explicitly. It verifies input/source
mutation after admission, independent one- and two-sink completion order,
mixed outcomes and asynchronous capacity pressure, cancellation of queued
work, rejected dispatch and explicit release of native Memory leases.
The public work Type is certified for a valid statically composed
`DedicatedWorkerLease` with bounded one-record Task capacity. This is
host **API/lifetime contract** validation, not evidence that a real native
EDP-Threading Worker executed these Task callbacks.

Uncompleted #11 work, in order: run the actual EDP-Threading
StaticThreadingRuntime and native Worker through the adapter with
slow/blocked/failed final sinks; qualify cancellation of already-running
Tasks, independent timed wait/stop with canonical Clock, native Memory
release-provider fault injection and safe recovery; establish product
resource/stack budgets and real physical WROOM execution. A final EDP-Serial
receipt or cut-power EDP-Persistence durability claim remains separately
unproven. G1–G15 product release is still open.


## 2026-10-22 G10 native EDP-Memory release fault follow-up (#11)

The test fixture additionally uses the **actual EDP-Memory Runtime's**
Mutex provider `FailNextAcquire()` to force native
`ObjectPoolLease::Release` to return `ProviderFailure` after the
Task has reached terminal Completed state. The Task interest is retired,
but the external Memory lease is still nonempty; admission remains Busy
and a caller-owned `RetryReclaim()` later releases the exact same
native claim once its provider is healthy. A second injected failure
occurs during the rejection-cleanup path after Threading dispatch
refusal: admission is `ReclaimUnconfirmed` and the same lease remains
available for explicit recovery. Neither case silently discards
provider-uncertain ownership.

The deterministic synthetic Task driver now also exercises a
cancellation request *after* a Task has entered Running state. Such a
request leaves Task Pending and native Memory owned while the callback
is still executing; only after its non-acknowledging callback completes
is the native lease eligible for reclaim. This tests the public Task
contract, **not a real simultaneous multicore race or underlying
native worker implementation**. Real EDP-Threading Worker execution,
provider-accurate cancellation and full product teardown must still
be qualified independently before #11 is closed.
