# Reference — `src/logging/detail/ThreadingWaitSlot.hpp`

**Classification:** private Logging admission adapter. [Source](../src/logging/detail/ThreadingWaitSlot.hpp).

`ThreadingWaitSlot<TWaitWakeProvider>` adapts one caller-reserved EDP-Threading wait/wake slot for `AdmissionWakeCoordinator` and `AdmissionLifecycleCoordinator`. The application supplies the provider, while the admission caller reserves exclusive use of a slot for the operation.

- `TWaitWakeProvider` selects the provider type. It must supply `WaitUntil(slot, deadline)` using the EDP-Threading `BoundedWaitWakeResult` vocabulary and the same EDP-Clock monotonic timeline as the caller.
- `WaitWake` is a borrowed pointer to the live provider. The provider owns all synchronization and waiting storage and must outlive the wait.
- `Slot` is the ordinal of the caller's already assigned slot. Constructing the view performs no registration. Copying it conveys no additional slot ownership.

`WaitUntil(deadline)` forwards the slot and the original absolute `MonotonicTimestamp` once. It maps provider `Woken` to Logging's `BoundedWaitResult::Woken`, `TimedOut` to `TimedOut`, and all other results to `ProviderFailure`. The provider controls blocking, and the adapter performs no retry or deadline extension. `Woken` is advisory. The admission caller must retry capacity and lifecycle checks with its original deadline.

This adapter is for ordinary task context. It retains only a pointer and an ordinal. The admission caller owns synchronization for the slot and may use the view only while both the provider and slot assignment remain valid. See the [G1 acceptance evidence](../docs/ACCEPTANCE-G1-G15.MD) for the composed admission tests.
