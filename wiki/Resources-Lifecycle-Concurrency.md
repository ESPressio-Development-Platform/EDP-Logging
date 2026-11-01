# Memory, concurrency and lifecycle

Application Bootstrap fixes argument sizes, producer/sink topology, logical retained byte limit, batching size, Threading providers and storage medium before first initialization. Memory leases are move-only and may not be reclaimed before final sink use.

## Boundaries

A coordinated shutdown must cancel admission, drain or reject pending work, preserve protected EDLS predecessors, record uncertain provider outcomes, and join the owning Tasks. The five-minute 100-rps trial is finite workload evidence, not indefinite or lifetime certification.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.
