# AsyncCopyingTaskSink — G10 partial Task-backed independent lifetime

**Source:** [`logging/AsyncCopyingTaskSink.hpp`](../src/logging/AsyncCopyingTaskSink.hpp)
**Normative contract:** [Async Copying Task Sink](../docs/ASYNC-COPYING-TASK-SINK.MD)
**Parent:** Logging #1; child #11. Version stays 0.1.0.

## Public API

`AsyncOwnedDeliveryWork<Codec,MaxBytes,Sink>` is the public exact callable
identity for Bootstrap-selected EDP-Threading TaskExecutionFacility or
DedicatedWorkerLease. `AsyncCopyingTaskSink<Codec,MaxBytes,Pool,ByteOps,Executor,Sink>`
holds **nonowning** pointers to the separate EDP-Memory ObjectPool and
caller-held Lease, caller-held optional Task handle, EDP-Memory
ByteOperations provider, EDP-Threading dispatcher and synchronous final sink.
It is noncopyable and nonmovable. The supplied objects, including the
underlying MemoryRuntime, must remain alive and at stable addresses until
every accepted Task is terminal and its lease reclaim has completed.

`Submit(codec,borrowedBytes)` is synchronous, no waiting or unbounded
retry, one in-flight Task max. On a successful dispatch the original borrowed
bytes can disappear immediately: the Worker uses only its own EDP-Memory
pooled bytes. `operator()(codec,bytes)` maps admission to the existing
synchronous G10 `RecordSinkSubmission` interface; it **does not** report
eventual delivery as admitted. Codec mismatch, zero/oversized buffers,
capacity failure, busy Task/lease and dispatch refusal preserve typed
admission statuses.

`TryFinalize()` checks the existing EDP-Threading Task terminal state.
Pending work cannot release memory. Completed work moves its exact
per-sink result; terminal cancelled Tasks expose cancellation separately.
Only then is the native Memory lease explicitly released. Unknown native
reclaim leaves the caller-owned Lease intact; `RetryReclaim()` permits an
explicit later attempt *only when no Task remains*. `RequestCancellation()`
is advisory until Task termination. Neither method invents a Clock deadline,
Task queue, Semaphore, Thread or new Memory allocation system.

## Ownership invariants

1. Parent LogRecord and encoded parent representation never escape callback.
2. A successful Task dispatch owns a distinct independent EDP-Memory payload
   for the entire Queued/Running lifetime.
3. Other sinks run/complete independently. No rejected sink rolls back
   another already admitted Task.
4. Bootstrap must serialize concurrent submit/finalize/cancel paths with an
   existing EDP-Threading synchronization owner.
5. Task complete + native callback Accepted does **not** prove UART physical
   receipt, EDP-Persistence durability or true external cut-power survival.

## Historical initial host-only test stage (superseded by later native subset)

The dedicated host test uses actual Memory pools and Threading's public Task
handle ABI with a **simulated** deferred Task executor. It covers 1- and
2-sink independent completion, cancellation, backpressure, mixed statuses,
source mutation, dispatch refusal and valid DedicatedWorkerLease compile
declaration. Actual EDP-Threading native Worker scheduling, live cancellation
race/failure, real WROOM execution, and durable sink completion remain open
under #11. See [normative gate list](../docs/ASYNC-COPYING-TASK-SINK.MD).

## Fault-preservation follow-up

The native test Memory mutex can reject the next real lease Release;
the adapter reports `ReclaimUnconfirmed`, keeps the stable external
lease and forbids resubmission. Only a later explicit owner
`RetryReclaim()`, after Task termination, may complete release.
The same rule applies to failed Task dispatch followed by failed
native Memory release. A running Task's cancellation request is not
terminal and cannot trigger early reclaim. Simulated completion tests
do not prove a real Worker execution context.

## 2026-10-23 ESP32 native Worker subset
An actual ESP-IDF topology-owned EDP-Threading DedicatedWorkerLease now
executes the public AsyncOwnedDeliveryWork on the RPI400-integrated WROOM.
It passed three native 19/19 cycles after correcting the HIL's mistaken
assumption that AbandonImmediately queues work. The policy returns
Unavailable while the Worker is busy; no implicit queue or wait was added.
The rejected second Task's independent Memory lease is reclaimed, while
running-cancel-request work keeps the first lease until genuinely terminal.
Subsequent dispatch and final Rejected delivery are tested. Firmware,
capture IDs, provenance and remaining gaps are detailed in the HIL
VALIDATION.MD. No durable archive or complete Logging release is claimed.

## Repeated eight-sink source/Task stress

A new host test uses a native nine-slot EDP-Memory pool for one transient
source plus eight independent downstream leases; 96 source cycles verify
one encode per representation, mixed downstream status, inverse Task
completion, queued cancellation, admission rejection and exact native
Memory reclamation. GCC, Clang and Clang UBSan passed on pinned RPI400.
Host ASan cannot start on this RPI400 even for a trivial binary;
ASan remains an unfulfilled release check, not a source failure.
See the normative async copying Task document for detached run IDs.

## Source-complete declaration and result-state audit — 10 November 2026

**Source link at the exact reviewed implementation:** [`src/logging/AsyncCopyingTaskSink.hpp`](../src/logging/AsyncCopyingTaskSink.hpp). The original host-only “not implemented” list above represents the early G10 test stage. The subsequent physical WROOM DedicatedWorkerLease subset documented below is real but **does not** itself certify an arbitrary sink's durability or all target races.

### Public `AsyncSinkAdmissionStatus : uint8_t` — every value

- `Queued = 0`: exactly one Task was actually admitted by the selected EDP-Threading execution owner and an independent Memory lease remains live; not final delivery or durability
- `Busy = 1`: an existing Task handle or native lease still occupies this sink instance; no overwrite
- `CodecMismatch = 2`: input EDLG codec does not match the statically selected `TCodec`
- `InvalidPayload = 3`: empty or oversized borrowed input, rejected before Memory/Task mutation
- `PoolCapacityUnavailable = 4`: Memory NoWait could not claim a slot (including reported native timeout contention)
- `PoolUnavailable = 5`: configured Memory topology not initialized, tearing down or unavailable
- `PoolProviderFailure = 6`: true Memory provider fault/unknown acquisition outcome, or unresolvable acquired payload address
- `DispatchRejected = 7`: Threading Task dispatch refused but the independent Memory lease was **positively released**
- `ReclaimUnconfirmed = 8`: a rejected dispatch could not confirm native Memory release; caller must not destroy/repurpose that lease

### Public `AsyncSinkCompletionStatus : uint8_t` — every value

- `NoPendingTask = 0`: no Task handle and independently leased storage is empty
- `Pending = 1`: Task handle exists but is not terminal; Memory must not be reclaimed
- `CancelledAndReleased = 2`: cancellation became terminal and native lease release succeeded (a later native failure overrides it with ReclaimUnconfirmed)
- `AcceptedAndReleased = 3`: completed callback returned `RecordSinkSubmission::Accepted` and native lease release succeeded; **not** a durable archive ACK
- `RejectedAndReleased = 4`: completed callback rejected and lease was released
- `PartialAndReleased = 5`: completed callback reported Partial and lease was released
- `ProviderFailureAndReleased = 6`: completed callback reported provider failure, but the lease was released safely
- `TaskResultUnavailable = 7`: Task claims termination but a result cannot be taken; release is **not** attempted and the stable Task/lease owners remain for explicit reconciliation
- `ReclaimUnconfirmed = 8`: native release could not be proved, even if Task terminal; no fabricated reclaimed capacity

`AsyncSinkCompletionOutcome` has three value fields: **`Status`** (above, default NoPendingTask), **`Delivery : RecordSinkSubmission`** (the actual callback result once a completed Task yields it, default ProviderFailure), and **`Release : Memory::ObjectPoolLeaseReleaseResult`** (the actual attempted native lease release, default AlreadyEmpty when none attempted). The fields must not be conflated: admission, Task completion, callback acceptance, native pool reclamation, and separately qualified durable storage are distinct transitions.

### Public callable `AsyncOwnedDeliveryWork<TCodec,TMaximumPayloadBytes,TDeliverySink>`

Template **`TCodec`** fixes one EDLG representation (Cbor or Json), **`TMaximumPayloadBytes`** is the compile-time size of the independent `OwnedEncodedPayload`, and **`TDeliverySink`** is the Bootstrap-owned ordinary synchronous callback Type. Field **`Payload`** borrows a stable, separately leased `OwnedEncodedPayload<TMaximumPayloadBytes>`; field **`Sink`** borrows the stable callback object. Both must outlive the accepted Task. `operator()() const noexcept` constructs a borrowed `span<const byte>` from exactly `Payload->Used` bytes and synchronously invokes `(*Sink)(TCodec,bytes)`; that span must not escape the call. It returns a `RecordSinkSubmission` and creates no additional copy, allocation, Task or archive transaction.

### Public owner `AsyncCopyingTaskSink<TCodec,TMaximumPayloadBytes,TPool,TByteOperations,TExecutor,TDeliverySink>`

The template parameters select one fixed supported codec; a positive bounded payload byte capacity; a concrete EDP-Memory ObjectPool for exactly `OwnedEncodedPayload<TMaximumPayloadBytes>`; an EDP-Memory-qualifying `ByteOperations` concrete; an EDP-Threading executor exposing `Dispatch(DeliveryWork,TaskDispatchPolicy::AbandonImmediately)` with exact `TaskDispatchResult<Task<RecordSinkSubmission>>`; and a noexcept synchronous final callback returning exactly `RecordSinkSubmission`. Static assertions enforce these owner contracts rather than invent another task or byte-copy provider.

Private alias **`Lease`** is `TPool::LeaseType`; **`Task`** is `Threading::Task<RecordSinkSubmission>`; **`DeliveryWork`** is the exact callable described above. Private pointers **`_pool`**, **`_lease`**, **`_task`**, **`_byteOperations`**, **`_executor`**, **`_sink`** all **borrow** existing Bootstrap-owned, address-stable providers/handles. The optional Task handle and Memory lease are separately owned by the application, not allocated by this wrapper. Constructor binds all six borrowed owners; copy/move construction and assignment are deleted to avoid invalidating a Task's captured payload or sink pointers.

`Submit(codec,source)` requests NoWait from Memory and immediate admission or refusal from Threading. The selected Threading owner acquires its existing facility mutex with `WaitTimeout::Forever()` before making the capacity and worker-grant decision. The operation has no provider-independent wall-clock bound. It validates exact codec, one-in-flight exclusivity and finite nonempty bytes, claims the existing Memory lease with **`WaitTimeout::NoWait()`**, then copies the entire source into separately owned bounded payload using the already-composed ByteOperations provider **before** Task dispatch. The original borrowed source may be destroyed after the call. On Dispatch failure it attempts native lease release immediately and reports DispatchRejected or ReclaimUnconfirmed; on success it takes the Task handle into the caller's optional storage and returns Queued. No queued Task ever points into the ephemeral producer's LogRecord/representation memory.

`operator()(codec,source) noexcept` implements the preexisting synchronous `RecordSinkSubmission` callback boundary: Queued maps to **Accepted** (Task *admission only*); Busy, CodecMismatch, InvalidPayload, PoolCapacityUnavailable and safely reclaimed DispatchRejected map to **Rejected**; genuine faults and unconfirmed reclamation map to **ProviderFailure**.

`TryFinalize()` is a caller-serialized inspection of the existing Task handle. It does not request a Task wait. Its state, result and release operations use the selected owners' synchronization and may wait on those internal mutexes. It returns Pending while work still executes, never releasing that lease early. After terminal cancellation it drops the Task handle and attempts lease release. After ordinary completion it takes the exact returned `RecordSinkSubmission` result, drops the handle, maps Accepted/Rejected/Partial/ProviderFailure to the matching completion state, then attempts native lease release. A missing result leaves the handle/lease in place as TaskResultUnavailable. A failed native lease release overrides the otherwise completed state with ReclaimUnconfirmed. `RequestCancellation() noexcept` asks the existing Task to cancel and returns its real Threading `TaskCancelResult`; it **does not** itself prove termination. `RetryReclaim() noexcept` permits another explicit native lease release only once the Task handle is absent; with a live Task it returns provider failure rather than releasing memory still used by a worker.

Bootstrap must serialize Submit, operator(), TryFinalize, RequestCancellation and RetryReclaim through an existing Threading ordinary-context synchronization owner. A single adapter supports **one in-flight Task maximum**; create separately bounded instances for concurrent sinks. The selected Memory ObjectPool, independent lease, optional Task handle, executor, byte provider and callback must survive through terminal Task handling and any uncertainty resolution. No ISR-safe or universal bounded callback duration guarantee is given. See the higher-level typed EDLS Task sink for its **separate** strict NVS acknowledgement and actual finite HIL evidence.

The pinned Threading `AbandonImmediately` profile returns a valid handle only after assigning a Worker grant. When the sole worker is busy it refuses admission, so Bootstrap cannot obtain a handle that it can cancel while the Task is queued. `AsyncSinkAdmissionStatus::Queued` is the adapter's dispatch-accepted state across qualifying executors. It does not report the Task state. A production profile that returns ungranted queued handles would require its own explicit bounded dispatch and cancellation contract.

The native WROOM test retains a destroyed Memory token when the capacity-return mutex acquisition fails, then retries and verifies both slots. The runtime can also report `ProviderFailure` after returning capacity and clearing the token, for example on failed waiter service or unlock. Preserve the original release outcome across recovery. `AlreadyEmpty` after that failure does not clear a native provider quarantine. The named active-shutdown and injected-release results are recorded in the [async contract](../docs/ASYNC-COPYING-TASK-SINK.MD).
