# TransactionalRecordQueue (G1 partial)

**Header:** `<logging/TransactionalRecordQueue.hpp>`
**Namespace:** `ESPressio::Logging`
**Branch:** `feature/logging-v1`
**Package version:** `0.1.0`

`TransactionalRecordQueue<Capacity, Lease, MutexProvider, ReleaseCoordinator, Generation>` provides allocation-free, stable queue slots retaining real move-only **EDP-Memory** leases. Bootstrap selects a keyed **EDP-Threading** ordinary mutex and a coordinator that delegates capacity wake/release to existing **EDP-Threading** bounded wait slots. **EDP-BoundedTopology** supplies the authoritative one-bit occupancy and intrusive FIFO; Logging supplies only the record transaction/lifecycle semantics.

**Operations:** `Begin(lease,ticket)` transfers a pre-acquired Memory lease into an unpublished slot; `Commit(ticket)` enters commit-order FIFO; `Abort(ticket)` releases an unpublished claim; `Pop(ticket)` retrieves oldest committed record; `Borrow(ticket)` borrows the object while exclusively Reserved or InFlight; `Complete(ticket)` returns Memory capacity then signals queue-slot availability. Tickets contain a compact index and monotone non-wrapping generation.

The owner distinguishes Busy mutex, no record, capacity exhaustion, stale token, invalid phase, unconfirmed native lease release, failed wake after actual release, and fail-closed mutex integrity failure. A per-slot retirement claim prevents duplicate ticket holders from concurrently releasing the same Memory lease; partial failure leaves stable ownership retained rather than silently returning a slot.

**Move-only RAII handles:** Use `Begin(lease, Reservation&)` to acquire an unpublished `Reservation` that is automatically aborted unless committed; use `Pop(ConsumerClaim&)` to acquire a consumer-owned record that is automatically completed/released at scope exit. Both are noncopyable and move-constructible. Automatic release failure latches queue integrity failure rather than pretending the native lease was returned. The raw `Ticket` overloads are a lower-level interface requiring caller-managed exclusive interest.

**Important:** This header is a **composable queue primitive**, not an automatically configured Logging runtime. The application supplies the admission deadline and must retain exclusive borrow interest until the final consumer; ordinary RAII scope exit is supported, but a failed native release never justifies destroying live or uncertain owners. Separate, already implemented RecordProduction, generated LogRecord, coordinator/sink and strict EDLS modules compose the end-to-end pipeline, with finite physical WROOM proof documented in the G1–G15 matrix. These results do not give this low-level header an implicit producer/worker or universal storage guarantee. Unsafe native `EDP-Memory::Acquire(Finite/Forever)` is NOT used.

[Detailed contract and tests](../docs/TRANSACTIONAL-RECORD-QUEUE.MD) | [Dependency graph](../DEPENDENCY_GRAPH.MD) | [Authoritative living handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-10-15/2026-10-15-11-46-EDP-Logging-Architecture-Living-Handoff.MD)

## Pre-Memory reservation surface

ReserveSlot(Reservation&, WaitTimeout) establishes move-only RAII ownership of a fixed queue entitlement in the new AwaitingMemory state. AttachLease(Reservation&, TLease&, WaitTimeout) transfers EDP-Memory ownership into that entitlement before Commit. WaitTimeout is calculated externally from one canonical EDP-Clock deadline; neither operation owns a Clock. Empty AwaitingMemory Abort reclaims a slot and wakes the preexisting EDP-Threading producer slots without native Memory release. The [composed bounded admission owner](Reference-logging-BoundedRecordAdmission) uses both operations and retains all original FIFO/RAII/provider failure semantics.

## 1 November 2026 V2 predicate names

Private ticket/slot state predicates are now named `IsMatchingTicket` and `IsPhaseLive`; their Boolean semantics and queue generation, release, and deadline ordering have not changed. No old spellings or compatibility aliases remain, in accordance with the approved V2 breaking-source correction.


## Source-level type, state and resource reference

The C++20 template `TransactionalRecordQueue<TCapacity,TLease,TMutexProvider,
TReleaseCoordinator,TGeneration>` accepts a positive compile-time slot bound,
an existing move-only EDP-Memory lease, a Bootstrap-owned EDP-Threading
ordinary mutex, a coordinating release/wake provider and an unsigned,
nonwrapping generation representation. Its `Capacity` constant exposes
the configured active slot limit; `StorageBytes` reports
`sizeof(TransactionalRecordQueue)` for compile-time product budgeting.
Neither constant includes the application's Memory object-pool storage,
provider-internal thread resources or caller stack.

**`RecordQueueResult`** is an explicitly numbered `std::uint8_t`
result, not a Boolean admission outcome: `Succeeded=0`, `Empty=1`,
`CapacityUnavailable=2`, `Busy=3`, `InvalidTicket=4`,
`WrongPhase=5`, `OutputLeaseEmpty=6`, `GenerationExhausted=7`,
`ReleasedNotificationFailed=8`, `ReleaseUnconfirmed=9`,
`IntegrityFailure=10`, `OutputHandleOccupied=11`, and
`DeadlineElapsed=12`. In particular, a native release that succeeded
but whose wake notification failed is distinct from an unconfirmed
Memory lease release; only the former permits retiring the slot
without pretending that the producer was awakened.

### Nested ownership and topology

`Index` names the compact index in this queue's owner-specific topology. A `Ticket` carries that slot and the generation assigned to its reservation. `Ticket::IsValid()` checks the shape only (a valid index and a nonzero generation). Queue transitions separately verify the stored generation and allowed phase while holding the queue owner. A stale ticket can pass the shape check.

**`RecordIndexSpace`**, `Topology`, `Queue` and `Index`
are bindings to the existing EDP-BoundedTopology owner, not local
second implementations of a slot bitmap or FIFO.
The owner rejects stale generations when a physical slot is reused. `Slot` defaults to the invalid sentinel and `Generation` defaults to zero.

The per-slot private `Record` groups `Next` (intrusive FIFO link),
`Lease` (the actual move-only Memory owner), `Generation`,
`State` (a typed `Phase`), atomic `RetirementClaimed`
(the one-retirer guard), and `RetirementNotificationFailed`
(the release/wake discrepancy retained across retries).
`QueueNext()` and `SetQueueNext(next)` are the bounded topology's
intrusive-link accessors; they do not acquire a second allocation.

The `Phase` ordinals are `Vacant=0`, `AwaitingMemory=1`,
`Reserved=2`, `Queued=3`, `InFlight=4`,
`Retiring=5`, and `Exhausted=6`. The last is an
irreversible generation exhaustion, not a temporarily busy slot.
The private `UnlockStatus` distinguishes `Released=0` from
`Unconfirmed=1`; an unconfirmed native mutex release quarantines
the queue rather than allowing further optimistic mutation.

The owning queue holds only nonowning `_mutex` and `_releases`
pointers, fixed `_slots` occupancy topology, fixed `_fifo`
links, a statically bounded `_records` array, and atomic
`_integrityFailed` quarantine status. Bootstrap must keep providers
alive longer than the queue and must drain all nonvacant phases before
destroying it; the destructor calls `std::terminate` rather than
silently leaking an active native Memory lease.

### Reservation and ConsumerClaim lifetime

Both RAII handles are move-constructible but noncopyable; move
assignment is deliberately deleted. Each records the nonowning
`_owner` pointer, exclusive `_ticket` and stable
`_borrowedLease` view, without taking an additional Memory lease.

- **`Reservation`** represents an unpublished admission.
  `IsActive()` checks the retained queue owner;
  `Borrow()` returns the lease only while active and the queue
  is not quarantined. `Commit()` and `CommitUntil(deadline)`
  publish the ticket exactly once, then disarm the RAII owner on
  confirmed success; otherwise the reservation remains held.
  `Abort()` and `AbortUntil(deadline)` attempt native release
  and slot retirement, disarming on `Succeeded` or the already
  released `ReleasedNotificationFailed`. The private
  `AbandonOnScopeExit()`, called by the destructor, refuses to
  treat uncertain native release as successful teardown and latches
  `_integrityFailed`.
- **`ConsumerClaim`** represents exclusive interest after the
  FIFO pop. `IsActive()` and `Borrow()` have the same stable,
  non-owning handle semantics. `Complete()` and
  `CompleteUntil(deadline)` release the source lease after its last
  reader and disarm only after confirmed native retirement; the
  private `CompleteOnScopeExit()` invokes that finalization on
  destruction and quarantines unconfirmed failures.

A caller must not transfer a borrowed `TLease*` to an unrelated
concurrent consumer without a separate ownership protocol. All
record and handle lifetimes are bounded by the queue and its
Bootstrap-owned providers.

### Private synchronization and transition helpers

`Lock()` is a no-wait native mutex attempt: an ordinary timeout is
`Busy`; a provider fault or previously latched integrity fault is
`IntegrityFailure`. `Unlock()` confirms native release and
quarantines a nonconfirmed mutex unlock. `LockWith(remaining)`
uses an already-computed EDP-Platform wait budget and
`LockUntil(deadline)` uses the *same original* EDP-Clock monotonic
deadline, refusing a grant observed after expiration.
`UnlockWith(outcome)` returns the intended result only after
confirmed unlock, otherwise an integrity failure.

`ClaimSlot(initial,ticket)` consumes a fixed BoundedTopology
slot, advances a never-rewinding generation and places the record
into `AwaitingMemory` or `Reserved`. `IsMatchingTicket(ticket)`
requires occupied slot and exact generation; `IsPhaseLive(phase)`
classifies a still-owned source for lifecycle validation.
`CommitUnderPolicy(ticket,deadline)` is the single
commit-order FIFO-publishing path (bounded or no-wait).
`PopUnderPolicy(ticket,deadline)` removes exactly the oldest
queued ticket, transitioning to `InFlight` under the same mutex.

`Retire(ticket,expected,deadline)` is the release-once
transaction used by abort and completion. It claims retirement
under the mutex, releases the native Memory owner *outside*
that mutex, reacquires without restarting the canonical deadline,
frees the slot only when release is confirmed, and publishes the
separate queue-capacity wake *after* slot retirement. A timeout
during finalization keeps sufficient ownership state for a safe
later continuation; no duplicate release is authorised.

### Public queue transition API

- **`Begin(lease,ticket)`** transfers an already-acquired
  nonempty Memory lease into one unpublished `Reserved` slot and
  returns the exclusive ticket; on failure before transfer the caller
  retains the lease. `Begin(lease,Reservation&)` installs an
  inactive RAII output handle only when ownership actually moved.
- **`ReserveSlot(out,mutexRemaining)`** reserves
  `AwaitingMemory` queue capacity before potentially slow
  Memory-pool acquisition. `AttachLease(out,lease,mutexRemaining)`
  binds a real owner lease later, using the same original deadline
  budget; neither operation restarts the application's Clock.
- **`Commit(ticket)`** and **`CommitUntil(ticket,deadline)`**
  place an unpublished reserved source in FIFO exactly once.
  **`Abort(ticket)`** and **`AbortUntil(ticket,deadline)`**
  reclaim an unpublished source through the same guarded retirement
  machinery.
- **`Pop(Ticket&)`** and **`PopUntil(Ticket&,deadline)`**
  claim the oldest committed source. Their
  **`Pop(ConsumerClaim&)`** and
  **`PopUntil(ConsumerClaim&,deadline)`** overloads create
  exclusive move-only consumer handles with scope cleanup.
- **`Borrow(ticket)`** returns `BorrowResult`. `Status()` reports
  `Succeeded`, `Busy`, `InvalidTicket`, `WrongPhase` or `IntegrityFailure`.
  `Lease()` exposes a non-owning pointer only for a confirmed success.
  The matching ticket must be `Reserved` or `InFlight`. The caller keeps
  the claim live and serializes access to the lease. An unconfirmed mutex
  release overrides the lookup outcome with `IntegrityFailure`.
  **`Complete(ticket)`** and
  **`CompleteUntil(ticket,deadline)`** finalize the last
  consumer and preserve retirement correctness if lock acquisition
  runs out of time.
- **`IsIntegrityFailed()`** exposes the latched fail-closed
  state so Bootstrap can avoid unsafe teardown or further admission.

These low-level typed transitions do not replace the higher-level
bounded admission coordinator, the actual Memory object pool,
the worker/sink scheduling, or the Persistence/Serial integration.
The queue itself never promises durable bytes, external signal
delivery or fairness beyond the specific bounded contracts
qualified by its tests.
