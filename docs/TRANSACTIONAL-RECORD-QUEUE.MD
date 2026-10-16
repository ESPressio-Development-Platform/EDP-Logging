# Logging transactional record FIFO — initial G1 owner implementation

**Branch:** `EDP-Logging/feature/logging-v1`. **Version:** `0.1.0`.
**Status:** implemented partial producer-record publication primitive, **NOT full end-to-end Logging G1/G2–G15**.

## Ownership and reuse

`TransactionalRecordQueue<N,TLease,TMutexProvider,TReleaseCoordinator,TGeneration>` accepts an **already acquired** move-only `EDP-Memory::ObjectPoolLease` through `Begin`; it does **not** allocate records, duplicate an object pool, or implement a second memory backend. The record remains owned until Abort or Complete returns a verified release result.

- `EDP-BoundedTopology::BoundedSlotTopology` owns exactly one bit of slot occupancy per configured index.
- `EDP-BoundedTopology::IntrusiveQueue` owns the FIFO endpoints and uses the externally owned record's one compact Next index. FIFO order is **commit order**, not attempt ticket order or producer arrival order.
- `EDP-Threading::OrdinaryMutexProvider` is Bootstrap-supplied; operations make **nonblocking** TryAcquire attempts and never fall into implicit Forever waits. A contended lock returns `Busy`, and a failing mutex latches `IntegrityFailure` at the queue.
- The existing `AdmissionWakeCoordinator` supplies `ReleaseAndNotify` and `PublishCapacityAvailable`, which delegate to the original EDP-Threading bounded wake slots. No private Signal or clock. Native Memory release and wake callbacks are never invoked while the queue mutex is held.

The record queue does **not** replace the upstream Logging `AcquirePoolUntil`/TrackedAdmission seam or the EDP-Memory native ObjectPool. It provides the previously missing resource-specific transaction/ordering layer. Bootstrap must compose capacity profiles and lifetime order. Do **not** call unsafe native `EDP-Memory::Acquire(Finite/Forever)` (open issue #16).

## API and lifecycle

1. A producer obtains one valid EDP-Memory lease through the existing bounded admission mechanism.
2. `Begin(lease,ticket)` claims one stable slot, transfers lease ownership, and publishes its generation-bearing ticket; if no slot is available, the original lease remains with the caller and must be returned via the existing release coordinator.
3. `Borrow(ticket)` is valid only while the producer has an **exclusive Reserved** ticket (caller must not share this borrowed reference concurrently).
4. `Commit(ticket)` appends exactly once to `IntrusiveQueue` and changes the slot to Queued, or `Abort(ticket)` retires an unpublished reservation with a confirmed Memory lease release.
5. A consumer `Pop(ticket)` claims the *oldest committed* record and transitions it to InFlight. The consuming ticket alone may `Borrow` the object until the final source reader finishes.
6. `Complete(ticket)` returns the native Memory lease through the existing coordinator, retires the stable slot, then publishes a separate capacity wake **after** that slot becomes reusable. `ReleasedNotificationFailed` means the lease/slot were returned but a required notification failed.
7. Generation tokens prevent old tickets from claiming a recycled slot. Generations never wrap: an exhausted slot becomes permanently exhausted until a new runtime is constructed.
8. The caller must **not destroy the queue** while a record is Reserved, Queued, InFlight, Retiring or release-uncertain. The destructor terminates in those invalid lifecycle states instead of silently dropping a live native lease without its required notification.

The separate `RetirementClaimed` flag prevents concurrent duplicate `Complete`/`Abort` invocations with the same copied ticket from releasing a native Memory lease twice. An unconfirmed provider release **retains** the stable Retiring slot, and a later retry can be attempted if the native lease is still visibly owned. A provider that reports failure while ownership inexplicably disappears is treated as integrity failure, not proof of a clean release.

**Move-only RAII ownership now available:** `Begin(lease, Reservation&)` establishes one exclusive `Reservation`. The guard is noncopyable, move-constructible, exposes `Borrow()` and `Commit()/Abort()`, and automatically aborts the native Memory lease if unpublished when its scope ends. Similarly `Pop(ConsumerClaim&)` establishes a noncopyable, move-constructible final-reader `ConsumerClaim` which automatically invokes `Complete()` on scope exit. A failed automatic release marks the queue's integrity failed rather than fabricating clean shutdown. Earlier raw `Ticket` overloads remain an explicitly lower-level interface whose caller must enforce exclusive ownership; the RAII overloads are preferred for new consumers.

**Current deliberate limitations:** This is a composable core, not the complete producer interface. Queue capacity starvation is signalled as `CapacityUnavailable` and has not yet been folded into one end-to-end canonical Clock deadline for the entire producer transaction. Queue `Busy` requires caller scheduling; no implicit spin or waiting. Single-actor consumption and forked sink reader lifetimes remain the higher-level coordinator's responsibility. A sticky integrity failure requires refusing operation/teardown, not guessing lock ownership.

## Validation

The repository's `tests/run-transactional-record-queue-tests.sh` runs with `EDP_LOGGING_DEPENDENCY_ROOT` pointing to exact pinned independent source siblings:

- `TransactionalRecordQueueTests.cpp`: FIFO commit order, owned lease transfer, empty output, full capacity, precommit abort, stale generation, duplicate commit rejection, release/notify failures, retry of unconfirmed release, mutex busy and provider failure, unconfirmed integrity, one-byte capacity-derived compact index.
- `TransactionalRecordQueueMemoryTests.cpp`: actual EDP-Memory `MemoryRuntime` and ObjectPool lease lifecycle, actual Bootstrap-composed EDP-Threading Mutex + BoundedWaitWake coordinator, teardown blocked by outstanding records, FIFO with real object values, complete source-object destruction, concurrent duplicate-ticket release claim interleaving. The test imports and runs all prerequisite EDP-Memory/Threading wake test fixture cases.

Tests run with `g++` and `clang++` on RPI400 in mandatory persisted detached runs. See the up-to-date Policy/Main Logging living handoff for exact SHA, run ID and all validation claims. **No device/flash validation is claimed by these host tests.**

## Architectural next work

- Expand the new move-only RAII Reservation/ConsumerClaim into the full composer-level producer and sink reader lifetimes, with explicit failure propagation for automatic unwinding and no borrowed references escaping the last owner.
- Compose queue-slot and Memory-capacity admission under one canonical EDP-Clock deadline without duplicated Threading wake slots; comprehensive lost-wake/fairness/producer cancellation tests.
- Complete LogRecord provenance/argument model, generated diagnostic schemas, governed permanent root TypeIdentifier, 1:1 consumer/raw-record ownership and separate encoded-buffer sink leases.
- PlatformIO ESP32-WROOM and ESP32-S3 footprints and physical producer/consumer HIL, deterministic mutex/notify/memory/power fault tests.
- G2 Clock/Mesh coherent revision and G4 TypeIdentifier governance remain separate owner-level dependencies. All versions remain `0.1.0`.
