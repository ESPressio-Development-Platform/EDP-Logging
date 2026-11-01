# TransactionalRecordQueue (G1 partial)

**Header:** `<logging/TransactionalRecordQueue.hpp>`
**Namespace:** `ESPressio::Logging`
**Branch:** `feature/logging-v1`
**Package version:** `0.1.0`

`TransactionalRecordQueue<Capacity, Lease, MutexProvider, ReleaseCoordinator, Generation>` provides allocation-free, stable queue slots retaining real move-only **EDP-Memory** leases. Bootstrap selects a keyed **EDP-Threading** ordinary mutex and a coordinator that delegates capacity wake/release to existing **EDP-Threading** bounded wait slots. **EDP-BoundedTopology** supplies the authoritative one-bit occupancy and intrusive FIFO; Logging supplies only the record transaction/lifecycle semantics.

**Operations:** `Begin(lease,ticket)` transfers a pre-acquired Memory lease into an unpublished slot; `Commit(ticket)` enters commit-order FIFO; `Abort(ticket)` releases an unpublished claim; `Pop(ticket)` retrieves oldest committed record; `Borrow(ticket)` borrows the object while exclusively Reserved or InFlight; `Complete(ticket)` returns Memory capacity then signals queue-slot availability. Tickets contain a compact index and monotone non-wrapping generation.

The owner distinguishes Busy mutex, no record, capacity exhaustion, stale token, invalid phase, unconfirmed native lease release, failed wake after actual release, and fail-closed mutex integrity failure. A per-slot retirement claim prevents duplicate ticket holders from concurrently releasing the same Memory lease; partial failure leaves stable ownership retained rather than silently returning a slot.

**Move-only RAII handles:** Use `Begin(lease, Reservation&)` to acquire an unpublished `Reservation` that is automatically aborted unless committed; use `Pop(ConsumerClaim&)` to acquire a consumer-owned record that is automatically completed/released at scope exit. Both are noncopyable and move-constructible. Automatic release failure latches queue integrity failure rather than pretending the native lease was returned. The raw `Ticket` overloads are a lower-level interface requiring caller-managed exclusive interest.

**Important:** This is an optional **partial implementation**, not a complete Logging producer. The client still owns the admission deadline and must hold exclusive Borrow interest until the final consumer; ordinary RAII scope exit is supported, but a broken native release is not an excuse for destroying the owner with live/uncertain records. The bounded Memory+queue end-to-end producer, governed LogRecord TypeIdentifier, sinks, historical durable archive and physical ESP32 qualification remain pending. Unsafe native `EDP-Memory::Acquire(Finite/Forever)` is NOT used.

[Detailed contract and tests](../docs/TRANSACTIONAL-RECORD-QUEUE.MD) | [Dependency graph](../DEPENDENCY_GRAPH.MD) | [Authoritative living handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-10-15/2026-10-15-11-46-EDP-Logging-Architecture-Living-Handoff.MD)

## Pre-Memory reservation surface

ReserveSlot(Reservation&, WaitTimeout) establishes move-only RAII ownership of a fixed queue entitlement in the new AwaitingMemory state. AttachLease(Reservation&, TLease&, WaitTimeout) transfers EDP-Memory ownership into that entitlement before Commit. WaitTimeout is calculated externally from one canonical EDP-Clock deadline; neither operation owns a Clock. Empty AwaitingMemory Abort reclaims a slot and wakes the preexisting EDP-Threading producer slots without native Memory release. The [composed bounded admission owner](Reference-logging-BoundedRecordAdmission) uses both operations and retains all original FIFO/RAII/provider failure semantics.

## 1 November 2026 V2 predicate names

Private ticket/slot state predicates are now named `IsMatchingTicket` and `IsPhaseLive`; their Boolean semantics and queue generation, release, and deadline ordering have not changed. No old spellings or compatibility aliases remain, in accordance with the approved V2 breaking-source correction.
