# Reference — `src/logging/CoordinatorSinkService.hpp`

**Classification:** PUBLIC OPTIONAL coordinator service callback contract; C++20 concept. [Exact source](../src/logging/CoordinatorSinkService.hpp). Direct abstract dependency is **EDP-Clock** for the shared monotonic origin. This source creates **no** new worker, task, queue, filesystem, timer, allocator or provider. The callback belongs to a sink object already owned by Bootstrap.

## Public `CoordinatorSinkServiceStatus` enum

**Ready (0)** — the sink's ordinary synchronous coordinator service succeeded and the dedicated worker may continue. **RecoveryRequired (1)** — a pending write/acknowledgement has uncertain or failed state; the worker must terminate and Bootstrap must reconcile it explicitly. RecoveryRequired is **not** retry, timeout, successful admission, log record retirement or a guarantee that bytes were durably stored.

## Public `CoordinatorSinkServiceOutcome` fields

`Status : CoordinatorSinkServiceStatus` defaults to Ready and is the authoritative service decision for **this** sink. `DurableSinkCompletions : uint64_t` counts newly confirmed durable **record completions for this sink only since its prior service poll** (default 0), consumed once. Summing across multiple sinks does *not* produce a distinct-source-record count; a record might have been accepted by one but not another. `NextDeadline : optional<EDP-Clock::MonotonicTimestamp>` is the earliest canonical absolute time at which this sink needs another service to flush an owned bounded batch; empty means no deadline and permits the worker's existing finite idle fallback. Its origin/era must match the Threading Context's Clock reference; callbacks must **not** compute a fresh independent timebase.

The result is a value-only observation and never retains a buffer pointer or a FileStorage lease.

## Public `CoordinatorServiceSink<TSink>` concept

**Template parameter `TSink`** is the existing concrete application-owned ordinary sink callback Type. The concept is satisfied exactly when `sink.ServiceCoordinator()` is callable with no arguments from a mutable `TSink&`, is `noexcept`, and returns precisely `CoordinatorSinkServiceOutcome`. This is an **optional** additional sink capability; ordinary sink callbacks without ServiceCoordinator remain supported and are not polled. The concept does not pick a concrete provider, execute callbacks in an ISR or silently allocate a task.

## Consumer and failure semantics

`DedicatedCoordinator` owns the **single** Threading execution context and invokes eligible service callbacks after every healthy drain quantum (including a consumed budget). It takes the earliest returned NextDeadline across sinks and uses that same context's `WaitUntil` for idle service, or its configured `WaitFor` fallback. RecoveryRequired causes immediate terminal worker report and no implicit retried write. Each returned durable completion increments `DedicatedCoordinatorReport::DurableSinkCompletions` once per sink, separately from RAM admission and source retirement counters; these cannot be conflated. Callback objects, encoded record buffers, FileStorage and NVS watermark retain their original application-selected ownership and lifetime; workers cannot safely preempt a blocking sink callback.

See [normative coordinator sink service](../docs/COORDINATOR-SINK-SERVICE.MD), [DedicatedCoordinator](Reference-logging-DedicatedCoordinator), [Typed Managed Batch Worker](../demos/TypedManagedBatchWorker/README.MD) and [G1–G15 acceptance](../docs/ACCEPTANCE-G1-G15.MD). GCC/Clang/UBSan tests and finite WROOM strict-NVS-ACK integrations are recorded with their exact scope; none certifies every possible sink/provider.
