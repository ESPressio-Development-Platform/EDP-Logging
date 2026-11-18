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
   waits on a Task deadline, creates clocks or sleeps. Owner operations can
   acquire native mutexes. It sees a queued/running Task as Pending
   and retains the lease. Only a terminal Task may lead to EDP-Memory release.
5. A Completed Task's `TakeResult` supplies the **actual per-sink**
   `RecordSinkSubmission` (Accepted, Rejected, Partial, ProviderFailure).
   A cancelled terminal Task reports `CancelledAndReleased`. Task handle
   result interest is retired explicitly before the native Memory release.
   A failed or uncertain native `Release` is `ReclaimUnconfirmed`.
   A retained token permits explicit `RetryReclaim` after terminal Task
   state. Preserve the first provider result even if the lease is empty.
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


## 2026-10-23 physical native Task qualification (bounded scope)

The real RPI400-integrated ESP32-D0WD HIL now binds the published
`AsyncOwnedDeliveryWork` to an actual topology-owned EDP-Threading
`DedicatedWorkerLease` and uses real Memory pools and callbacks.
The `AbandonImmediately` worker-admission policy explicitly refuses a second
Task while the sole Worker executes a slow first Task. Its rejected
admission reclaims its own lease immediately and leaves the first lease
held. A running-Task cancellation request does not release bytes before
the still-running callback completes, and the payload remains correct
after parent source mutation. A subsequent Task is admitted after the
Worker becomes available and reports its final native Rejected result
distinct from admission.

Real PioArduino ESP-IDF build and three on-device reset/UART cycles pass
**19/19** mandatory markers each. Physical evidence run:
`20261023T065718-c2eb8268` (PASS, exit 0). Exact firmware factory SHA:
`26ba1e46da43d43596ded351e428d3247e61a54d0d5d8867fa60ef9e853c9765`.
See `tests/hil/g1-espidf/VALIDATION.MD` for the earlier failed run,
build/generator provenance and limitations. This supersedes the earlier
host-only qualification statement **for these specific native paths**;
queued-task cancellation, concurrent shutdown races, forced physical
Memory provider release failure, durable Persistence and release-wide
stress/resource qualification are still unproven.


## 2026-10-23 G10 eight-sink repeated finite lifecycle stress (#11)

The optional source-owned callback is now verified at its maximum approved
static width (8 distinct sinks) for **96 consecutive independent record
lifecycles**. The host fixture uses a real EDP-Memory nine-slot object
pool (one transient encoded source and eight distinct downstream leases)
and eight independent instances of the public Task API's deterministic
completion driver. It never extends the borrowed source beyond the
synchronous callback. The test deliberately mutates that source, completes
children in reverse order, exercises mixed Accepted/Rejected/Partial/
ProviderFailure final statuses, cancels some queued children before
delivery, injects bounded Task admission rejection every seventh cycle,
and attempts premature per-owner resubmission while busy. Each admitted
Task keeps precisely its own native lease until its terminal result;
every cancelled/rejected owner independently reclaims or preserves its
claim, and the native MemoryRuntime successfully tears down at the end.
This is **simulated Task scheduling using the actual EDP-Threading public
Task handle contract and real EDP-Memory allocation/release**, not eight
physically concurrent ESP32 workers.

The exact pinned-source RPI400 host matrix includes GCC
`20261023T075856-840410f0` PASS exit0, Clang
`20261023T075948-2b0906cc` PASS exit0, Clang UBSan
`20261023T075949-f3bd4d7c` PASS exit0 and prior G10 fanout
GCC `20261023T075949-31a04a6a` PASS exit0. One initial
compilation failed (`20261023T075833-330e2808`) because the new
test erroneously used `Attempted` rather than the public
`OwnedRepresentationOutcome::SinkInvocations` member; this was fixed
before the passing matrix.

**ASan limitation recorded without a false PASS:** Clang ASan host run
`20261023T075949-fe4092fe` failed at the sanitizer allocator's
initialisation without executing the tests. Independently compiled and
executed **trivial** GCC and Clang ASan smoke binaries on the same RPI400,
runs `20261023T080056-db8dcd7a` and
`20261023T080056-b18b4fec`, and both failed at the same
`sanitizer_allocator_primary64.h` `kSpaceBeg` check. GCC ASan
on the real test `20261023T080107-ab8523ba` also failed on this
same runtime check. None emitted an application memory diagnostic.
Therefore UBSan is executable evidence, but **ASan remains unavailable
on this appliance and must be repeated on an authorised ASan-capable
execution environment before claiming ASan qualification**.

## 18 November 2026 — selected Threading dispatch and teardown boundary

The Logging adapter asks for `AbandonImmediately`. At qualified
EDP-Threading `1c5ff530c7051d0ccb9480c9a46fbb3cd9424750`,
`TaskFacilityRuntime::Dispatch` checks the availability of a worker and
refuses admission if work is queued or a grant cannot be confirmed.
`TaskFacilityCore::HasWorkerGrant` is false for a Task in Queued state.
The accepted handle from the selected DedicatedWorkerLease therefore
cannot exercise cancel-while-queued through this adapter. The deterministic
host dispatcher still verifies the adapter's terminal-cancellation and
lease handling for a Task supplied in that state. Native coverage of an
alternate queued profile requires a separately designed bounded dispatch
contract. Do not bypass the supplied Threading policy in the native fixture
to claim qualification for this profile.

Immediate dispatch policy governs worker availability and capacity
waiting. The task facility's internal ordinary mutex uses
`WaitTimeout::Forever()`. Task inspection, cancellation and result
acquisition also enter owner synchronization. Bootstrap must account for
these waits when budgeting concurrent shutdown. There is no end-to-end
time bound from this adapter alone. Bootstrap serializes adapter calls
and keeps Memory and Threading intact until the Task is terminal and the
lease release is confirmed.

The selected WROOM profile passed active shutdown and a fault-injected
native Memory release. Build `20261118T140753-4ed0029e` and physical run
`20261118T140753-4d196cfd` used test revision `1bea40915c1281c9ce6dfb60edd1c086251bed16` and factory SHA-256
`87094f97a572a46f84845f51fdefdcad20ef3fad4c5b5e2570f16581bc703cc2`. All 22 UART markers passed, including EDLG CRC, the
resource snapshot and the configured G11 occurrence check. The original
4 MiB flash was restored and verified.

The release-fault fixture delegates locking to the native FreeRTOS
mutex, except for the second acquisition within the Memory release
transaction. It observes the original delivery result and
`ReclaimUnconfirmed` after the payload object is destroyed. The
Task handle is cleared but the destroyed token still owns its slot.
One explicit `RetryReclaim` returns that slot, the next retry reports
`AlreadyEmpty`, and both pool slots can be acquired again.

For the shutdown fixture, Bootstrap holds a real delivery active.
Threading accepts `BeginShutdown`, remains non-quiescent and refuses
`FinalizeShutdown` while the Task still owns the payload. After the
callback completes, the exact bytes and final delivery receipt are
verified, the Memory lease is returned, Threading finalizes and the
Memory runtime tears down. The qualification applies to the selected
immediate-admission topology and the injected pre-capacity-return
acquisition fault.

A separate Memory error can arise after capacity has already returned,
such as a failed waiter service or native unlock. Memory then clears the
token and quarantines coordination while returning `ProviderFailure`.
Bootstrap must preserve the original failure receipt. A subsequent
`AlreadyEmpty` observation cannot establish successful recovery of that
provider. The physical test does not inject this later failure.
