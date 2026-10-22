# G4 generated, schema-bound complete LogRecord Typed Envelope

**Status:** optional EDP-Logging feature branch implementation; native
GCC/Clang and ESP32-WROOM qualification as of 2026-10-22.
Version 0.1.0 unchanged; not a finished G1–G15 Logging product.

## Authority, identity and wire

The existing permanently issued LogRecord TypeIdentifier
`00 00 01 00 00 00 00 01` and all **fifteen** System FieldIds
0–14 remain unchanged. No second TypeIdentifier is minted.

`GeneratedLogRecordWireProjection<TAttemptWidth, TParameters...>`
is an application-composed, finite representation of an existing
`BasicLogRecord`. It expresses Field13 as
`std::tuple<GeneratedPositionalWireTypeOf<TParameters>...>`,
in exact compile-time parameter order, for direct EDP-Serialisation
CBOR and JSON `RootProfile::TypedEnvelope` operations. It does
not serialise Logging's private source arena offsets, kind tags,
pointer values or native C++ structure layout.

`ProjectGeneratedLogRecord<MessageSchema>` checks the original
source validation, the *actual EDP-Localisation GENERATED* diagnostic
message identity and exact historical fingerprint, positional arity,
and owner-certified semantic-to-wire conversions. It reloads each
source-owned argument into finite caller-owned positional scratch;
failure clears scratch and returns a typed result, never silently
publishes partially converted bytes. The actual CBOR/JSON typed
record writers/readers belong to EDP-Serialisation; tuple support is
an additive owner capability on `EDP-Serialisation/feature/logging-v1`.
The optional ClockEra diagnostic argument requires explicit
`logging/DiagnosticClockEraArguments.hpp` selection by the consumer,
and does not automatically claim a coherent Clock.

The full typed root is available **only with a precise generated
diagnostic schema and supplied compile-time parameter Types**.
This is not a generic runtime schema registry, nor a new wire-tag
allocation for the private source argument descriptors.

## Evidence

GCC and Clang full generated Localisation contract source-to-root
and complete envelope test run `20261022T101935-6c4407d7`
**PASS exit0**. EDP-Serialisation's entire owning GCC/Clang
suite including its new tuple schema-field test
`20261022T110514-0d40fd4c` **PASS exit0**.
Initial WROOM firmware compile `20261022T102541-0e5f81df`
**FAIL exit1**: the test omitted the already-approved
opt-in ClockEra owner adapter include. After inclusion, complete
ESP-IDF WROOM build `20261022T104606-13571622`
**PASS exit0**, physical flash verified, but real UART run
`20261022T104941-b95bb83d` **FAIL exit24**:
`LoadProhibited` after the whole-record decode on the initial
8 KiB HIL main task stack (decoded `_puts_r` at main.cpp:187).

A test-only 16 KiB main-task-stack diagnostic configuration
ran full ESP-IDF build `20261022T105402-3bbf6da3`
**PASS exit0**, image SHA256
`9ccd3e24db8459393442769f83b653e0a9e1fa4b41c76c02df79fc8b3acff124`.
Actual ESP32 0x0 write plus independent verify-flash and
three separate reset/NVS-restart UART runs
`20261022T110006-9f45a8ce` **PASS exit0**.
All three retained **18/18 functional markers**, EDLG CRC,
resource probes, G1 deadline and G4/G10/G11 prior gates.
Capture SHA256 manifest digest
`3540448f25bb2e907ba27fe57b3c094a83beb7f2f812cc110f8940feaf63a807`
is retained privately on RPI400.

Actual 16 KiB HIL main-task high-water observations:
**13,256 B free before** and **5,560 B minimum remaining
after** the new path, corresponding to **10,824 B peak consumed**.
These values prove the test's 8 KiB stack was insufficient,
not that any universal production minimum is certified.
Avoid adding a product stack requirement based on this one
hardware profile; reduce and requalify codec stack use in a
separate optimisation gate. Test-app stack remains explicitly
16 KiB until requalified.

## Reproduction and release limits

Use the existing checked-out authoritative owner branches and
EDP-Localisation's actual generator. The test fixture generator
`tests/hil/g1-espidf/generate_g4_root_fixture.py` emits the
required `GeneratedLocalisationContract.hpp` under a clean
application `src/generated`, with a declared two-parameter
RuntimeIncarnationId/ClockEra message. It is NOT permission
to substitute a hand-authored contract. Build with PioArduino's
C++20 ESP-IDF 5.5.5 configuration and exact owner dependency pins;
capture only with an explicitly held RPI400 agent lock.

Still open: a product-wide bounded stack/performance budget;
synchronised true cross-device Clock source, authenticated
historical schema provenance, full source-to-sink coordinator
lifetime including asynchronous retention, true power interruption,
rolling archival safety and remaining G1–G15 tests.
