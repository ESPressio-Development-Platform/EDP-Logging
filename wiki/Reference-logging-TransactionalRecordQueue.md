# TransactionalRecordQueue (G1 partial)

**Header:** `<logging/TransactionalRecordQueue.hpp>`
**Namespace:** `ESPressio::Logging`
**Branch:** `feature/logging-v1`
**Package version:** `0.1.0`

`TransactionalRecordQueue<Capacity, Lease, MutexProvider, ReleaseCoordinator, Generation>` provides allocation-free, stable queue slots retaining real move-only **EDP-Memory** leases. Bootstrap selects a keyed **EDP-Threading** ordinary mutex and a coordinator that delegates capacity wake/release to existing **EDP-Threading** bounded wait slots. **EDP-BoundedTopology** supplies the authoritative one-bit occupancy and intrusive FIFO; Logging supplies only the record transaction/lifecycle semantics.

**Operations:** `Begin(lease,ticket)` transfers a pre-acquired Memory lease into an unpublished slot; `Commit(ticket)` enters commit-order FIFO; `Abort(ticket)` releases an unpublished claim; `Pop(ticket)` retrieves oldest committed record; `Borrow(ticket)` borrows the object while exclusively Reserved or InFlight; `Complete(ticket)` returns Memory capacity then signals queue-slot availability. Tickets contain a compact index and monotone non-wrapping generation.

The owner distinguishes Busy mutex, no record, capacity exhaustion, stale token, invalid phase, unconfirmed native lease release, failed wake after actual release, and fail-closed mutex integrity failure. A per-slot retirement claim prevents duplicate ticket holders from concurrently releasing the same Memory lease; partial failure leaves stable ownership retained rather than silently returning a slot.

**Important:** This is an optional **partial implementation**, not a complete Logging producer. The client still owns the admission deadline, is required to hold exclusive Borrow interest, and must explicitly Abort/Complete before destroying the queue. The future move-only RAII publication guard, bounded Memory+queue end-to-end producer, governed LogRecord TypeIdentifier, sinks, historical durable archive and physical ESP32 qualification remain pending. Unsafe native `EDP-Memory::Acquire(Finite/Forever)` is NOT used.

[Detailed contract and tests](../docs/TRANSACTIONAL-RECORD-QUEUE.MD) | [Dependency graph](../DEPENDENCY_GRAPH.MD) | [Authoritative living handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-10-15/2026-10-15-11-46-EDP-Logging-Architecture-Living-Handoff.MD)
