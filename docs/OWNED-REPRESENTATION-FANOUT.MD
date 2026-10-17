# EDP-Memory-owned encoded representation fanout — G10 subset

**Implementation:** `logging/OwnedRepresentationFanout.hpp` (2026-10-17,
`feature/logging-v1`), application-composed, optional, no extra native task
or queue. This is an independently bounded production **representation
buffer lifecycle and copying-sink adapter**, not the complete G10
asynchronous persistence/serialisation lifetime or full G4 Typed Envelope.

## Ownership and composition

The application Bootstrap declares a real
`EDP-Memory::ObjectPoolSpec<Logging::OwnedEncodedPayload<N>,
DedicatedInstances<C>>` with a finite slot count. It selects one
`EDP-Serialisation` representation-specific encoder, one immutable codec
profile (CBOR or JSON) and 2–8 static bounded sinks. Nothing in Logging
allocates memory or creates a competing object pool.

`EncodeOnceAndSubmitToSinks<Codec,N>` attempts one
`pool.Acquire(callerLease, WaitTimeout::NoWait())`; it owns one live
encoded buffer in the caller's EDP-Memory lease. A typechecked
`noexcept` encoder writes once into that exact bounded storage and
returns `RepresentationEncodeResult`; no partially encoded result is
delivered. The same immutable `span<const byte>` is offered in
deterministic order to each participating copying sink. Each sink must
copy/transfer into its OWN bounded storage before returning
`RecordSinkSubmission::Accepted`; borrowing beyond callback return is
forbidden. Each sink is offered exactly once whether predecessors
accepted, rejected, partially accepted or failed.

All representations are **independent**: choose one buffer owner for
CBOR and another for JSON when both representations are requested.
The application/Bootstrap must not encode twice merely because two
sinks request the same profile. A future async fork/join must explicitly
own encoded buffers for each independent downstream consumer; this
adapter covers native copy-on-enqueue and synchronous sinks, not
shared async borrow/refcount or fully async Threading completion.

## Failure and reclamation

The outcome separately records native pool acquisition, encoding
result, per-sink submission, accepted count, and EDP-Memory release.
Pool absence, exhaustion, preoccupied caller lease, bad encoder length,
and encoder provider error never invoke sinks. If EDP-Memory release
is uncertain, the caller-owned lease remains explicit and its backing
must stay alive until owner-side recovery. The implementation DOES
NOT silently free, discard, requeue, block/retry or claim a
provider failed operation was successful. An accepted sink enqueue
is NOT physical UART transmission or Persistence durability.

The existing `EDP-Serial::BoundedEdlgFrameQueue` qualifies as a
bounded copying sink; its `TryEnqueue(codec, payload)` owns a copy
in a queue slot, and its `PumpNext` serializes one EDLG frame
without byte interleaving within that queue's exclusive endpoint.
Bootstrap still owns Serial endpoint arbitration and must not
mix console writes with frame mode. This adapter does not add
Logging→Serial compile-time dependency; the Serial concrete
binding exists only in tests/application composition.

This is a **strict no-wait admission**, not a guarantee that sink
callbacks complete within a physical deadline. Real sinks must
declare and observe their own finite execution budgets.

## Tests

`tests/run-owned-representation-fanout-tests.sh` compiles against
actual EDP-Memory Runtime/ObjectPool/Lease and real
EDP-Serial::BoundedEdlgFrameQueue with an EDP-Threading ordinary
mutex. Checks: one encode for two independently copying sinks,
exact codec binding, accepted buffers survive input mutation,
one rejected sink does not roll back the other, single-slot pool
exhaustion and preoccupied output lease, overlong owner encoder
fails closed without calling sinks, independent CBOR/JSON
profiles, Serial FIFO full backpressure, two CRC-verified frames
pumped after original EDP-Memory lease was returned.
GCC/Clang/UBSan and actual WROOM validation must be reported
separately and not invented.

## Open release gates

This component does not issue the missing Field11/14 LogRecord
sub-vocabularies, encode a complete canonical 15-field Typed
Envelope, implement a provider-coherent synchronized Clock,
or establish true PowerLoss durability. A production coordinator
still requires fully integrated app macros, profile assignment,
bounded async downstream completion and independently verified
resource budgets.
