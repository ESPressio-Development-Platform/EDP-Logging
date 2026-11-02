# src/logging/AttemptSequence.hpp

Public C++20 per-incarnation Logging attempt tickets.

- `AttemptSequenceWidth`: Bits16=16, Bits32=32, Bits64=64. Default Bits32.
- `AttemptSequenceAllocationResult`: Issued=0, OutputOccupied=1, SequenceExhausted=2.
- `Detail::AttemptSequenceStorage<TWidth>`: compile-time mapping to `uint16_t`, `uint32_t` or `uint64_t`; invalid widths generate a compiler diagnostic.
- `AttemptSequence<TWidth>`: strong value with invalid-zero, `FromValidatedValue`, `Value`, `IsValid`, and `WidthBits`; exact native field size (2/4/8 bytes).
- `AttemptSequenceCounter<TWidth>`: one externally synchronized, noncopyable/nonmovable, never-resettable state. `Issue(output)` returns strongly typed outcome and only updates an invalid output when successfully allocating a new ticket. `LastIssued` and `IsExhausted` inspect state.

**Lifetimes:** counter is owned by one immutable RuntimeIncarnationId and must not be reset or shared with another incarnation. `Issue` must run under one Bootstrap-composed EDP-Threading exclusive synchronization contract. No native atomic assumptions, heap allocation or internal scheduler are present. Concurrency protection and producer sequencing are provided by the separate composed RecordProduction and EDP-Threading paths; this primitive itself remains deliberately unsynchronized. The current GCC/Clang owner suite exercises sequence identity, exhaustion and integrated producer behavior; see the qualified acceptance ledger for exact evidence.
