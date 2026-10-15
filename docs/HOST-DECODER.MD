# Python host diagnostic framing — partial implementation

**Branch:** feature/logging-v1. **Version:** 0.1.0. Parent approved design: Policy/main D8/D13 and EDP-Logging#1; tracked under EDP-Logging#3.

## Implemented source

The pure-stdlib `tools/edp_logging` Python package provides:

- `crc32c`: reflected Castagnoli CRC32C with canonical check vector `123456789 -> E3069283`.
- `encode_frame`: deterministic v1 `EDLG` fixture writer, strictly respecting max-payload limits; this is **host reference only**, not an embedded serializer.
- `scan_stream`: seeks nowhere, reads incrementally through bounded chunks, checks v1 magic, codec (CBOR=1, JSON=2), all flags zero, 32-bit little-endian payload length, complete `GLDE` marker and frame CRC. It never allocates based on unvalidated lengths. Default conservative mode stops at first invalid candidate. Optional forensic mode scans a finite number of discarded bytes for later valid candidates and marks them **forensic**, not part of an uninterrupted archive.
- `parse_segment_header`: strict `EDLS` 40-byte header checker with slot index, nonzero 64-bit generation, canonical 16-byte device and nonzero 32-bit runtime incarnation, header CRC.
- `edp-logging PATH` or `python -m edp_logging PATH` CLI prints one JSON event per verified frame or integrity failure with source offsets. `--archive` validates and displays segment metadata; `--forensic` enables qualified bounded resync.

## NOT YET implemented

- CBOR/JSON **semantic typed LogRecord decoder**, historical schema fingerprint catalogue reconciliation, localisation/parameter interpolation, device-relative sequence gap analysis and Era/uncertainty comparisons.
- Bounded explicit live serial capture/retention, source-based filters/CSV export, or encoder/firmware interoperability confirmation.
- Durable storage recovery guarantees, cryptographic authentication, any interrupted-write simulation or physical power-loss validation.

The CLI reads one source file without modifying it. Frames returned by `scan_stream` own a bounded copy of their payload; aggregate callers must also stream output rather than retaining an unlimited list of frames.

## Authoritative byte format

EDLG header 16 bytes: `EDLG` (4), version=1 (1), codec (1), zero flags (2), payload length LE32 (4), CRC32C LE32 (4); exactly payload length bytes follow, then `GLDE` (4). Outer overhead = 20 bytes. CRC covers initial 12 header bytes then payload then final 4 completion bytes (not the CRC field).

EDLS header 40 bytes: `EDLS` (4), version=1 (1), flags=0 (1), physical slot LE16 (2), generation LE64 (8), DeviceIdentifier (16), incarnation LE32 (4), CRC32C of first 36 bytes (4).

`CRC32C` detects accidental corruption, not authentication. Forensic resync may falsely identify a valid-looking later candidate; report qualified provenance and never infer continuous retained history.

## Tests

`PYTHONPATH=tools python3 -m unittest discover -s tools/tests -v` (after the user permits an execution environment) exercises CRC golden check, valid/truncated/corrupt records, oversized length, bad flags, forensic salvage, scanner bounds and EDLS checks. **Tests have been authored only, not executed.** No appliance access, C++ target build or physical tests occurred.

## Structural Typed Envelope inspection (no catalogue assumptions)

The new `record_structure.py` can validate a **canonical** CBOR root `[1, bytes8, numeric FieldId map]` or existing JSON Typed Envelope `{"$edp":{"v":1,"type":"lowercase-hex16"},"value":{"0":...}}`. CBOR rejects indefinite containers, tags, non-minimal integer/length encoding, unordered/duplicate/non-numeric map keys, half floats, nonfinite floating values, invalid UTF-8, NUL text, trailing bytes and excessive depth/total nodes. Float32 versus float64 remains distinguished by `PreservedFloat(width_bits)`; JSON integer values are parsed losslessly as Python integers, not forced through binary64.

`inspect_typed_envelope` returns `TypedEnvelope` with the root TypeIdentifier, source codec and numeric-identity-keyed field map. **It deliberately does NOT infer the semantic meaning or signedness of positional argument values without the historical DiagnosticSchemaFingerprint**, nor validate LogRecord field 0–14 semantics yet; those future checks need the governed schema. `--inspect-record` on the CLI enables this optional inspection, preserves independently validated frame metadata when the inner record is malformed and reports the typed-envelope error separately. It never pretends valid frame CRC implies a valid semantic record.
