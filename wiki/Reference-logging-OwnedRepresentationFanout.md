# G10 Memory-owned encoded representation fanout

**Source:** [src/logging/OwnedRepresentationFanout.hpp](../src/logging/OwnedRepresentationFanout.hpp). **Classification:** optional public composition contract, with memory/ownership internals documented for cross-repository consumers. This header is not imported by the default Logging umbrella.

## Architectural purpose

Bootstrap composes an EDP-Memory ObjectPool for exactly one fixed-size encoded output representation; EDP-Platform supplies the native non-blocking admission policy. Encoders and sinks are selected statically by the application. One through eight synchronous copying sinks may consume a single encoded CBOR or JSON result. Serial-only and archive-only are valid one-sink compositions (issue #7). This implementation creates no extra Task, heap, thread, or dependency on Mesh or concrete Serial.

## Public type contracts

**OwnedEncodedPayload<TMaximumPayloadBytes>**: the pooled bounded value. The template parameter defines a positive byte capacity. Member Bytes is the owned byte array; Used records actual encoder output length. The type contains no borrowed pointers.

**RepresentationEncodeStatus**: Succeeded means the encoder completed; Rejected is a semantic refusal; ProviderFailure means its underlying implementation failed. **RepresentationEncodeResult** has Status and Written: the latter must fit the static array for a successful encoding.

**OwnedRepresentationStatus** includes SubmittedAndReleased, PoolCapacityUnavailable, PoolUnavailable, PoolProviderFailure, OutputLeaseOccupied, EncoderRejected, EncoderProviderFailure, EncoderLengthInvalid, SubmissionNotAccepted and MemoryReclaimUnconfirmed. These distinguish bounded capacity/ownership failure, typed encode refusal, individual sink decisions, and possible unreleased owner capacity; they do not claim actual UART delivery or durable Persistence commit.

**OwnedRepresentationOutcome<TSinkCount>** is a statically bounded result Type for one through eight sinks. Status summarizes the result; Acquisition and Release preserve the exact EDP-Memory owner outcomes; EncodedBytes is the successfully written byte count; EncoderInvocations and SinkInvocations record bounded callback counts; Accepted records accepted callbacks (not end-to-end delivery). Submissions is the fixed TSinkCount array containing ordered per-sink RecordSinkSubmission outcomes, including partial/rejected callbacks.

## Function and ownership contract

**EncodeOnceAndSubmitToSinks<TCodec,TMaximumPayloadBytes>(pool, lease, record, encoder, sinks...)**:

- Requires TCodec to be explicitly CBOR or JSON, at least one and at most eight static sink callbacks, and exactly the EDP-Memory ObjectPool value type OwnedEncodedPayload<TMaximumPayloadBytes>.
- Requires an initially empty caller-owned EDP-Memory lease. Admission uses WaitTimeout::NoWait(), and there is no retry, polling, hidden output allocator or wait.
- Calls the noexcept encoder exactly once on a borrowed immutable source record into the bounded owned payload; rejects overlength or provider-error outputs before invoking any sink.
- Calls every selected noexcept sink once in deterministic order with codec and immutable byte span, retaining all individual typed results even if a preceding sink rejects.
- Requires every accepted sink to copy/transfer the bytes into independent bounded owned capacity before returning. Never retain the borrowed span or source record beyond the callback boundary.
- Attempts release of the memory lease after all synchronous sink callbacks. Inability to prove release returns MemoryReclaimUnconfirmed and leaves ownership responsibility with Bootstrap.
- Does not implement asynchronous shared output lifetime, an external I/O timeout, physical Serial arbitration, a complete canonical 15-field typed record, or power-loss durability.

## Validation and remaining boundaries

The host owner-composed [test](../tests/OwnedRepresentationFanoutTests.cpp) and [runner](../tests/run-owned-representation-fanout-tests.sh) verify one-sink acceptance and rejection, one encoding, source mutation after output copying, owner pool exhaustion, encoder failure without callback, and unchanged two-sink Serial FIFO/EDLG CRC. RPI400 detached 20261019T094312-c9539875 **PASS exit 0** with GCC and Clang. The initial attempt failed on a C++ test assertion lexical error and was explicitly corrected and rerun. This is executable host evidence, not newly re-flashed firmware.

Missing G4 Field 11 and 14 governed wire sub-vocabulary, full G10 asynchronous fork/join and complete product resource budgets remain open. Consult [G10 documentation](../docs/OWNED-REPRESENTATION-FANOUT.MD) and parent Logging issue #1.

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
