# Memory, concurrency and lifecycle

Application Bootstrap fixes argument sizes, producer/sink topology, logical retained byte limit, batching size, Threading providers and storage medium before first initialization. Memory leases are move-only and may not be reclaimed before final sink use.

## Boundaries

A coordinated shutdown must cancel admission, drain or reject pending work, preserve protected EDLS predecessors, record uncertain provider outcomes, and join the owning Tasks. The five-minute 100-rps trial is finite workload evidence, not indefinite or lifetime certification.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.

DedicatedCoordinator can borrow a Bootstrap-owned ConsumerClaim after its report argument. Keep that claim and its queue/providers alive through the binding. After execution quiesces, Bootstrap can use CompleteUntil to reconcile retirement without invoking sinks again. Queue mutex waits consume the supplied deadline. Memory release and advisory wake calls keep their separate provider contracts. Preserve failed ownership and the original delivery evidence until reconciliation succeeds. See [Dedicated coordinator](Reference-logging-DedicatedCoordinator).
