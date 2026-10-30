# Bounded queue transitions

Existing header: logging/TransactionalRecordQueue.hpp, version0.1.0.

CommitUntil, PopUntil, AbortUntil and CompleteUntil reuse the existing
ordinary mutex and one canonical absolute deadline. Late native grants
cannot mutate the queue. Timeout preserves reservation/claim ownership;
post-release finalization retry never releases the native object twice.
Release-notification failure survives that retry.

ProduceRecordUntil uses the original deadline for commit. The same
DedicatedCoordinator explicitly accepts CoordinatorQueueLockWait<N>.
Unclaimed contention is counted separately and ordinary sink service
continues. Old no-wait entry points remain.

Strong handle Borrow uses its stable owned lease view; raw-ticket Borrow
takes the no-wait queue lock before inspecting occupancy.
[Contract and evidence](../docs/BOUNDED-QUEUE-TRANSITIONS.MD) describe
canonical binding, failure handling and current native qualification limits.
