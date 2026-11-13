# Reference — `src/logging/CoordinatorQuantum.hpp`

**Classification:** public bounded drain helper and result vocabulary, private queue-policy implementation. [Source](../src/logging/CoordinatorQuantum.hpp).

The coordinator processes a finite quantum of immutable queue records using the existing [record-production ownership contract](Reference-logging-RecordProduction). The caller owns the queue, reusable consumer claim and synchronous sinks. The application-composed worker owns wake, stop and join.

| `CoordinatorQuantumStatus` | Meaning |
| --- | --- |
| `QueueEmpty` (0) | The next drain found no queued record. Earlier progress in the same quantum remains in the result. |
| `BudgetConsumed` (1) | The finite record budget was reached. The worker rechecks work and stop before sleeping. |
| `ClaimFailed` (2) | The queue could not establish a safe claim. |
| `ReclaimUnconfirmed` (3) | The current source release could not be proven. The caller must preserve and reconcile its claim. |
| `PriorClaimUnresolved` (4) | The supplied claim was already active at entry. The quantum leaves it with its owner. |
| `QueueContended` (5) | The bounded transition returned Busy or DeadlineElapsed before acquiring the next claim. |

`CoordinatorQuantumOutcome<TSinkCount>` owns one result and finite counters. `TSinkCount` must be 1 through 8 and fixes both per-sink array sizes. `Status` defaults to QueueEmpty. `Processed` counts only iterations that reach accounting. `AcceptedByAll` counts accounted iterations where all sinks accepted and the source was reclaimed. `FailedSubmissionRecords` counts the other accounted iterations. `NotificationFailures` counts confirmed releases whose advisory capacity notification failed. `PerSinkAccepted` and `PerSinkUnaccepted` count Accepted and all other submission outcomes respectively, by sink position. An iteration returning Empty, ReclaimUnconfirmed or ClaimFailure exits before accounting, even if a sink callback already ran before the reclaim failure. These counters cannot be used as a delivery receipt for the unconfirmed record.

`Detail::DrainCoordinatorQuantumUnderQueuePolicy<TMaximumRecords, TBoundedQueueWait, TQueue, TSinks...>(queue, claim, maximumQueueLockWaitNanoseconds, sinks...)` is the shared synchronous implementation. `TMaximumRecords` must be positive and bounds the number of drain attempts. `TBoundedQueueWait` selects whether the existing queue transition consumes the supplied bounded wait in nanoseconds and reports contention separately. `TQueue` selects the queue and its existing `ConsumerClaim` type. `TSinks...` supplies 1–8 application-owned synchronous callbacks. The queue, claim and sinks remain valid throughout the call. A sink may borrow the source only during its callback. The queue transition lock is released before callbacks run. On an unconfirmed reclaim the caller retains the claim and must resolve it before another drain.

`DrainCoordinatorQuantum<TMaximumRecords, TQueue, TSinks...>(queue, claim, sinks...)` is the public NoWait queue-transition wrapper. The same finite record budget and borrowed lifetimes apply. The function adds no thread, signal or dynamic storage. The finite attempt count leaves the duration and durability of a callback to that callback's provider. See [G1–G15 acceptance](../docs/ACCEPTANCE-G1-G15.MD) for the recorded host and native profiles.
