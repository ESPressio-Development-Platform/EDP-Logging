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
