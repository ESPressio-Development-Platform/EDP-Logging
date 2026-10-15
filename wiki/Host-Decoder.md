# Python host decoder: bounded framing and archive headers

This page covers `tools/edp_logging/framing.py`, `archive.py`, `cli.py`, `__main__.py` and `tools/tests/test_framing.py`. This is a **PARTIAL** host tool, without semantic CBOR/JSON LogRecord decoding.

`FrameCodec`: Cbor=1, Json=2. `FrameStatus`: distinct invalid marker, partial header/frame, unsupported version/codec/flags, oversized payload, invalid `GLDE` marker, CRC mismatch and over-budget resynchronization.

`Frame`: immutable validated byte-offset/codec/payload/checksum and `recovered_candidate` flag. `FrameIssue`: byte-offset plus typed frame status. `crc32c` uses canonical reflected Castagnoli. `encode_frame` emits portable reference fixture bytes.

`scan_stream` is a single-pass bounded incremental scanner over any BinaryIO, with prevalidated lengths, no seek/heap proportional to entire file, conservative default and finite optional forensic recovery. An issue invalidates continuous normal recovery even if later CRC-valid candidates are found; recovered candidates are labelled, not silently trusted. `parse_segment_header` validates exact 40-byte EDLS metadata and checksum before scanning the archive payload at base_offset=40.

`cli.main` scans a caller-supplied file and prints JSON evidence; it never mutates the input. Tools and tests belong to version 0.1.0. Unit tests are committed but NOT RUN; no RPI400/AI-AGENT-02 interaction.

## Structural Typed Envelope inspection (no catalogue assumptions)

The new `record_structure.py` can validate a **canonical** CBOR root `[1, bytes8, numeric FieldId map]` or existing JSON Typed Envelope `{"$edp":{"v":1,"type":"lowercase-hex16"},"value":{"0":...}}`. CBOR rejects indefinite containers, tags, non-minimal integer/length encoding, unordered/duplicate/non-numeric map keys, half floats, nonfinite floating values, invalid UTF-8, NUL text, trailing bytes and excessive depth/total nodes. Float32 versus float64 remains distinguished by `PreservedFloat(width_bits)`; JSON integer values are parsed losslessly as Python integers, not forced through binary64.

`inspect_typed_envelope` returns `TypedEnvelope` with the root TypeIdentifier, source codec and numeric-identity-keyed field map. **It deliberately does NOT infer the semantic meaning or signedness of positional argument values without the historical DiagnosticSchemaFingerprint**, nor validate LogRecord field 0–14 semantics yet; those future checks need the governed schema. `--inspect-record` on the CLI enables this optional inspection, preserves independently validated frame metadata when the inner record is malformed and reports the typed-envelope error separately. It never pretends valid frame CRC implies a valid semantic record.

## Matching historical diagnostic argument schemas (CBOR only)

`schema.py` loads the generated Localisation `diagnostic-schema.json` artefact in a bounded way: strict duplicate-name rejection, supported schema version and identifier widths, sorted governed String tuples, `diagnosticDeclared` state (including legitimate zero-argument diagnostics), dense declared positional types/lifecycle and consistent 128-bit fingerprint/full digest. A single host artefact is **not independently authenticated**; the caller must obtain it through a trusted/pinned deployment source or verify the generated-set manifest separately. The flattened current Localisation JSON does not contain enough parent-state information to recalculate its full SHA-256 transcript independently; do **not** confuse a matching claimed prefix with cryptographic verification.

`decode_declared_arguments(TypedEnvelope, DiagnosticSchema)` currently validates CBOR fields 0 (StringIdentifier triple), 12 (exact matching fingerprint) and 13 (positional array) and checks scalar values against their authorised types, including signed/unsigned range, distinct bool, preserved binary32/binary64 width and strong identifier shapes. It **refuses JSON semantic argument typing** until the JSON byte-string/strong-identity representation is locked; structural Typed Envelope validation still supports JSON.

The CLI `--schema` selects historical schema validation and records exact message identity, argument count and canonical types in its JSON output without inventing translation wording. Missing/mismatched schema or invalid parameter values report a distinct `schemaError` while retaining independently verified frame/offset provenance. Original payload bytes remain in the caller's source file.

## Schema-input strictness

`schema.py` rejects duplicate or unknown root properties, non-integer schema-version values, nonfinite JSON numeric literals and unexpected parameter field types before attempting membership/shape validation. Retired catalogue Strings cannot be interpreted as active message invocations. All input syntax errors are typed `SchemaError` rather than an accidental TypeError. Tests have not run.
