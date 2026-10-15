# Python host decoder: bounded framing and archive headers

This page covers `tools/edp_logging/framing.py`, `archive.py`, `cli.py`, `__main__.py` and `tools/tests/test_framing.py`. This is a **PARTIAL** host tool, without semantic CBOR/JSON LogRecord decoding.

`FrameCodec`: Cbor=1, Json=2. `FrameStatus`: distinct invalid marker, partial header/frame, unsupported version/codec/flags, oversized payload, invalid `GLDE` marker, CRC mismatch and over-budget resynchronization.

`Frame`: immutable validated byte-offset/codec/payload/checksum and `recovered_candidate` flag. `FrameIssue`: byte-offset plus typed frame status. `crc32c` uses canonical reflected Castagnoli. `encode_frame` emits portable reference fixture bytes.

`scan_stream` is a single-pass bounded incremental scanner over any BinaryIO, with prevalidated lengths, no seek/heap proportional to entire file, conservative default and finite optional forensic recovery. An issue invalidates continuous normal recovery even if later CRC-valid candidates are found; recovered candidates are labelled, not silently trusted. `parse_segment_header` validates exact 40-byte EDLS metadata and checksum before scanning the archive payload at base_offset=40.

`cli.main` scans a caller-supplied file and prints JSON evidence; it never mutates the input. Tools and tests belong to version 0.1.0. Unit tests are committed but NOT RUN; no RPI400/AI-AGENT-02 interaction.
