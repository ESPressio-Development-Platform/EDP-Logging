# Reference — `src/logging/AttemptSequence.hpp`

**Classification:** public C++20 Logging ticket/value API and private compile-time storage mapping. [Source](../src/logging/AttemptSequence.hpp). **Dependencies:** only the C++20 standard library. This is identity generation within one application-owned RuntimeIncarnationId, **not** a logger worker, scheduler, global process counter or synchronization primitive.

## Public compile-time vocabulary

`AttemptSequenceWidth` is a strongly typed `uint8_t` enum with individually supported **Bits16** (2-byte ticket), **Bits32** (4-byte ticket), and **Bits64** (8-byte ticket). No other value is valid as template input. `DefaultAttemptSequenceWidth = Bits32` is the compile-time default, not a mutable runtime setting or implicit rollover policy.

`AttemptSequenceAllocationResult` distinguishes **Issued** (one nonzero unique new ticket, output filled), **OutputOccupied** (caller supplied an already valid ticket; no counter/output change), and **SequenceExhausted** (no more distinct values within the incarnation; never wrap/reuse). All three outcomes are stable public API values.

## Private implementation Type

`Detail::AttemptSequenceStorage<TWidth>` maps the supplied width to `uint16_t`, `uint32_t` or `uint64_t`. **Template parameter `TWidth`** chooses the exact representation/storage layout; its `static_assert` prevents unsupported widths at compile time. The nested alias `Type` is the underlying unsigned storage type, not a new runtime allocation.

## Public strong ticket Type

`AttemptSequence<TWidth = DefaultAttemptSequenceWidth>` is a small value object. Its **template parameter** selects the exact 2/4/8-byte layout, required for interoperable width-qualified representation. The public alias **Storage** is the selected unsigned type; public constant **WidthBits** expresses the bit count. Private field **`_value`** is the entire authoritative identity, zero until assigned. The private explicit constructor is used by the factory to prevent implicit numeric-to-ticket conversion; default construction yields an invalid ticket. `FromValidatedValue(Storage value)` restores the exact supplied numeric identity **without independently validating provenance or rejecting zero**: passing zero returns an *invalid/unassigned* ticket. The caller is responsible for independently verifying external field provenance, selected width and nonzero identity before treating it as issued. `Value()` retrieves the raw numeric identity; `IsValid()` tests only nonzero. Neither method confers incarnation uniqueness or validation of external bytes.

## Public stateful issuing Type

`AttemptSequenceCounter<TWidth = DefaultAttemptSequenceWidth>` has the same width parameter and **Storage** alias, plus public compile-time **MaximumValue**. The private **`_lastIssued`** is the only authoritative counter state; it starts at zero, is incremented **only by successful `Issue(output)`**, and never wraps. `Issue` requires an invalid output slot and exclusive external sequencing; it checks output occupancy first, then maximum exhaustion, then increments and publishes exactly one ticket. It returns one typed `AttemptSequenceAllocationResult` and performs no allocation, scheduling, native time read or implicit lock. `LastIssued()` reveals the current counter value and `IsExhausted()` is the exact maximum-value predicate; both require the same external synchronization if another task may issue. Construction starts a fresh incarnation's sequence; copy and move are deleted, and no reset/rewind API exists.

## Concurrency, lifecycle and integration

Only a single owning runtime/incarnation may issue through a counter. **EDP-Threading/application Bootstrap owns the exclusive producer sequencer**, and callers must serialize `Issue` (including check and update) rather than assuming 64-bit atomicity on ESP32. A successful ticket is an **attempt identity**, not proof of queue admission, record creation, sink acceptance, persistence or acknowledgement. Rejected admission may still consume a previously issued ticket. The generated record encoder records exact width metadata; neither this primitive nor the default Logging umbrella requires EDP-Mesh or a synchronized clock.

**Validation:** GCC/Clang Logging host suites including issuance/exhaustion, width and producer integration; see [G1–G15 qualification](../docs/ACCEPTANCE-G1-G15.MD) and the exact run receipts in Policy/main. Do not infer whole-firmware threading/power-loss certification from these value tests.
