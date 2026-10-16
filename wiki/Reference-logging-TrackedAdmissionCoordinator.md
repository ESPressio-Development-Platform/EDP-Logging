# logging/TrackedAdmissionCoordinator.hpp — optional G1 tracked lease ownership

**PUBLIC opt-in header**, version `0.1.0`; same EDP-Clock/Memory/Threading/Platform abstraction graph as the prior G1 coordinators, no new concrete or reverse EDP dependency. Not imported by the default Logging umbrella.

**Types and methods**

- `TrackedAdmissionValidation : uint8_t`: `Valid`, `WrongCoordinator`, `LeaseOccupied` (rejection never touches the pool).
- `TrackedAdmissionResult`: `Validation`, native `CoordinatedAdmissionResult` preserving truthful `Admitted`, `GrantedAfterDeadline`, `GrantedDuringShutdown` and separate capacity-notification health.
- `TrackedAdmissionCoordinator<ProducerCount,WaitWakeProvider>`: noncopyable/movable, borrows Bootstrap-selected EDP-Threading provider with **ProducerCount+1** dedicated stable slots. Internal lock-free counters track *outer acquire transactions*, *successfully granted and not positively released leases*, *unconfirmed native destructor releases* and *failed wake publications*. One shutdown-drain caller; no heap or new Task.
- `TrackedAdmissionCoordinator::TrackedLease<TNativeLease>(coordinator)`: noncopyable/movable RAII owner holding an entirely private EDP-Memory ObjectPoolLease. Consumer gets `Get()`, `IsEmpty()`, `Release()`; not raw lease ownership. Explicit `Release()` and scope destructor use **real Memory ReleaseAndNotify**. A native release failure during guard destruction becomes sticky **unconfirmed**, with a trailing best-effort wake AFTER the unobservable underlying native lease destructor's retry.
- `AcquireUntil<MaxWakeRetries>(slot,pool,guard,oneMonotonicDeadline,args...)`: validates ownership, uses existing cancellation-aware Memory **NoWait** + Threading WaitUntil, retains leases on every late/shutdown racing grant and increments tracked outstanding before outer acquire exits; returns strong validation and admission.
- `ShutdownTrackedUntil<MaxDrainWakes>(oneMonotonicDeadline)`: event-drains both active acquires and counted *tracked* leases, returns `TrackedDrainResult` with `Quiescent/DeadlineElapsed/WakeRetryLimitReached/DrainWaitProviderFailure/UnconfirmedNativeRelease` and numeric outstanding/active/unconfirmed/wake-failure evidence. A native RAII release error can **never** be called clean quiescence. `RequestShutdown()`, `OutstandingLeases()`, `UnconfirmedReleases()` are ordinary-context observable owner methods.

**Not guaranteed:** No fair transactional producer FIFO, timeout bound for every provider call, host delivery, automatic task join, untracked raw lease accounting, real ESP32 Physical/FreeRTOS timing, LogRecord typed identity or archive. A tracked clean drain is *not* proof of all app resource teardown: Bootstrap must stop issuing new calls, join tasks and account for every separately owned resource.

[Full implementation contract](../docs/TRACKED-ADMISSION-LEASES.MD) • [Dependency Graph](../DEPENDENCY_GRAPH.MD) • [Existing G1 cancellation/lifecycle](Reference-logging-AdmissionLifecycleCoordinator).

**Host tests:** `tests/TrackedAdmissionCoordinatorTests.cpp` uses actual EDP-Memory/TestSignal owners. GCC/Clang PASS RPI400 run `20261016T085118-8d57dc8a` including automatic destructor wake, capacity saturation/release, wrong owner, shutdown while lease remains, native release failure and conservative nonquiescence. Target/HIL not proven.
