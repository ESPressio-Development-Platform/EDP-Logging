# Record production and bounded single-sink drain (partial G1/G8/G10)

This feature-only, header-only implementation introduces RecordProduction.hpp.
This generic producer API is NOT a governed canonical LogRecord. The permanent
EDP-System LogRecord TypeIdentifier has not been issued and a test-only positional
CBOR array must never be decoded as a governed production LogRecord.

## Dependency direction

Logging consumes EDP-Clock strong temporal values and a Bootstrap-injected
CaptureCoherent() contract. EDP-Mesh is neither a dependency nor a prerequisite.
A local monotonic-only Clock observation is valid, with no synchronized coordinate
and an invalid ClockEra. In mesh-enabled applications the upstream Clock/Mesh
integration owns its reference-coherent time capture, not Logging.

ProduceRecordUntil uses an EDP-Threading sequencer mutex to issue a non-wrapping
attempt ticket and capture exactly one coherent Clock sample BEFORE capacity
waiting. Issued tickets remain consumed, including rejected admissions. The
owned ticket and temporal values go into the EDP-Memory record constructor.
Bounded queue-before-Memory admission and transactional FIFO Commit are reused,
not reimplemented. Caller retains scratch/reservation on release uncertainty.

DrainOneRecord exclusively claims the oldest FIFO record, borrows the native
EDP-Memory-owned object for a noexcept sink submission, then explicitly retires
the record. Accepted, Rejected, Partial and ProviderFailure are distinguished
from confirmed Memory reclamation. Sink acceptance does not prove flush,
physical receipt or durability. A failed reclamation leaves the RAII claim
caller-owned; Bootstrap must not tear down Memory prematurely.

## Limits and tests

Full universal canonical LogRecord fields 0..14, published generated schema,
multi-sink ownership, concrete worker, EDP-Serial/Persistence bindings, power-cut
retention/recovery and the EDP-System governed root identity remain OPEN.

Native GCC and Clang tests reuse actual Memory ObjectPool, Threading wake,
BoundedTopology transactional FIFO and local Clock. Target ESP32 HIL additionally
serialises a TEST-ONLY positional array [attempt, monotonic, value] through
EDP-Serialisation, frames with canonical EDLG/CRC, emits UART proof and confirms
source lease reclamation. This does not imply full G1-G15 qualification.

## Bounded synchronous multi-sink submission (partial G1/G10)

DrainOneRecordToSinks reuses the existing one-and-only-one source claim
and native EDP-Memory reclamation through DrainOneRecord. It invokes two
to eight app-composed synchronous noexcept sinks in supplied order,
each exactly once, while the source is still Memory-owned. Every sink
must own/copy its required content before returning (borrow never escapes).
The separate per-sink Accepted/Rejected/Partial/ProviderFailure results,
Attempted/Accepted/Unaccepted counts and final reclamation outcome prevent
partial delivery being misreported as complete. A failed sink is NOT retried
automatically because earlier sinks may have accepted the same record.
Real multi-task fork/join, durable acknowledgements and sink retry/dedup
policies remain future production work.

GCC/Clang host tests reuse actual EDP-Memory and EDP-Threading, including
all-accepted, partial, failed and occupied/empty source cases. The physical
ESP32 application separately tests three sink statuses with a single real
EDP-Memory lease reclamation.


## 2026-10-17 occurrence-time and source-value correctness hardening

`ProduceRecordUntil` now takes exactly one coherent Clock snapshot
at **entry before all potential sequencer-mutex/queue/Memory waits**.
Even if sequencing fails, the enabled source occurrence has
`TimeCaptured=true`; no false shifted timestamp at a later successful
lock acquisition. The per-incarnation attempt ticket still issues
under the caller-provided EDP-Threading sequencer mutex; incomplete
publication is never recorded as durable delivery. The generic producer
captures all caller scalar/enum values in finite stack-owned `std::tuple`
storage before its first blocking path. It does not accept raw
pointers, spans, aggregate pointer-bearing wrappers or dynamically
allocated objects that could retain live source state. The separate
generated `LogRecordSourceAssembly.hpp` is required for bounded
strings/bytes and governed strong semantic values.

Regressions in `RecordProductionTests.cpp` cover refused sequencer
capture, stop-path capture, and a sequencer provider deliberately
mutating the original caller argument during its `Acquire`.
The subsequently drained record must retain the value sampled
**before** that wait. No new allocator, Clock or platform provider.
This remains a G1 source-boundary improvement, not a real
production coordinator or authenticated coherent Clock provider.


## 2026-10-17 bounded coordinator drain quantum (partial D5)

New opt-in `logging/CoordinatorQuantum.hpp` composes the already-proven
`DrainOneRecordToSinks` primitive into a bounded, static
`DrainCoordinatorQuantum<MaximumRecords>(queue, claim, sinks...)`.
It cannot process more than the compile-time work budget and invokes
each of 2..8 statically selected synchronous sinks at most once per
source record, outside queue locks. It reports records drained,
all-accepted results, failed-submission records, per-sink accepted/
unaccepted counts and native release-notification failure separately.
On an unconfirmed native reclaim it **immediately stops**, retaining
the caller-owned active claim rather than dropping it or retrying
forever. A preexisting active claim returns `PriorClaimUnresolved`
without invoking any sink or modifying queue ownership.

An exhausted budget is **not** equivalent to an empty queue;
worker must inspect canonical Queue state before sleeping on an
EDP-Threading advisory wake. The coordinator remains an ordinary
synchronous bounded operation; the real Bootstrap-selected Dedicated
Thread, task wake/stop/join, asynchronous encoding and output buffer
fan-out lifetimes are still outstanding. No claim of bounded sink
I/O duration is made. Regression in `RecordProductionTests.cpp`
proves multiple records across quanta, static two-sink fanout,
rejections without retry, empty polling, exact-budget stop and
previous-claim safety. This adds no native platform SDK dependency.
