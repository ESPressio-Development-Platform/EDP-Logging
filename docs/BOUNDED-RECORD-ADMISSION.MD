# Composed bounded record admission — opt-in G1

This optional admission composition is available on `main` in the 0.1.0 development baseline. Its scope is queue and source lease acquisition.

## Purpose and ownership

The public, optional header src/logging/BoundedRecordAdmission.hpp composes the existing TransactionalRecordQueue, AdmissionLifecycleCoordinator, EDP-Memory ObjectPool NoWait acquisitions, EDP-Threading fixed-slot BoundedWaitWakeProvider and the one canonical EDP-Clock MonotonicTimestamp. There is no new waiter registry, memory pool, clock timeline, platform concrete or new dependency direction. Application Bootstrap selects and retains all concrete providers.

Each producer owns an exclusive EDP-Threading wake slot, an initially empty caller-owned Memory scratch lease and an inactive move-only queue Reservation. AcquireRecordUntil<MaximumQueueWakes,MaximumMemoryWakes> first reserves the queue entitlement before trying to acquire a potentially scarce Memory lease, then uses the existing lifecycle's NoWait Memory admission, attaches the acquired lease to that reserved queue slot, and returns the active exclusive Reservation. The caller fills the record and explicitly commits or aborts; an uncommitted guard aborts on destruction. Successful admission does not certify commit, sink processing, transport or durability.

## Deadline, shutdown and rollback

The caller supplies a single absolute canonical EDP-Clock monotonic deadline. Each queue mutex operation receives the remaining relative native EDP-Platform WaitTimeout measured from that same deadline. EDP-Threading WaitUntil consumes that original absolute deadline. Queue and Memory retry limits are compile-time constants; wake does not reset the deadline. These constraints apply to the admission owner operation, not arbitrary application computation or beyond the timing guarantee of the chosen native provider.

TrackAdmission counts the entire queue wait, Memory admission and rollback against the existing lifecycle shutdown drain. The inner Memory AcquireUntil is also counted in the same coordinator for its short lifetime. ShutdownRequest wakes all existing producer slots. ShutdownUntil cannot report quiescence while an active queue-stage producer has yet to exit. This does NOT drain accepted source records, outstanding leases, sink callbacks, buffers or application Tasks: Bootstrap must stop new calls, join all workers and return every outstanding owner before teardown.

RecordAdmissionOutcome contains Status, Queue, Memory, Release, MemoryAttempted, ReleaseAttempted. Its strong RecordAdmissionStatus values are Admitted, DeadlineElapsed, ShutdownRequested, QueueWakeRetryLimit, QueueWaitProviderFailure, QueueFailure, MemoryFailure, LateMemoryGrantReturned, ReleaseUnconfirmed, ReleaseNotificationFailed and OutputOccupied. Failed Memory release leaves the scratch lease owned; a failed queue abort leaves the RAII reservation active and reports the problem. The success status never hides notification failure. Late grants are explicitly returned through the existing coordinator or remain caller-owned when release cannot be confirmed.

## Resource and safety limits

Bootstrap fixes the queue, producer, Threading wake-slot and Memory pool
bounds. This ordinary-context path uses the Memory NoWait acquisition contract.
Direct native blocked-waiter relock behaviour is tracked separately in
[Memory #16](https://github.com/ESPressio-Development-Platform/EDP-Memory/issues/16).
The [record producer](RECORD-PRODUCTION-OWNERSHIP.MD) composes this path with
source capture, commit and sink service. Physical results below apply to
the named concurrency and deadline profiles.

## Tests

From EDP-Logging, with six exact pinned abstract EDP siblings: CXX=g++ bash tests/run-bounded-record-admission-tests.sh (also CXX=clang++). Test covers real Memory/Threading admission, queue capacity saturation without premature pool ownership, published release wake, finite deadline, cancellation and a queue-stage producer counted during shutdown. Run transactional queue and admission lifecycle suites alongside. The authoritative Policy/main handoff records exact detached RPI400 run identifiers and remaining qualification gaps.

## Producer-slot authority and independent publication health

The lifecycle coordinator's new IsValidProducerSlot(slot) checks the exclusive producer range and rejects the dedicated shutdown drainer slot (or any out-of-range value) **before** a queue slot is claimed or a Threading WaitUntil is attempted. InvalidProducerSlot (strong enum value 11) leaves all caller-owned Queue and Memory state unchanged. RecordAdmissionOutcome also includes the independent WakePublication status: the original AdmissionWakePublicationResult produced on an actual Memory grant is retained even if the returned RecordAdmissionStatus is Admitted. A successful reservation with a failed producer wake is not a silently healthy operation; Bootstrap must respond to that provider failure. When both a native Memory release and Queue abort fail, ReleaseUnconfirmed remains the top-level ownership-uncertainty status and Queue carries the second failure. Tests include genuine Memory grants with an injected Threading wake failure and invalid use of the reserved shutdown slot.


## 2026-10-19 — physical full-queue deadline evidence

New opt-in native HIL test uses real ESP32 FreeRTOS EDP-Threading,
EDP-Clock, EDP-Memory and the two-slot queue under a 40 ms producer
deadline. Requires a DeadlineElapsed result, no Memory acquisition,
no outstanding pool lease/reservation, zero live records and queue
capacity returned, within a 60 ms measured wall-clock check.
Separate verified flash plus three independent actual WROOM runs
were **PASS**, measured 31.810, 31.808, 31.678 ms, factory SHA256
e4bb59b63d1e0d3c03c7a17bf95bf68da43589184276dae330f340464e0b1f1b.
Exact jobs and capture-hash proof are in
[HIL validation](../tests/hil/g1-espidf/VALIDATION.MD).
GCC+Clang actual Memory/Threading owner admission tests
20261019T125527-7f3a13e2 PASS exit0.

This proves the **full queue / no-memory-borrow** deadline path,
not all contended native internal locks, coordinator shutdown,
multiple producer slots and all strict product worst-case bounds.
G1 release remains open.



## 2026-10-19 — independent native cross-core queue-mutex contention

Beyond the previously physically qualified 40 ms **full queue**
timeout, actual ESP32 FreeRTOS Core 1 now locks
the same EDP-Threading queue mutex while Core 0 attempts
AcquireRecordUntil with a 40 ms EDP-Clock absolute deadline.
Three fresh physical runs PASS with observed **40.035 ms**
elapsed each; DeadlineElapsed, MemoryAttempted=false, scratch
lease empty, no queue output owner, zero live records and
verified true-owner mutex release. Existing 18/18 HIL probes
remain PASS. Full build, flash verify, capture jobs and
SHA-256 recorded in HIL VALIDATION.MD.

The first two images failed because the test owner task
poll delay `pdMS_TO_TICKS(2)` rounded to **zero RTOS ticks**,
allowing premature mutex release and legitimate producer
admission. The test now uses `vTaskDelay(1U)` and
demonstrated the intended actual held-mutex path.
These tests are *not* exhaustive all-provider cancellation
or all-race qualification.


## 2026-10-19 — actual exhausted EDP-Memory pool finite admission

Third independently physically qualified native WROOM source path:
two dedicated EDP-Memory object leases occupied, queue empty,
third producer reserves one Queue slot, waits on Memory with
same 40 ms monotonic deadline, times out at **36.863 ms** in
three runs, yields no scratch lease/queue reservation,
no unauthorized third object, and both held leases return via
ReleaseAndNotify with successful capacity publication. Full 18/18
native HIL, independent flash verify and retained capture SHA
in [VALIDATION](../tests/hil/g1-espidf/VALIDATION.MD).
Actual provider-specific late-grant and adversarial wake/shutdown
cases remain to be independently closed.


## 2026-10-19 — real concurrent Memory-wait shutdown and join

A dedicated real ESP32 WROOM FreeRTOS Core1 producer enters
AcquireRecordUntil and waits against an exhausted native
EDP-Memory Record pool. Core0 confirms ActiveAdmissions>0
and requests a separate finite ShutdownUntil. Genuine
EDP-Threading wake notification leads to Quiescent, zero
active producers, worker returned ShutdownRequested,
joined via native task notification, zero retained output/
scratch ownership, and two held pool leases returned via
the original coordinator. Three physical runs PASS with
292–293 microseconds measured shutdown coordination,
all 18/18 HIL markers intact. Full jobs and artifact hash
in [HIL VALIDATION](../tests/hil/g1-espidf/VALIDATION.MD).
Native provider-failure and adversarial late-grant stress
cases remain independently open.


## 2026-10-19 — two-core real Memory release ordering proof

A new actual WROOM FreeRTOS HIL exercises the same native object
pool across Core1 producer/Core0 Memory capacity release:
one true Memory grant before a 500ms deadline and one 40ms
Memory wait timeout **before capacity is returned**. It requires
only approved `AcquireRecordUntil`, EDP-Memory, Coordinator,
Queue, EDP-Threading signalling, final complete reservation
rollback, and zero live objects or active admissions.
Final physical build `20261019T163505-49528812`, write/verify
`20261019T164114-6cda40fd`, independent real UART cycles
`20261019T164337-cf0128ff` all PASS; 3/3 full native
18/18 markers and both race orderings. See
[HIL Validation](../tests/hil/g1-espidf/VALIDATION.MD).

This is **not** an assertion that a provider granted capacity
at the exact deadline or after it already elapsed; that rare
`GrantedAfterDeadline` interleaving still requires a separately
controlled stress/fault-injection test.

## 2026-10-20 — G1 late native Memory grant rollback qualification

The native HIL includes `g1_injected_late_grant.hpp`: an opt-in
test-only decorator that permits **real** EDP-Memory NoWait acquisition
but delays returning a successful lease until after the Clock deadline.
Logging must report `LateMemoryGrantReturned`, retain the exact
Memory `GrantedAfterDeadline` evidence, release the lease through the
existing lifecycle coordinator, publish capacity and abandon the
reservation. This tests the production rollback logic without
introducing an artificial delay into production Memory.

Real ESP32-D0WD WROOM build, physical independently verified flash,
and three new native UART test cycles PASS. All three captured
`status=7|memory=1|release=0|wake=1|live=0|valid=1` and
elapsed 89,032 microseconds against a 40ms deadline, while existing
18/18 HIL markers remained green. Refer to
[HIL validation](../tests/hil/g1-espidf/VALIDATION.MD).

This does not prove natural race frequency or comprehensive
provider-failure/power-loss behaviour; G1/G14/G15 remain open.

## 2026-10-20 — post-queue-attachment deadline enforcement

`AcquireRecordUntil` now rechecks the same canonical EDP-Clock
absolute deadline **after** the final `Queue::AttachLease`
succeeds. An on-time EDP-Memory grant alone is not permission to
admit a record if the final native mutex operation completes late.

If shutdown or deadline expiry is observed after attachment,
`output.Abort()` returns the **already transferred** Memory lease
through the original queue/lifecycle owner, preserves existing
typed notification-failure evidence, and leaves the producer's
scratch lease empty. The result reports `ShutdownRequested` or
`DeadlineElapsed`, not a misleading late-Memory-grant status.
A failed queue abort is reported rather than falsely reclaimed.
No new statuses, allocations, Threads, platform concretes or wire
IDs were added to production Logging.

Reproducible fault injection uses an HIL-only adapter around the
actual EDP-Threading native mutex, inserting one 80ms+tick delay
on the final timed lock while the EDP-Memory grant is genuine.
Host GCC/Clang `20261020T071258-65de05a8` PASS exit0;
real ESP32 build, on-chip flash verify and 3/3 UART/NVS restart
captures `20261020T071340-b6adac07` /
`20261020T071458-cf925ae7` PASS exit0.
Each cycle reported `locks=2|status=1|memory=0|queue=0|live=0|
elapsed_us=89623|valid=1`. See HIL VALIDATION.MD.
This deliberately delayed mutex tests one failure interleaving,
not the normal mutex latency distribution or complete G1 worst case.


## 2026-10-20 08:38–08:42 CEST — native post-attachment cancellation

After the G1 post-AttachLease canonical deadline fix, a second test
exercises **cancellation becoming visible during** the final native
Queue::AttachLease. The HIL-only provider delegates to a *real*
EDP-Threading mutex and raises a test stop predicate on its second
successful timed Acquire, then introduces an 80ms + FreeRTOS tick
post-acquire delay. The actual EDP-Memory lease transfers to the
Queue and MUST be released by the normal Reservation abort path.
The expected result is `ShutdownRequested` (2), rather than
`Admitted`, even while the queue attachment returns Succeeded.
This is an injected stop predicate, not a claim that real lifecycle
ShutdownUntil or all inter-core stop races were exhausted.

Host real owner-composed GCC and Clang admission tests detached
`20261020T083848-50c62356` **PASS exit0**. Actual complete
PioArduino/ESP-IDF WROOM firmware build detached
`20261020T083937-a40a64b6` **PASS exit0**, factory SHA256
`360ef7ad8b732b32510e2f089318950a7a6df4e412b5b265a18da0cb0c7742c3`.
Real write at 0x0 followed by separately verified on-chip flash
digest `20261020T084050-323a7b0b` **PASS exit0**.
Three independent host-triggered native reset/NVS-restart UART cycles
`20261020T084135-8228e918` **PASS exit0**, each 18/18
original markers, full EDLG CRC/resource/G11 instrumentation.
Every cycle emitted `G1_CANCEL_DURING_ATTACH|locks=2|status=2|
memory=0|queue=0|live=0|elapsed_us=89563–89564|valid=1`;
no owner lease or admission was retained. Private capture files
at `/srv/espressio/scratch/gpt6-logging-20261019-g11/g1-final-attach-cancel-captures-20261020`,
checksum-manifest SHA256
`1bd6c5bde1d9de5a12e9d7aa72fe8296aa786d120982292d3e626fd183396f24`.
No arbitrary full-product timing, natural race probability,
unexpected external power cut or complete Logging release implied.

