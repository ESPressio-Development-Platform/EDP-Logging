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
profile (CBOR or JSON) and **1–8 static bounded copying sinks**. The one-sink profile
(e.g. Serial-only or Persistence-only) is valid without a dummy sink. Nothing in Logging
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
mutex. Checks: single copying sink accepted/rejected, encoder and
pool error isolation, input mutation after the last borrower, and
one encode for two independently copying sinks,
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

## Single-sink G10 bug qualification — 2026-10-19, issue #7

D10 does not require a second consumer. The earlier compile-time minimum of
two excluded otherwise valid Serial-only and Archive-only application
compositions. The bounded output and outcome now allow exactly one sink
(minimum 1, maximum 8) while still rejecting zero. The same EDP-Memory
pool/lease, no-wait acquisition, one encoding and synchronous copy-before-return
rules apply. No wire or public owner dependency changed.

Host regression tests validate accepted and rejected sole sink, source-input
mutation after EDP-Memory release, encoder provider failure before any sink
callback, and pool exhaustion with no double-ownership. Historical two-copy-sink,
Serial queue and CRC cases remain in the same test executable. No async leasing
is introduced, and a copying sink's acceptance still does not mean durable.

## 2026-10-19 — G10 one-sink *real ESP32-WROOM* integration

The real ESP-IDF WROOM HIL now exercises two genuinely static G10
single-sink configurations **in addition to** its existing two-sink fanout:
an EDP-Memory-owned CBOR buffer accepted by EDP-Serial's bounded
copy-on-enqueue FIFO then independently transmitted and EDLG CRC-checked
after the source representation lease has been returned; and a second
EDP-Memory-owned CBOR buffer accepted by one archive-style synchronous
copying sink with no dummy Serial consumer. Each case checks exactly one
encode and one sink call, accepted count 1, empty owner lease, independent
payload ownership and complete pool teardown. The existing native
two-sink ordering, rejection, CRC and 18 canonical markers remain intact.

RPI400 real integrated ESP32-D0WD (WROOM) 2026-10-19:
- Actual PioArduino/ESP-IDF build **20261019T102408-3ed33bde PASS exit0**,
  source Git blob 41cab54885badfafc3f5d1aa611c58d473e7e630,
  entire factory image SHA256
  65e28a05e853f8266390be98948563928fb0aa8096948849efc21d50a40ed146.
- Physical write from 0x0 and **separate on-chip digest verification**
  **20261019T103019-e7ce3060 PASS exit0**.
- Three independent live reset + intentional NVS restart UART sessions
  **20261019T103056-344072ea PASS exit0**, finish 10:31:06 CEST.
  Every run 18/18 native functional markers, EDLG CRC, full-test
  resource checks and both G11 marker observations PASS; the canonical
  MEMORY_OWNED_ENCODING_SERIAL_COPY gate now also proves both single sinks.
- The six retained capture files/hashes live privately in
  /srv/espressio/scratch/gpt6-logging-20261019-g11/g10-one-sink-captures;
  capture-sha256 manifest digest
  5f10a96f545f4ac5ce5f410ef31df0b1a0b0f9a37bb64b8d21cc3cc364fc7562.
- Whole test bundle free8 min 282016 B, dedicated Thread stack remaining
  5036 B and app_main stack remaining 5048–5064 B. These do **not**
  constitute a complete Logging product resource budget.

**Limits:** The archival callback is an independently copied bounded
memory fixture, **not** a durable EDP-Persistence sink. Accepted Serial
FIFO bytes are not proof of end-user physical receipt. This one-sink
change does not complete asynchronous copy/lease/fork-join, governed
15-field CBOR/JSON or power-interruption durability; issue #1 remains open.
