# Python host diagnostic framing and governed offline record forensics

**Source baseline:** current branch, 0.1.0 (the early dated test evidence below is historical, not the complete validation record). Parent approved design: Policy/main D8/D13 and EDP-Logging#1; tracked under EDP-Logging#3.

## Implemented source

The pure-stdlib `tools/edp_logging` Python package provides:

- `crc32c`: reflected Castagnoli CRC32C with canonical check vector `123456789 -> E3069283`.
- `encode_frame`: deterministic v1 `EDLG` fixture writer, strictly respecting max-payload limits; this is **host reference only**, not an embedded serializer.
- `scan_stream`: seeks nowhere, reads incrementally through bounded chunks, checks v1 magic, codec (CBOR=1, JSON=2), all flags zero, 32-bit little-endian payload length, complete `GLDE` marker and frame CRC. It never allocates based on unvalidated lengths. Default conservative mode stops at first invalid candidate. Optional forensic mode scans a finite number of discarded bytes for later valid candidates and marks them **forensic**, not part of an uninterrupted archive.
- `parse_segment_header`: strict `EDLS` 40-byte header checker with slot index, nonzero 64-bit generation, canonical 16-byte device and nonzero 32-bit runtime incarnation, header CRC.
- `edp-logging PATH` or `python -m edp_logging PATH` CLI prints one JSON event per verified frame or integrity failure with source offsets. `--archive` validates and displays segment metadata; `--forensic` enables qualified bounded resync.

## Current scope and explicitly unsupported capabilities — 12 November 2026

The early host-only framing prototype described in the preceding paragraphs was subsequently extended. The **current checked-in stdlib Python package implements** optional canonical TypedEnvelope CBOR/JSON structural parsing (`--inspect-record`), supported permanent 15-field LogRecord V1 source-fact validation with an independently pinned root TypeIdentifier (`--validate-record --expected-root-type-hex`), generated EDP-Localisation diagnostic schema and strict positional CBOR argument validation (`--schema --schema-sha256`), explicit historical archive ContractFamily selection/provenance, bounded per-incarnation ticket-gap evidence (`--analyse-attempts`), uncertainty-aware same-era chronological comparison (`--compare-record-times`), and a **separate** bounded JSONL-to-CSV formatter (`python -m edp_logging.export_csv`). The later source-specific sections of this file and [Tooling Reference](../wiki/Tooling-Reference.md) document exact flags, bounds and failure states. Do not mistake an earlier dated “partial” feature note for the current implementation.

**Explicitly unsupported or separate-owner work:** generic historical translated string reconstruction *without exact trusted ContractFamily provenance*, JSON positional argument typing where owner certification has not been established, arbitrary unissued CBOR tags/LogRecord root IDs, generic live UART/serial capture/retention in this **Python CLI**, proof of arbitrary device-side exporter/transport receipts, cryptographic authentication, Flash endurance, direct filesystem mutation and any interrupted-write or electrical PowerLoss validation performed by the host decoder. Optional native EDP-Serial/ESP-IDF output belongs to the distinct Serial owners. The C++ strict FileStorage+NVS-acknowledged production implementation and its named physical cut receipts are **separately implemented and qualified**; this host reader is not their durability evidence.

The CLI reads one source file without modifying it. Frames returned by `scan_stream` own a bounded copy of their payload; aggregate callers must also stream output rather than retaining an unlimited list of frames.

## Authoritative byte format

EDLG header 16 bytes: `EDLG` (4), version=1 (1), codec (1), zero flags (2), payload length LE32 (4), CRC32C LE32 (4); exactly payload length bytes follow, then `GLDE` (4). Outer overhead = 20 bytes. CRC covers initial 12 header bytes then payload then final 4 completion bytes (not the CRC field).

EDLS header 40 bytes: `EDLS` (4), version=1 (1), flags=0 (1), physical slot LE16 (2), generation LE64 (8), DeviceIdentifier (16), incarnation LE32 (4), CRC32C of first 36 bytes (4).

`CRC32C` detects accidental corruption, not authentication. Forensic resync may falsely identify a valid-looking later candidate; report qualified provenance and never infer continuous retained history.

## Tests

`PYTHONPATH=tools python3 -m unittest discover -s tools/tests -v` exercises CRC golden checks, framing, strict historic schema interpretation, exact-byte SHA-256 pinning, malformed pin rejection, CLI pin reporting, archive recovery, bounded analysis and EDLS validation. **20 Python host tests passed on RPI400 on 2026-10-16**, detached run `20261016T065108-f102fd04`, exit 0. Separate foundation C++20 GCC/Clang runs `20261016T064416-0f4afd63` and `20261016T064424-db3864fd` passed. No physical/target builds or power-loss tests are claimed.

## Structural Typed Envelope inspection (no catalogue assumptions)

The new `record_structure.py` can validate a **canonical** CBOR root `[1, bytes8, numeric FieldId map]` or existing JSON Typed Envelope `{"$edp":{"v":1,"type":"lowercase-hex16"},"value":{"0":...}}`. CBOR rejects indefinite containers, tags, non-minimal integer/length encoding, unordered/duplicate/non-numeric map keys, half floats, nonfinite floating values, invalid UTF-8, NUL text, trailing bytes and excessive depth/total nodes. Float32 versus float64 remains distinguished by `PreservedFloat(width_bits)`; JSON integer values are parsed losslessly as Python integers, not forced through binary64.

`inspect_typed_envelope` returns `TypedEnvelope` with the root TypeIdentifier, source codec and numeric-identity-keyed field map. **It deliberately does NOT infer the semantic meaning or signedness of positional argument values without the historical DiagnosticSchemaFingerprint**, nor validate LogRecord field 0–14 semantics yet; those future checks need the governed schema. `--inspect-record` on the CLI enables this optional inspection, preserves independently validated frame metadata when the inner record is malformed and reports the typed-envelope error separately. It never pretends valid frame CRC implies a valid semantic record.

## Matching historical diagnostic argument schemas (CBOR only)

`schema.py` loads the generated Localisation `diagnostic-schema.json` artefact in a bounded way: strict duplicate-name rejection, supported schema version and identifier widths, sorted governed String tuples, `diagnosticDeclared` state (including legitimate zero-argument diagnostics), dense declared positional types/lifecycle and consistent 128-bit fingerprint/full digest. A single host artefact is **not independently authenticated**; the caller must obtain it through a trusted/pinned deployment source or verify the generated-set manifest separately. The flattened current Localisation JSON does not contain enough parent-state information to recalculate its full SHA-256 transcript independently; do **not** confuse a matching claimed prefix with cryptographic verification.

`decode_declared_arguments(TypedEnvelope, DiagnosticSchema)` currently validates CBOR fields 0 (StringIdentifier triple), 12 (exact matching fingerprint) and 13 (positional array) and checks scalar values against their authorised types, including signed/unsigned range, distinct bool, preserved binary32/binary64 width and strong identifier shapes. It **refuses JSON semantic argument typing** until the JSON byte-string/strong-identity representation is locked; structural Typed Envelope validation still supports JSON.

The CLI `--schema` selects historical schema validation and records exact message identity, argument count and canonical types in its JSON output without inventing translation wording. Missing/mismatched schema or invalid parameter values report a distinct `schemaError` while retaining independently verified frame/offset provenance. Original payload bytes remain in the caller's source file.

### Independently pinned exact schema bytes (implemented)

`load_generated_schema(data, trusted_artifact_sha256=pin)` now optionally verifies the **exact serialized diagnostic-schema.json bytes** against a **32-byte SHA-256 digest obtained through an independently trusted deployment manifest or pinned source**. It fails closed with `SchemaError` on an invalid/mismatching pin before interpreting any schema declarations; the unpinned API remains accepted for explicitly untrusted/forensic analysis and sets `artifact_sha256_pin_matched=False`. A correctly matched pin sets this fact True; matching cannot prove that the provider of the pin was trustworthy.

The CLI exposes the corresponding `--schema-sha256` argument (exactly 64 hex characters), which requires `--schema`, and emits `schemaArtifactSha256PinMatched` on structurally valid envelopes. Example (use the trusted manifest's `outputs[]` entry with `path=diagnostic-schema.json` and its `sha256` value; **do not read the value from the untrusted schema itself**):

```sh
PYTHONPATH=tools python3 -m edp_logging capture.edlg --schema generated/diagnostic-schema.json --schema-sha256 <64-hex-digest-from-trusted-manifest>
```

This is a **separately pinned artefact-byte integrity check**, not reconstruction of `DiagnosticSchemaFingerprint` from the flattened JSON; that canonical digest still depends upon Domain/SubDomain status and empty branches omitted from the generated schema. It does not authenticate unsigned manifests, external channels or a claimed LogRecord TypeIdentifier. No Localisation toolchain fingerprint algorithm was duplicated or modified.

## Historical schema input hardening

Schema document parsing rejects unknown top-level properties, duplicate members, booleans masquerading as schema version 1, nonfinite JSON constants, malformed or unhashable declaration properties and retired-message use for currently active diagnostic argument decoding. The loader still requires a trustworthy origin for the externally claimed SHA-256 identity; it is **not** a cryptographic verifier of the flattened JSON contents. Negative unit tests passed on RPI400 in the Python host suite dated 2026-10-16.

## LogRecord candidate facts (D13 FieldIds 0–14)

`record_facts.py` now provides a bounded semantic-range check over a structurally decoded Typed Envelope. It validates all required D13 fields, optional field7 synchronized coordinate (zero is valid), emitted severity 0..5, 16-byte nonzero DeviceIdentifier, uint32 incarnation, attempt ticket within explicitly selected 16/32/64-bit width (nonzero, no wrap), unsigned nanosecond timestamps, EDP-Clock state 0..4, opaque Era, saturated uint32 uncertainty, exact 16-byte schema fingerprint and positional array. Missing or unregistered FieldIds fail. Incoherent `NeverSynchronized` plus coordinate and present coordinate without valid Era also fail.

The root TypeIdentifier allocation is **not issued yet**. The function therefore returns a `RecordFacts` **candidate**, with optional `expected_root_type_identifier` for future registered equality checking; never treat unknown nonzero TypeIds as conclusively LogRecord. Field11 temporal provenance and field14 flags have no governed exact sub-vocabulary yet; they are preserved uninterpreted rather than manufactured. The CLI `--validate-record` exports qualified `logRecordCandidate=true`, `rootTypeGoverned=false`, without claiming chronology or authentication.

This is structural/semantic-range groundwork only and the new unit tests have **not been executed**.

## Forensic continuity correction

Once any corruption or resynchronisation has occurred, all subsequent individually valid frames in **that same scanned source stream** retain `recovered_candidate=True`, even when no additional damaged bytes appear between them. The inter-frame resync-search byte budget resets after each validated candidate, but CRC success alone never re-establishes the pre-damage archive chain. Added an authored test covering an intact prefix, corruption and two successive later valid frames. This test passed as part of the 2026-10-16 RPI400 Python suite.

## Portable EDLS segment header parity

Both C++ `WriteEdlsHeader/ReadEdlsHeader` and Python `encode_segment_header/parse_segment_header` use a version-one 40-byte portable layout, little-endian fixed-width fields, nonzero generation/incarnation/device, zero flags and CRC32C over exactly bytes 0..35. A shared golden vector is documented in source tests: slot 2, generation 7, device 01..10 hex, incarnation 9, checksum LE `AE 4E 0C 51`. This is not a Flash append protocol and does not establish crash durability. C++20 GCC/Clang and Python foundation suites passed on RPI400 on 2026-10-16; no physical Flash/runtime test is implied.

## Read-only bounded multi-slot inspection

`archive_recovery.py` accepts a statically supplied finite list of physical slot paths (minimum two; explicit upper bound) and inspects each header and *contiguous* EDLG prefix without loading an entire archive. It reports slot state (empty, intact, damaged tail, malformed header, wrong physical slot, I/O failure), last verified byte offset and frame count. It identifies only the largest **known** generation; any unreadable/bad header, duplicate generation or mixed DeviceIdentifier makes the generation ordering ambiguous and prevents recommending an incremented generation. No generation wraps; 64-bit exhaustion is explicit. The Python reader **never mutates files** or appends behind a damaged tail. This reports read-only evidence, not a safe writer election or Flash durability. Mock file tests passed in the 2026-10-16 RPI400 Python host suite; no hardware interruption was executed.

## Attempt ticket evidence and uncertainty-aware ordering

`analyse_attempts` groups validated `RecordFacts` by canonical DeviceIdentifier plus RuntimeIncarnationId, checks the single compile-time sequence-width profile for each group, counts repeated observations, and reports bounded unseen intervals strictly **within the captured ticket range**. Gaps are not equated with Logging admission, queue, archive, or radio loss: rejected enabled attempts also consume tickets, and captures may start/end mid-run. The caller sets finite max records and max groups.

`compare_record_time` uses canonical local Monotonic ordering only within one device+incarnation. For other identities it requires identical nonzero ClockEra, actual synchronized coordinate, synchronized/holdover state, and nonsaturated uncertainty; it compares closed uncertainty intervals and returns Earlier, Later, Overlaps, or Incomparable. This is not UTC ordering and does not infer chronology from attempted ticket or FIFO commit order. Original temporal quality policy may impose stricter rules than this conservative mathematical comparability helper and remains external. Authored unit tests are not executable evidence.

## Optional historical generated-family schema (G5)

The host decoder can now receive an **explicit** `--historical-archive` root and `--contract-family-fingerprint` (optionally a 256-bit digest) to use EDP-Localisation's archived EDPL/generated-schema verifier instead of `--schema`. This is a separate historic presentation selection: Field12's *diagnostic schema* fingerprint cannot determine which *translation-sensitive ContractFamily* produced the text. Optional `--trusted-archive-manifest-sha256` is an **independently supplied** byte pin; without it the report correctly marks provenance flags false even though archived files internally match their checksums. No root LogRecord TypeIdentifier is asserted. See `docs/HISTORICAL-DIAGNOSTIC-SCHEMA-BRIDGE.MD`.

The `--historical-language` option supplies the requested Localisation language for bounded **historical template retrieval only**, reporting actual fallback language and `historicalTemplateInterpolated=false`. The diagnostic arguments remain separately schema-typed and are not guessed into translated formatted strings.


## 2026-10-17 optional owner-root pin verification

`--validate-record --expected-root-type-hex HEX16` now tells the
Python CLI to require an exact eight-byte root TypeIdentifier before
labelling a parsed outer structure as a LogRecord candidate.
The value MUST be supplied from independently governed EDP-System
TypeAuthority evidence; selecting a value on the CLI does **not**
authenticate its origin. The output reports
`rootTypeMatchedSuppliedIdentity=true` only after a successful strict
comparison, but retains `rootTypeGoverned=false` to avoid upgrading
caller-selected bytes into historical authenticity. The existing
`--validate-record` (without root pin) remains a conservative
forensic candidate inspection for legacy forensic use.
Malformed/partial option or mismatched types fail closed.
No changed EDLG/EDLS, Typed Envelope or Field11/14 semantics.

## 2026-10-17 bounded multi-device attempt evidence (partial G8/G9)

`--validate-record --expected-root-type-hex HEX16 --analyse-attempts`
now reports separate JSON Lines `kind=attempt-evidence` summaries for
each observed strong DeviceIdentifier and RuntimeIncarnationId pair,
**only** after independently selected exact root-TypeIdentifier matching
and the existing structural LogRecord checks. Generated-schema/publisher
authenticity and archive provenance require their separate trusted
manifests; supplying an expected root is a candidate identity equality
test, never a claim of record origin authentication.

Two explicit resource controls:
`--max-analysis-records` (default 65536; positive) caps stored
validated source observations, and `--max-analysis-groups` (default
1024; positive) caps distinct device/incarnation groups. A bound
exceeded returns a typed `attempt-evidence-error`, nonzero exit,
and NO misleading incomplete summary. Malformed policy arguments
and attempts to analyze without both root pin and structural
validation fail before opening the capture. The raw frame/per-record
JSONL events are still emitted, including damage/errors in forensic
mode.

Each summary distinguishes distinct tickets from duplicate
observations and displays only **unseen intervals between actually
observed tickets**. It explicitly publishes
`partialCapturePossible=true` and `lossCauseInferred=false`.
An unseen ticket is NEVER proof that an event was dropped by
Logging, Persistence, Serial, the producer, or transport. The same
incarnation cannot advertise competing attempt sequence widths;
the existing evidence owner rejects that contradiction rather
than silently partitioning. Cross-device temporal ordering and
translation remain separate governed analyses, not inferred here.

Regression: a synthetic canonical 15-field (14 required, one optional)
CBOR TypedEnvelope inside actual EDLG v1 frames, with tickets
1,3,3, proves two distinct observations, duplicate=1, unseen
interval [2,2], exact device/incarnation, strict overflow refusal and
negative root/option cases. Neither Field 11 nor Field 14 subflags
are allocated by these test-only synthetic placeholder bytes.

## Binary stdin and live byte-stream input (2026-10-17)

The existing reader also accepts **`-`** in place of the path:
`python3 -u -m edp_logging - --forensic`, with the same existing
strict EDLG/EDLS framing, bounded `--max-payload` and bounded
`--resync-limit` behavior as ordinary files. The scanner never
requires `seek()` or loading all input before processing complete
frames. This allows externally captured serial bytes to be piped
through standard input; `-u` prevents host stdout buffering.
The owning capture process MUST keep the serial endpoint in
exclusive binary mode (do not interleave console text/other
writer bytes). For example, external capture utilities may pipe
binary frames into this process using an authorized, already
configured serial device; this decoder itself does not acquire
or configure an endpoint and **does not guarantee** frame
liveness while a producer is silent. Scanner terminates at stdin
EOF; it never closes stdin itself.

The same sticky forensic damaged-interval semantics and explicit
frame-offset reporting apply to nonseekable chunked pipes. A valid
CRC is evidence of accidental corruption detection, not
authenticated origin or durable persistence.

For live viewing with an independent raw evidence file, configure an
exclusive binary capture process to emit bytes on stdout. With
`PYTHONPATH=tools`, feed its output through `tee` before the decoder.
For example, if `capture_command` is your configured capture program:

```bash
set -o pipefail
capture_command | tee -a capture.edlg | python3 -u -m edp_logging - --forensic
```

Choose a new capture file for a new session. The capture process owns the
device and link settings, and `tee` writes the original bytes independently
of decoder interpretation. Inspect the pipeline exit status and keep the
raw capture if a schema is unavailable or frames are damaged. This example
uses ordinary host file writes. Application-specific evidence durability
requires its own storage policy.

## 2026-10-17 bounded uncertainty-aware pairwise time evidence (partial G9)

`--validate-record --expected-root-type-hex HEX16 --compare-record-times`
reuses **the existing** `edp_logging.analysis.compare_record_time`
authoritative comparison semantics and the same bounded, candidate-only
record collection used by `--analyse-attempts`. The CLI performs
comparisons only after full outer LogRecord structural validation and
caller-supplied root identity equality.

Output JSON Lines `kind=temporal-comparison` specifies original
capture `leftOffset`/`rightOffset`, `relationship` = Earlier,
Later, Overlaps or Incomparable, and `certainTotalOrder` true only
when the comparison proves Earlier/Later. It explicitly states
`civilUtcInferred=false` and `unobservedGapLossInferred=false`.
A common *nonzero* EDP-Clock era, usable synchronized clock state and
non-saturated uncertainty are necessary to compare **different**
DeviceIdentifier+RuntimeIncarnation pairs. Distinct unsynchronised
devices with numerically similar local monotonic timestamps remain
**Incomparable**; uncertainty-overlapping synchronized intervals are
**Overlaps**, not sorted into fictional absolute order. This feature
never assumes synchronized Clock coordinates are UTC.

`--max-comparison-pairs` (default 4096, positive) and
`--max-analysis-records` (default 65536) bound the finite
O(n²) output/work and its source memory. If either limit is
exceeded the CLI emits only one
`kind=temporal-evidence-error` (nonzero status) **instead of
an incomplete subset of comparisons**. The raw per-frame findings
remain available. A verified root TypeIdentifier value is still a
user-supplied equality pin, not historical publisher authentication.


## 2026-10-19 — approved Field11/14 historical validation

Strict candidate RecordFacts inspection now validates approved Field11
as a two-element ordered EDP-Clock pair: ClockCorrelationProjectionStatus
(0..3) and TemporalQualityAssessment (0..2), and Field14 as exactly
one uint8 zero-only. Unknown/out-of-range values, malformed child
arrays, boolean coercions and reserved facts fail closed. The CLI
exports numeric clockCorrelationProjectionStatus,
temporalQualityAssessment and recordValidityFacts only after strict
validation, and never asserts record origin/authenticity based solely
on a type identifier.

Cross-device chronological comparison requires both sides explicitly
Correlated (projection 0) and Reliable (quality 2), plus existing same
nonzero ClockEra, eligible synchronization states and nonsaturated
uncertainty. Local monotonic ordering within one device incarnation
remains valid even if synchronized evidence is unreliable. Bounded
attempt ticket gap analysis is unchanged. Tests cover negative wire
states and no false cross-device total-order claim.


## 2026-10-19 — schema-governed positional JSON decoding (G3/G5 incremental)

The historical schema adapter now interprets EDP-Serialisation JSON
positional argument values using only **generated EDP-Localisation**
parameter kinds and owner-emitted shapes; this is not a second encoder:

- `bytes` and `device-identifier`: canonical padded RFC4648 Base64,
  exact round-trip re-encoding (reject alternate padding bits), and 16
  bytes exactly for the DeviceIdentifier
- `type-identifier`: exact eight-element JSON uint8 array, per
  EDP-Serialisation's System canonical representation, with nonzero
  required by strong semantic validation
- `bool`, signed/unsigned fixed widths, bounded UTF-8 text,
  field-identifier, runtime-incarnation, ClockEra and ordered
  StringIdentifier: exact type/range checks against historical schema
- `f32` and `f64`: finite source JSON numeric values converted to
  PreservedFloat with the governed width; binary32 overflow and
  underflow reject, never coerce Boolean/string to float
- Field12 diagnostic fingerprint: JSON 16-element uint8 source array
  or CBOR exact 16-byte form, checked against historical schema
- Integer and floating JSON lexical tokens remain distinct. Structural
  JSON parsing retains arbitrary-precision Decimal until finite
  binary64 range/lexical underflow checks, avoiding irreversible
  `1e-5000` → zero interpretation before schema validation.

These are **typed parameter values** under a previously verified
schema fingerprint, not evidence of the historical schema publisher,
record authenticity, a complete fifteen-field LogRecord serializer,
or an approved generic JSON rendition of every LogRecord field.
Historical template interpolation is still disabled.

Executable unit tests: `tools/tests/test_json_schema_owner.py`.
Full Python test suite: RPI400 detached
`20261019T124110-449cdd49` **PASS exit0**, 34 tests.
An initial fixture incorrectly classified canonical `AQL/` as invalid
(it is legitimate Base64), corrected to `AQL-`. A preexisting
nonfinite JSON envelope test exposed a second transient implementation
gap after Decimal parsing; structural finite-binary64 checks were
added before final PASS. Earlier FAIL runs are not completion evidence.

No new firmware was physically flashed for this host-only decoder
tranche. The earlier actual G4 approved Field11/14 WROOM physical
qualification remains separately valid.

