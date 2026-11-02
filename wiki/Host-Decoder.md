# Python host decoder: bounded framing and archive headers

This page covers `tools/edp_logging/framing.py`, `archive.py`, `cli.py`, `__main__.py` and `tools/tests/test_framing.py`. This is a bounded, read-only host tool with framing, structural Typed Envelope inspection and a separately governed LogRecord semantic-field validation path. JSON argument interpretation may require an exact historical diagnostic schema; do not infer it from framing alone.

`FrameCodec`: Cbor=1, Json=2. `FrameStatus`: distinct invalid marker, partial header/frame, unsupported version/codec/flags, oversized payload, invalid `GLDE` marker, CRC mismatch and over-budget resynchronization.

`Frame`: immutable validated byte-offset/codec/payload/checksum and `recovered_candidate` flag. `FrameIssue`: byte-offset plus typed frame status. `crc32c` uses canonical reflected Castagnoli. `encode_frame` emits portable reference fixture bytes.

`scan_stream` is a single-pass bounded incremental scanner over any BinaryIO, with prevalidated lengths, no seek/heap proportional to entire file, conservative default and finite optional forensic recovery. An issue invalidates continuous normal recovery even if later CRC-valid candidates are found; recovered candidates are labelled, not silently trusted. `parse_segment_header` validates exact 40-byte EDLS metadata and checksum before scanning the archive payload at base_offset=40.

`cli.main` scans a caller-supplied file and prints JSON evidence; it never mutates the input. Tools and tests belong to version 0.1.0. On 2026-10-16 RPI400 executed the Python host suite (20 tests, PASS; detached run `20261016T065108-f102fd04`) and the existing C++20 foundation tests with GCC/Clang (PASS). These are host tests, not physical device or power-loss validation.

## Structural Typed Envelope inspection (no catalogue assumptions)

The new `record_structure.py` can validate a **canonical** CBOR root `[1, bytes8, numeric FieldId map]` or existing JSON Typed Envelope `{"$edp":{"v":1,"type":"lowercase-hex16"},"value":{"0":...}}`. CBOR rejects indefinite containers, tags, non-minimal integer/length encoding, unordered/duplicate/non-numeric map keys, half floats, nonfinite floating values, invalid UTF-8, NUL text, trailing bytes and excessive depth/total nodes. Float32 versus float64 remains distinguished by `PreservedFloat(width_bits)`; JSON integer values are parsed losslessly as Python integers, not forced through binary64.

`inspect_typed_envelope` returns `TypedEnvelope` with the root TypeIdentifier, source codec and numeric-identity-keyed field map. **It deliberately does NOT infer the semantic meaning or signedness of positional argument values without the historical DiagnosticSchemaFingerprint**, nor validate LogRecord field 0–14 semantics yet; those future checks need the governed schema. `--inspect-record` on the CLI enables this optional inspection, preserves independently validated frame metadata when the inner record is malformed and reports the typed-envelope error separately. It never pretends valid frame CRC implies a valid semantic record.

## Matching historical diagnostic argument schemas (CBOR only)

`schema.py` loads the generated Localisation `diagnostic-schema.json` artefact in a bounded way: strict duplicate-name rejection, supported schema version and identifier widths, sorted governed String tuples, `diagnosticDeclared` state (including legitimate zero-argument diagnostics), dense declared positional types/lifecycle and consistent 128-bit fingerprint/full digest. A single host artefact is **not independently authenticated**; the caller must obtain it through a trusted/pinned deployment source or verify the generated-set manifest separately. The flattened current Localisation JSON does not contain enough parent-state information to recalculate its full SHA-256 transcript independently; do **not** confuse a matching claimed prefix with cryptographic verification.

`decode_declared_arguments(TypedEnvelope, DiagnosticSchema)` currently validates CBOR fields 0 (StringIdentifier triple), 12 (exact matching fingerprint) and 13 (positional array) and checks scalar values against their authorised types, including signed/unsigned range, distinct bool, preserved binary32/binary64 width and strong identifier shapes. It **refuses JSON semantic argument typing** until the JSON byte-string/strong-identity representation is locked; structural Typed Envelope validation still supports JSON.

The CLI `--schema` selects historical schema validation and records exact message identity, argument count and canonical types in its JSON output without inventing translation wording. Missing/mismatched schema or invalid parameter values report a distinct `schemaError` while retaining independently verified frame/offset provenance. Original payload bytes remain in the caller's source file.

**Explicit exact-byte pin (2026-10-16):** `schema.load_generated_schema(data, trusted_artifact_sha256=pin)` optionally compares SHA-256 over the complete input bytes to a required 32-byte digest obtained independently from a trusted/pinned Localisation generated-set manifest. The new CLI `--schema-sha256` requires `--schema` and precisely 64 hex characters. Invalid pins or mismatches yield typed `SchemaError`/CLI `schema-error` before interpretation, not silent fallback. `DiagnosticSchema.artifact_sha256_pin_matched` and the CLI `schemaArtifactSha256PinMatched` evidence fact distinguish unpinned content from exact matched content. **This does NOT independently recompute the canonical DiagnosticSchemaFingerprint** (the flattened JSON omits parent hierarchy facts), authenticate the manifest/pin-distribution channel, or establish a governed LogRecord TypeIdentifier. Independently source and pin the generated manifest SHA-256; never treat an untrusted schema's own declared digest as its validation anchor. No Localisation fingerprint engine is cloned into Logging.

## Schema-input strictness

`schema.py` rejects duplicate or unknown root properties, non-integer schema-version values, nonfinite JSON numeric literals and unexpected parameter field types before attempting membership/shape validation. Retired catalogue Strings cannot be interpreted as active message invocations. All input syntax errors are typed `SchemaError` rather than an accidental TypeError. Tests passed as part of the RPI400 Python host suite on 2026-10-16.

## LogRecord candidate facts (D13 FieldIds 0–14)

`record_facts.py` now provides a bounded semantic-range check over a structurally decoded Typed Envelope. It validates all required D13 fields, optional field7 synchronized coordinate (zero is valid), emitted severity 0..5, 16-byte nonzero DeviceIdentifier, uint32 incarnation, attempt ticket within explicitly selected 16/32/64-bit width (nonzero, no wrap), unsigned nanosecond timestamps, EDP-Clock state 0..4, opaque Era, saturated uint32 uncertainty, exact 16-byte schema fingerprint and positional array. Missing or unregistered FieldIds fail. Incoherent `NeverSynchronized` plus coordinate and present coordinate without valid Era also fail.

The permanent LogRecord TypeIdentifier is issued by EDP-System (00 00 01 00 00 00 00 01). This offline tool does **not authenticate the issuer merely from bytes**: it returns a `RecordFacts` **candidate**, with optional `expected_root_type_identifier` for future registered equality checking; never treat unknown nonzero TypeIds as conclusively LogRecord. Field 11 has approved Clock correlation/quality numeric child domains and Field 14 permits only zero in v1; the current semantic fact parser rejects invalid members instead of silently treating them as historical truth. The CLI `--validate-record` exports qualified `logRecordCandidate=true`, `rootTypeGoverned=false`, without claiming chronology or authentication.

This is bounded structural and semantic validation, not authentication of the evidence source. The current native Python regression suite exercises these validations and negative cases; see the qualification ledger for exact scope.

## Sticky post-corruption forensic provenance

After first damage, the scanner classifies every later record in the same stream as a candidate requiring forensic qualification. Only the resync-search byte counter resets between recognized frames. This prevents later CRC-valid frames from being mislabeled as part of a continuous verified prefix. The regression test passed as part of the RPI400 Python host suite on 2026-10-16.

## Read-only archive slot inspection

`inspect_slot` and `inspect_archive_slots` in `archive_recovery.py` stream bounded fixed-slot sources, track contiguous verified frame prefixes, surface torn/corrupt tail, compare nonwrapping generations and refuse a conclusive next generation when any header is untrustworthy, duplicate or inconsistent. No automatic truncation/append, no rename/listing assumption, and no claim of atomic Flash durability. Mock archive tests passed under RPI400 Python; independent physical WROOM PowerLoss and LittleFS/NVS integrity are separately qualified by the current application fixture, not by this host parser alone.

## Sequence and time provenance

`analysis.py` offers strictly bounded per-incarnation observed attempt intervals/duplicates with no inference of why a ticket is unseen, and a deterministic pairwise `compare_record_time` that is monotonic within one Device/Incarnation and uncertainty-interval-aware only within the same valid ClockEra. Saturated uncertainty, missing time or mismatched Era results in Incomparable; overlapping intervals do not establish order. These analysis tests passed in the RPI400 Python host suite on 2026-10-16; no distributed physical-clock validation is implied.
