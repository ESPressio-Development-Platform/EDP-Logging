# Admission lifecycle atomic binding

The existing optional AdmissionLifecycleCoordinator now accepts a third template
parameter, TAtomicWordProvider. Its default is void, retaining the original
always-lock-free std::atomic<uint32_t> requirement. Existing two-argument source
remains valid where that requirement is satisfied.

Bootstrap may explicitly select an existing Platform AtomicWord32 provider.
The owning public contract validates its capability, LockFree property,
four-byte storage, noncopyable/nonmovable Word and noexcept load/store/CAS
operations. Logging selects that existing storage through private type
selection; it creates no atomic provider or alternate lifecycle algorithm.

The same lifecycle owns two words: a monotonic shutdown flag and the count of
active operations. Construction explicitly initializes both before publication.
Reads acquire; shutdown stores release before existing wake fanout; scope count
updates use acquire/release operations. A canonical CAS failure refreshes the
observed value and retries. This is lock-free, not a wait-free per-caller bound.
Scheduler and atomic contention time are not a guaranteed overall latency.

Shutdown/drain semantics are unchanged. Count before queue/Memory waiting;
decrement once on every scoped exit; wake the existing dedicated drain slot
after shutdown. One extra wake slot is required beyond producer slots.
Quiescent counts active operations, not accepted leases; task join and explicit
Memory lease return still precede owner teardown.

## Why explicit selection is needed

The pinned Arduino SDK adds -mdisable-hardware-atomics. Its standard uint32_t
atomic trait therefore fails the existing G1 requirement even though ESP32
hardware and the compiler support native 32-bit CAS. Actual-command isolation
234903-65ff38d3 reproduced the difference; bare toolchain probes234324-eac769fd
confirmed support. The library must not remove that assertion or change global
SDK flags to hide it.

The existing ESP-IDF AtomicWord32Provider qualifies complete internal-DRAM
storage before first write and rejects unsupported placement. The typed worker
example binds this provider explicitly and keeps lifecycle storage stable in
internal DRAM. The existing Portable provider supplies the corresponding real
host binding for tests. Neutral Logging includes only the abstract Platform
contract, never a concrete SDK provider.

## Source and expansion notes

Private AdmissionAtomicWordSelection selects the existing word type.
LoadAcquire, InitializeWord, PublishShutdown and ChangeActiveCount preserve
ordering over the selected existing operations. ActiveAdmissionScope continues
to register/unregister the same parent owner and notify the same drainer.
No Memory operation, allocator, queue, signal, clock or scheduler is added.

The optional abstract Platform dependency already belongs to the G1 graph.
Concrete ESP-IDF/Portable selection belongs to application/test Bootstrap.
The header remains outside the default Logging umbrella. All versions remain
0.1.0; source consumers rebuild with their selected template configuration.

## Verified evidence

RPI400 20261031T000433-77e1c128 PASS: GCC and Clang with UBSan, both legacy and
canonical Portable bindings, four contending scopes, exact active-count/drain,
real Memory/cancellation/late-grant/wake-failure coverage.
000929-5a3dd6bc PASS: producer, queue, real Memory, bounded transitions,
typed batch Task/VFS integration under both compilers.
001504-eee39309 PASS: Arduino IDE, PlatformIO Arduino and ESP-IDF actual
two-boot typed-worker demonstrations using the ESP-IDF hardware word.
Global SDK atomic flags were retained. These finite tests do not establish
wait-free execution or complete production readiness.
