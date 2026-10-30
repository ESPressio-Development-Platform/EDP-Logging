# Bounded transitions on the existing Logging queue

Status: feature/logging-v1, version0.1.0. Host-qualified; the explicit
O2/500ms native managed-worker profile also passes its finite workload.
This extends the existing
TransactionalRecordQueue and its existing ordinary Threading mutex.

## Explicit deadlines and unchanged ownership

CommitUntil, PopUntil, AbortUntil and CompleteUntil take a canonical
Clock::MonotonicTimestamp deadline. Reservation exposes CommitUntil and
AbortUntil; ConsumerClaim exposes CompleteUntil. Original no-wait APIs
remain available. The queue reuses its existing LockWith path and the
same canonical remaining-budget conversion used by source admission.

An expired deadline causes no new transition. A native lock grant at or
after the deadline is released before queue mutation. DeadlineElapsed is
a distinct RecordQueueResult, appended without renumbering old values.
Provider acquisition or release uncertainty still quarantines integrity.
No indefinite or restarted relative wait, new mutex, queue or Memory
facility is introduced.

ProduceRecordUntil uses its original producer-entry deadline for commit
when the reservation offers CommitUntil. A commit timeout retains the
reservation and Memory ownership; Bootstrap must explicitly abort or
reconcile it. Older custom queue contracts keep their no-wait fallback.
Do not destroy an unresolved owner or infer publication from admission.

Retirement uses one absolute deadline across its pre-release and
post-release lock. Native Memory release and capacity notifications stay
outside the queue mutex. If confirmed release finishes but final metadata
locking times out, the stable Retiring slot and empty lease remain owned.
A later explicit CompleteUntil may only finalize; it cannot release that
object again. Confirmed release-notification failure is retained in the
slot across that retry, even if a later capacity notification succeeds.
A new generation resets that notification observation.

Strong reservation/consumer handles carry a stable lease view established
while their generation is exclusively owned. Borrow no longer reads the
shared non-atomic occupancy bitmap concurrently with other producers.
Retirement invalidates that source view. The legacy raw-ticket Borrow
validates under the queue's no-wait mutex and may return no view under
contention; callers still must own the ticket and prevent concurrent
retirement/reuse.

## Existing dedicated coordinator opt-in

Construct the same DedicatedCoordinator with
CoordinatorQueueLockWait<MaximumNanoseconds> as its first argument,
followed by queue, report and sinks. The compile-time bound must be
positive, the queue must supply bounded transitions, and Bootstrap must
bind the canonical Clock before activation. The old constructor retains
legacy no-wait behavior.

The budget applies independently before a source claim and before its
retirement. Synchronous sink execution has its separate application
budget; a new retirement transition does not restart a producer's
admission deadline. Both retirement locks retain the same absolute
deadline. No global bounded sink/shutdown time is claimed.

A finite unclaimed queue-lock timeout is QueueContended, counted in the
atomic report's QueueContentions. The coordinator still services ordinary
sinks, then uses its existing wake/deadline path before checking the queue
again. It neither invokes a sink without a source nor reports corruption
for ordinary contention. An unresolved retirement still stops with the
claim retained; no source callback is replayed.

The private typed delivery acknowledgement helper now returns a dedicated
BatchAcknowledgementStatus enum instead of an operational Boolean.
Existing public completion numeric values are explicit and unchanged.
All modified calls and new owner/template documentation follow V2.

## Appliance evidence and limits

RPI40020261030T163848-bcd7812f passed GCC/Clang UBSan old queue, real Memory
queue and new exact-deadline/late-grant/retained-ownership/release-once tests.
Broader20261030T164414-438a461e passed both compilers: queue, actual Memory,
record production, coordinator service and bounded admission.
20261030T164415-ea06e9da passed GCC/Clang UBSan typed Memory/Task/VFS batch
service, legacy strict ACK delivery and sealed batching.
The final notification-preservation and distinct-owner contention tests
run separately after those checkpoints; see the living handoff for their
exact final outcome and subsequent native results.

Earlier physical runs161030-bcd6a9dd and162046-198ff12f remain FAIL and
preserved. Their precise phase1 Commit Busy3 with active reservation
motivated this correction. Native idle/no-new-input, scheduled ingestion,
raw media, CPU/resource and remaining reliability gates are not closed
by host tests. No GitHub Actions coverage is relied upon.

Final notification-preservation and distinct-thread mutex-contention tests
passed GCC and Clang with UBSan in20261030T164857-8d454bf9. Actual native
managed fixture selecting CoordinatorQueueLockWait<5000000> builds cleanly
in20261030T165014-930d07b6. Later physical results are recorded below.


The explicit O2/500ms/160MHz managed-worker run20261030T180038-622f5b3b
passes finite pressure128/32, idle partial flush and2000 scheduled sources
offered at100records/s, followed by complete durable drain/teardown and
independent actual-media/NVS audit. QueueContentions observed0 in that run;
host distinct-owner tests remain the deliberate finite-lock-contention proof.
Earlier native Commit Busy failures and later Og capacity failures remain
unchanged. See native README for exact configuration and limits.
