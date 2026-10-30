# CoordinatorSinkService

Header: logging/CoordinatorSinkService.hpp. Optional direct EDP-Clock
abstraction; excluded from the default Logging umbrella. Version 0.1.0.

CoordinatorSinkServiceOutcome carries Ready or RecoveryRequired,
consumed-once DurableSinkCompletions for one sink, and optional canonical
NextDeadline. CoordinatorServiceSink requires an exact noexcept callback.

DedicatedCoordinator services opted-in source callbacks after each
healthy quantum and uses the earliest deadline in its existing
ThreadContext::WaitUntil. Ready without a deadline retains WaitFor.
RecoveryRequired stops without retry. Aggregate completions count per
sink, independently of source retirement and RAM acceptance.

The existing typed delivery supports a BoundedEdlsBatch target; Bootstrap
forwards service from its source-record callback and keeps encoding,
delivery and service in the same serialized owner. Batch-target delivery
cannot be copied or moved. Dwell configuration includes zero.

See [contract and appliance evidence](../docs/COORDINATOR-SINK-SERVICE.MD).
Host GCC/Clang/UBSan passed; native worker qualification remains pending.

[Full typed managed example](../demos/TypedManagedBatchWorker/README.MD) now
builds and passes actual two-boot tests for all three required targets in
20261031T001504-eee39309. It preserves one coordinator for encoding, delivery
and service and verifies the difference between retired RAM and durable ACK.
