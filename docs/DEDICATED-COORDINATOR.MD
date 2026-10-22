# Dedicated Logging coordinator — topology-owned G10 runtime

**2026-10-17 status:** opt-in compiled C++20 callable and physical HIL
integration candidate on `feature/logging-v1`. This is the
application-directed EDP-Threading execution substrate; the end-to-end
Logging product, governed LogRecord wire, format fork, sinks and
power-loss-proof retention are separate gates.

## Bootstrap topology and ownership

Define one EDP-Threading `DedicatedThread<LoggingCoordinatorIdentity,
StackCapacity<N>, Priority<...>, Affinity<...>>` in the application's
`ThreadingTopology`. Bind exactly one movable
`Logging::DedicatedCoordinator<MaximumRecordsPerQuantum,
IdleWaitNanoseconds,Queue,SinkA,SinkB,...>` callable using
`Threading::BindDedicatedThread<LoggingCoordinatorIdentity>`.
Build one `StaticThreadingRuntime` using the actual Platform execution
context, mutex, spinlock and signal owner selected at Bootstrap.

The selected record queue remains an EDP-Memory
`TransactionalRecordQueue` over EDP-BoundedTopology; the
coordinator MUST NOT allocate a competing task queue, work pool,
timer, mutex, signal or execution context. Its object holds pointers to
the already-constructed Queue, 1..8 synchronous static sink objects,
and a caller-owned atomic `DedicatedCoordinatorReport`. Bootstrap
must retain them **through successful stopped execution and final
release of any active ConsumerClaim**, before tearing down the
Memory Runtime or its Queue.

Bootstrap MUST initialise and start the EDP-Threading runtime before
`ThreadHandle<Identity>().Start()`. Once a Record is committed, the
caller can publish one advisory wake using that exact EDP-owned
`Thread::Wake()`. The coordinator drains finite compile-time
`MaximumRecordsPerQuantum` batches with
`DrainCoordinatorQuantum`, inspecting the canonical Queue again
after every full budget. On QueueEmpty it calls the topology-owned
`ThreadContext::WaitFor` for a positive finite physical duration;
timeout rechecks actual Queue membership, covering coalesced/lost
notifications without inventing a second wake or waiter registry.
Each synchronous sink call must meet the application-specified
maximum blocking budget; Logging cannot preempt an uncooperative
provider.

For stop, first stop new producers, quiesce admissions to the selected
finite deadline, choose explicit queue backlog/durability disposition,
then request the EDP-Threading Thread stop and wake it. **Only a caller
already inside a managed EDP-Threading execution context may use
`Thread::JoinFor/JoinUntil`**. An external/unmanaged ESP-IDF
`app_main` Bootstrap must instead use one finite canonical-time
budget to check `StaticThreadingRuntime::IsExecutionQuiescent()`;
`Thread::JoinFor` must fail as `Interrupted` for that caller. This is
not permission to register an extra context or unmanaged join waiter.
Inspect the terminal atomic report before finalization. Only once
all memory claims and pending sink buffers are released may the
application invoke `BeginShutdown` and `FinalizeShutdown`. A stop
request NEVER implies all records became durable or even submitted;
the caller retains an explicit backlog policy.

## Outcomes and failure preservation

`DedicatedCoordinatorExit`:
`NeverStarted`, `CooperativeStop`, `ClaimFailed`,
`ReclaimUnconfirmed`, `PriorClaimUnresolved`,
`WakeProviderFailure`. The report also counts records retired,
records accepted by all sinks (submission only), partial submissions,
release-notification failures and idle timeout observations, all
`std::atomic` values. A failed native reclaim may still leave an
active caller-owned claim. Bootstrap MUST NOT destroy the Queue or
worker resources in that state; freeze topology, retain the claim and
request explicit owner-side recovery/abort. It cannot force reset
or silently turn provider uncertainty into successful reclamation.

## Invariants and exclusions

- Logging remains Mesh-independent; unsynchronised monotonic samples
  remain valid.
- Bounded record count per quantum; total active coordinator duration
  is governed by the EDP-Threading cooperative stop and native
  provider wake contract, not an invented while-loop timeout.
- No raw Thread, pthread or FreeRTOS calls in Logging's public header.
- No dynamic allocation or extra worker topology is introduced.
- A successful sink submission means only **accepted**, not durable or
  physically transmitted, and is counted independently for each sink.
- Source Memory is reclaimed exactly once after synchronous fanout;
  any async encoded payload requires its OWN bounded buffer lease
  before this callback returns (not implemented here).
- G4 Field 11/14 still lack approved bit vocabularies. This routine
  cannot authorise full typed LogRecord CBOR/JSON or issue those bits.

## Tests

`tests/RecordProductionTests.cpp` uses the actual
`EDP-Threading::ThreadContext` callback ABI with deterministic
simulated native wake and stop, EDP-Memory-backed records, two sinks,
rejected delivery, finite one-record quantum and report checks;
compile-time `BindDedicatedThread<Identity>` type checks enforce
topology compatibility. GCC, Clang and UBSan expected.

The ESP-IDF WROOM HIL integration uses actual
`Threading::StaticThreadingRuntime` selected with
`EDP-Platform-ESP-IDF::ExecutionContextProvider`,
native EDP-FreeRTOS Signal/Mutex and EDP-ESP-IDF SpinLock.
Physical qualification requires real dual-core execution, completion
counts, wake, cooperative stop, finite **quiescence from unmanaged
Bootstrap**, and final native shutdown to succeed. A positive
`JoinFor` qualification additionally requires an already managed
Threading context; it must never be asserted from `app_main`. Merely building this harness does NOT provide
physical proof; separate UART captures and original-flash restoration
are mandatory before reporting that gate closed.

## Single-sink coordinator contract (2026-10-22, #10)

The record fanout, bounded quantum and EDP-Threading dedicated coordinator
accept **1–8** statically selected synchronous copying/consuming sink
callbacks, consistently with the existing G10 representation fanout.
Zero sinks remain a compile-time error. Exactly one sink requires no
dummy second sink, introduces no additional worker, queue, memory owner
or asynchronous borrowed lifetime, and retains the same per-sink
Accepted/Rejected/Partial/ProviderFailure evidence and source-lease
reclamation contract. Host coverage uses one sole sink across direct
drain, finite quantum and DedicatedCoordinator stop/wake. No durability
or physical-transport claim follows from sink acceptance.
