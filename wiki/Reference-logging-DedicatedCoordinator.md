# Reference — `src/logging/DedicatedCoordinator.hpp`

**Classification:** PUBLIC OPTIONAL composed worker adapter; PRIVATE bounded queue/claim/service machinery. [Exact source](../src/logging/DedicatedCoordinator.hpp). This header directly imports **EDP-Clock** and **EDP-Threading** and uses the sibling coordinator quantum and optional sink service contract. It does not allocate its own worker, task, Clock, queue, semaphore or hidden heap; Bootstrap owns all underlying resources.

## Public `DedicatedCoordinatorExit` enum

**NeverStarted (0)** — no terminal report yet; **CooperativeStop (1)** — existing ThreadContext stop request ended ordinary processing; **ClaimFailed (2)** — source queue claim could not be established; **ReclaimUnconfirmed (3)** — retirement/capacity reclamation was not affirmatively proven; **PriorClaimUnresolved (4)** — a previously outstanding stable claim was not safely settled; **WakeProviderFailure (5)** — topology-owned Threading wake/wait provider failed while no stop was requested; **SinkServiceRecoveryRequired (6)** — one opted-in sink reported uncertain or failed ordinary service. These are **terminal worker exit reasons**, not per-record durable ACKs. A failure must not be rebranded as orderly drain or restarted without explicit Bootstrap reconciliation.

## Public `DedicatedCoordinatorReport` — every retained state member

This is a shared report object supplied by Bootstrap; **only the Dedicated Thread mutates its atomic fields during activation**, while readers inspect them from other contexts using atomic loads. All counters begin at zero except `Exit`, which begins NeverStarted.

- `Exit : atomic<DedicatedCoordinatorExit>` — terminal state published with release semantics on exit; a consumer should pair it with an acquire observation when interpreting final counts.
- `RecordsRetired : atomic<uint64_t>` — number of source Memory ownership units **confirmed retired** after completed queue transactions.
- `AllSinksAccepted : atomic<uint64_t>` — source records submitted successfully to **every selected callback**; still RAM acceptance, **not** persistence.
- `PartialSubmissions : atomic<uint64_t>` — source records with one or more sink submission failures.
- `NotificationFailures : atomic<uint64_t>` — confirmed Memory releases for which the advisory capacity wake failed; real capacity may be free despite missing notification.
- `IdleTimeouts : atomic<uint64_t>` — count of native ThreadContext idle waits that returned TimedOut, *not* a record or flush count.
- `DurableSinkCompletions : atomic<uint64_t>` — sum of newly **independently verified per-sink** durable record completions, which need not be unique source records.
- `SinkServiceFailures : atomic<uint64_t>` — count of callbacks reporting RecoveryRequired during ordinary worker service.
- `QueueContentions : atomic<uint64_t>` — bounded queue ordinary mutex contentions observed **before** claim or sink invocation, separated from actual claim failure.

No field makes a FileStorage provider intrinsically interruption-atomic, promises fairness forever, or represents an unbounded monitoring history.

## Public compile-time bounded queue policy

`CoordinatorQueueLockWait<TMaximumNanoseconds>` is a small value tag for **explicit opt-in** bounded queue-lock transitions. **Template parameter `TMaximumNanoseconds`** must be positive, and constant `Nanoseconds` exposes exactly the selected per-transition mutex wait interval. This is *not* an end-to-end complete sink callback bound or an alternate Clock; the queue's own `PopUntil`/claim `CompleteUntil` must reuse the canonical monotonic origin and be `noexcept`.

## Public `DedicatedCoordinator<TRecordsPerQuantum,TIdleWaitNanoseconds,TQueue,TSinks...>`

**Template parameters:** `TRecordsPerQuantum` is a strictly positive count of source records before the next service/stop check; `TIdleWaitNanoseconds` is a strictly positive finite idle fallback; `TQueue` is the exact application-owned transactional source queue with its stable `ConsumerClaim` Type; and `TSinks...` is a pack of **one to eight** existing synchronous source-copying callbacks. Each sink may optionally implement the exact noexcept `CoordinatorServiceSink` concept and is statically rejected when its declared service method has the wrong contract. These capacities are compile-time bounds; no hidden growing container is created.

**Private state and invariants:** `_queue : TQueue*` is the stable borrowed queue owner throughout activation; `_sinks : tuple<TSinks*...>` holds stable pointers, not copies, of the app-owned sink callbacks; `_report : DedicatedCoordinatorReport*` borrows Bootstrap's authoritative atomic report; `_claim : TQueue::ConsumerClaim` is the coordinator's **exclusive retained source claim**, deliberately preserved after an uncertain return rather than silently forgotten; and `_queueMutexWaitNanoseconds : int64_t` is zero for the default immediate NoWait queue transition, or the caller's positive bounded-lock budget. Every referenced object and its native provider must outlive the Threading binding and any unresolved claim; it is unsafe to tear down the queue merely because the worker returned.

**Private predicate `SupportsBoundedQueueWait`** is a constexpr compile-time concept check: `TQueue::PopUntil(claim,deadline)` and `claim.CompleteUntil(deadline)` must both be `noexcept` and return `RecordQueueResult`. An application asking for bounded transitions without these operations fails at compilation. **Private `ServiceSinks()`** visits only compile-time eligible service callbacks, adds per-sink newly durable completions to the atomic report, refuses to continue after RecoveryRequired and returns the earliest absolute NextDeadline. It does not extend a previous deadline by later observations; it owns no timer or extra Threading context.

### Constructors, special members and execution call

The ordinary constructor `DedicatedCoordinator(queue,report,sinks...)` borrows the queue, report and callback objects and leaves bounded queue wait disabled (legacy NoWait transitions). The tag constructor `DedicatedCoordinator(CoordinatorQueueLockWait<TMaximumNanoseconds>,queue,report,sinks...)` selects one positive queue-mutex budget and statically requires the queue/claim bounded interface. The adapter is noncopyable and non-copy-assignable; a supported move constructor transfers stable pointers, retained `_claim` and selected budget into the Threading binding. Move assignment is deleted to prevent ambiguous ownership after a claim has escaped.

`operator()(Threading::ThreadContext& context) noexcept` is the *only* execution entry point, called by the already composed Dedicated Thread. While no stop is requested it drains up to `TRecordsPerQuantum` by the existing `DrainCoordinatorQuantum` (or bounded queue-policy variant), updates retired/accepted/partial/notification/queue-contention counters, and **terminates immediately** on ClaimFailed, ReclaimUnconfirmed or PriorClaimUnresolved without destroying the unconfirmed claim. It then services opted-in sinks; RecoveryRequired is terminal, never an implicit retry/erase. On a consumed source budget it loops for another quantum. Otherwise it waits on the **same topology-owned ThreadContext** using the earliest eligible absolute sink deadline via `WaitUntil`, or `WaitFor(TIdleWaitNanoseconds)` when no deadline exists. A timeout increments `IdleTimeouts`; a wake provider failure without an intervening stop exits as WakeProviderFailure. Cooperative stop eventually records CooperativeStop after leaving the loop. This callable does not itself join the thread, drain without limit, or guarantee blocked sink callbacks can be preempted.

## Application lifecycle and qualification

Bootstrap configures and starts the Threading topology, keeps all queue/sink/Clock/claim/provider objects alive, routes `Thread<TIdentity>::Wake()` after work admission, rejects new work before shutdown, requests cooperative stop and uses a bounded JoinFor/JoinUntil policy. It must reconcile any unfinished sink ACK and outstanding queue/claim ownership explicitly. The worker cannot promise whole-application shutdown deadline or indefinite 100-record/sec performance if a synchronous sink blocks outside the application's budget. Source retirement, sink acceptance and strict NVS ACK are **different measurements** and must remain distinct.

GCC/Clang owner tests include healthy quantum, contention, sink readiness, earliest deadline, failure and shutdown, and prior qualified WROOM managed worker/NVS retention profiles prove only their bounded run-specific workloads. See [Dedicated Coordinator contract](../docs/DEDICATED-COORDINATOR.MD), [sink service](Reference-logging-CoordinatorSinkService), [bounded quantum](Reference-logging-CoordinatorQuantum) and [G1–G15 acceptance](../docs/ACCEPTANCE-G1-G15.MD). No claim of generic filesystem power-loss atomicity or flash lifespan is made.

`DedicatedCoordinatorReport::ReclaimFailure` preserves the typed queue result when `Exit` is `ReclaimUnconfirmed`. This diagnostic is reset when the dedicated coordinator starts.
