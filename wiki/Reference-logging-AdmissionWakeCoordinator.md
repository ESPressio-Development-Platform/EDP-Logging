# logging/AdmissionWakeCoordinator.hpp — composed G1 wake fanout

**Partial** application-composed, allocation-free fixed-slot producer notification mechanism. No direct platform concrete dependency or default Logging umbrella import. See [full owner contract](../docs/ADMISSION-WAKE-COORDINATOR.MD).

## Public types

- `AdmissionWakePublicationResult` (strong `uint8_t`): `NotAttempted=0`, `Published=1`, `ProviderFailure=2`, returned by `PublishCapacityAvailable()` after all configured wake targets are attempted.
- `CoordinatedAdmissionResult` (value Type): `Admission` of `AdmissionWaitResult` and `Notification` of `AdmissionWakePublicationResult`; distinguishes ownership from wake failure.
- `CoordinatedReleaseResult` (strong `uint8_t`): `ReleasedAndNotified=0`, `ReleasedNotificationFailed=1`, `AlreadyEmpty=2`, `MemoryProviderFailure=3`.
- `AdmissionWakeCoordinator<TProducerCount,TWaitWakeProvider>`: borrows one long-lived Bootstrap-composed EDP-Threading BoundedWaitWake owner. No dynamic registration, no coordinator heap allocation, no copy/move. Compile-time asserts `TProducerCount>0` and sufficient provider capacity.

## Methods and internal layout

- `PublishCapacityAvailable() noexcept`: O(`TProducerCount`) wake fanout, fail status preserved even if later slots succeed.
- `ReleaseAndNotify(lease) noexcept`: Memory owner `ObjectPoolLease::Release()` followed by wake fanout **only if released**, preserving native provider-failure and release notification-failure separately.
- `AcquireUntil<TMaximumWakeRetries>(slot, pool, lease, deadline, arguments...) noexcept`: checks slot range, constructs a stack-local `SlotWaitPoint` adapter without *registering* a pointer or escaping lifetime, reuses `AcquirePoolUntil` canonical Clock deadline and Memory **NoWait** policy, then wakes all slots on successful claim. Slot adapter maps Threading `BoundedWaitWakeResult` to the exact `BoundedWaitResult` typed in `BoundedAdmission.hpp`; other failure becomes `ProviderFailure`.
- Internal members: one borrowed pointer `_waitWake`. The nested adapter retains only the owner pointer and one size_t slot; no shared mutable application state is maintained.

**Important invariant:** producer slot identity is exclusive and its Threading signal must latch Notify-before-Wait. Every capacity release (including RAII destruction/other consumers) must be paired with a wake: the helper cannot intercept uncoordinated `lease.Release()` or RAII cleanup. Exact D2 transactional FIFO, cancellation and end-to-end strict deadline remain open.

## Tests / source limits

`tests/AdmissionWakeCoordinatorTests.cpp` imports a pinned **real EDP-Memory host test fixture** with its actual composed MemoryRuntime, EDP-Threading BoundedWaitWakeProvider, Platform mutex/signal, canonical Clock provider, two actual threads/producer slots, full shared/dedicated pool and 12 repeated release/handoff cycles. It additionally tests wake latching, pre-expired deadlines, failed notifier after successful release, and Memory coordination mutex transient contention without notifications. Run `tests/run-composed-admission-tests.sh` with `EDP_LOGGING_DEPENDENCY_ROOT` pointing at six matching siblings. GCC and Clang host suite passed on RPI400 in `20261016T073232-16bb29e5`; these are host tests, not target power-loss, ESP32 HIL or complete Logging runtime evidence.
