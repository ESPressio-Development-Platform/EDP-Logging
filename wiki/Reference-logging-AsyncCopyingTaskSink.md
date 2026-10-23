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

## Tests and unimplemented product gates

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
