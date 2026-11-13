# G4 generated, schema-bound complete LogRecord Typed Envelope

The optional generated envelope API is available on `main` in the 0.1.0
development baseline. See the [qualification matrix](ACCEPTANCE-G1-G15.MD)
for the tested host and ESP32-WROOM profiles.

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
supplied by the qualified `EDP-Serialisation/main` revision in the [owner baseline](DEPENDENCY-OWNER-BASELINE.MD).
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


## 2026-10-22 — requalified fourteen-KiB whole-record HIL stack with phase measurements

The initial complete generated fifteen-field G4 Typed Envelope had a
**test-application-only 16 KiB app_main stack** following the decoded
8 KiB overflow. A fresh physical test reduced this HIL-only stack
to **14,336 bytes (14 KiB)** without adding a heap allocation or
changing any production codec/record/owner interfaces. The fixture
`tests/hil/g1-espidf/sdkconfig.defaults` now declares 14,336 B;
product applications must still determine their own budgets.

A test-only diagnostic `ReportG4TestStack` records actual FreeRTOS
cumulative minimum remaining stack at five boundaries. Three distinct
ESP32-D0WD/WROOM-32 reset and NVS-restart cycles reported identically:

| Phase | Minimum app_main stack remaining |
| --- | ---: |
| Source projection | 11,208 B |
| CBOR encode | 10,536 B |
| CBOR decode | 10,536 B |
| JSON encode | 3,512 B |
| JSON decode | 3,512 B |

**JSON encoding is the observed pressure point:** it decreased
remaining stack by **7,024 B** after CBOR decode; CBOR's earlier
increment was 672 B. These are *cumulative high-water* observations,
not instantaneous stack frame sizes or exclusive per-operation cost.
Full-test peak stack use is 14,336 − 3,512 = **10,824 B**.
A future optimisation should review the **EDP-Serialisation owner**
JSON TypedEnvelope writer/tuple path, not duplicate a Logging codec
or silently expand platform-wide stack defaults.

- 14 KiB PioArduino ESP-IDF build detached
  `20261022T114529-e070ff5c`: **PASS exit 0**. The generated
  config header explicitly defines 14,336.
- Complete factory binary SHA256:
  `acb6aeee53f64852f2c467abc95b4fc19d05f93785369a8908089efceb8c3f56`.
- Actual hardware flash from address 0x0 and **independent on-chip
  digest verification**, plus three real UART/NVS restart HIL
  executions in detached run `20261022T115119-c3346488`:
  **PASS exit 0, 3/3**. Each retains all canonical **18/18**
  functional markers, EDLG CRC, G11 resource source evidence,
  and the complete G4 generated CBOR/JSON roundtrip.
- Private capture sha256 manifest digest:
  `aa90b39888ab953db70c72a04952b372912e27d968bc4f0d707380304f0ccb1e`.
  Retained exclusively under
  `/srv/espressio/scratch/gpt6-logging-20261022-0919-v4/g4-whole-record-physical-14k-phases-20261022`.
- Actual final resource high-water: app_main 3,512 B remaining;
  dedicated worker 5,036 B remaining; whole HIL free 8-bit heap
  before/after/min = 284,316 / 280,796 / 275,872 B. These are
  **whole-application** figures, not isolated Logging library costs.
- The test-only `verify-g4-stack-phases.py` rejects missing/reordered,
  duplicate, non-monotonic or below-2,048-B margin observations.
  Three real traces and synthetic negative cases independently passed
  detached `20261022T115404-f750b36c` (exit 0).

**Limitations:** No full product resource budget, multiple concurrent
large-schema JSON encodes, full strict worst-case stack bound,
uncontrolled real power cut, or end-to-end durable coordinator
is established. The 8 KiB root HIL remains physically **failed**,
and the earlier 16 KiB physical profile remains a valid baseline;
do not replace historical failure claims with this improvement.


## 2026-10-22 — G4 complete record after EDP-Serialisation owner JSON bound

The previous 14 KiB test showed 3,512 bytes of cumulative minimum
app_main stack remaining after JSON. A separate owner fix
([EDP-Serialisation #10](https://github.com/ESPressio-Development-Platform/EDP-Serialisation/issues/10),
commit `08fecdcc74f72c7cc73dc777eb843db727c9827d`) bounds
compile-time canonical JSON field traversal by the maximum actually
declared EDP-System FieldIdentifier. The same complete 15-field
generated LogRecord CBOR/JSON test, with the same **14,336-byte**
app_main stack, was rebuilt and physically requalified without changing
Logging root field identity, codec, or Memory ownership.

- New full factory SHA256: `6ab5745e6ffc587c1c5ca6480e27a11447a6ac0bccc72e6baa6c483f3c3ba572`.
- ESP-IDF build `20261022T120813-3c70bd12`: **PASS exit 0**.
- On RPI400, the recovered build image hash was independently checked
  before real ESP32-D0WD flash/write and separate on-chip `verify-flash`.
  Detached physical job `20261022T122616-a4d5f0a3`: **PASS exit 0**,
  three distinct UART reset/NVS-restart runs, each 18/18 functional
  markers, real full G4 CBOR/JSON encode/decode, framing CRC and G11
  resource markers.
- Cumulative FreeRTOS stack remaining after source projection:
  **11,208 B** (unchanged); after CBOR encode/decode:
  **10,536 B** (unchanged); after JSON encode/decode:
  **10,472 B** (previously **3,512 B**).
- Linked whole HIL firmware flash use: **378,789 B** (previously
  **388,513 B**); statically reported RAM: **21,480 B**, unchanged.
- Private capture manifest SHA256:
  `cb39d128a142951efdb8a25954b2acac95c408dab20a2c10ed7f82aac396727b`.
  Raw evidence remains only under RPI400 path
  `/srv/espressio/scratch/gpt6-logging-20261022-0919-v4/g4-whole-record-physical-14k-jsonbounded-recovery-20261022`.

The full EDP-Serialisation owner GCC+Clang suite, with restored negative
enum-certification tests and sparse/255 boundary static assertions,
also passed `20261022T123157-67f0fdca` exit 0.
These comparisons describe **whole HIL application** high-water
measurements on one compiler/board; they do not prove a universal stack
budget, actual power interruption durability, or completion of the
remaining G1–G15 release requirements. The earlier 8 KiB physical
failure remains a genuine historical failure, not rewritten as a pass.
