# G4→G10 real WROOM full typed record fanout proof

## 2026-10-22 — G4→G10 fully typed fifteen-field record, two independent copying sinks on real ESP32

Fresh dedicated ESP32-D0WD WROOM firmware contains `g4_root_owned_fanout_check.hpp` called from the native HIL. A generated EDP-Localisation schema supplies `RuntimeIncarnationId` and `ClockEra` Field 13 arguments, captured in owned source storage and projected into the complete, permanent fifteen-field typed envelope. The test encodes that envelope twice—once CBOR and once JSON—using **EDP-Serialisation**, each exactly once into one leased, bounded EDP-Memory representation. Two synchronous sinks independently copy the bytes before the source lease is released: an EDP-Serial bounded FIFO and an archive-*style in-memory copying test sink*. The FIFO subsequently pumps an EDLG frame, verifies frame CRC and confirms byte-for-byte equality with the archive copy; the typed CBOR/JSON payload is decoded from the transmitted frame and compared with the source identity, argument values, attempt, and schema fingerprint. Both encodings reclaim the memory lease and tear down the owner runtime successfully.

This is not a durable EDP-Persistence archive, async reference-counted lifetime, proof of physical UART receipt beyond the test endpoint, or unexpected PowerLoss validation. The coordinator/product G1–G15 completion gates remain open.

RPI400 authoritative evidence from 22 October 2026:

- Entire PioArduino/ESP-IDF WROOM image build **20261022T124919-34c6630c PASS exit0**, source main blob `0090c59e73fe40dff0c7f6cc6c1ca8dd92367f1a`, new test header blob `996eb9f7cda61a1f52099d0b7c8cd2621eb284ed`, factory image SHA256 `21f423be8a9fac6e70ea0dc0d837367581e4f7d5beadd2dab03292f0f667beb4`.
- Physical write at address 0x0 and separate independent flash verification, followed by **three** actual reset + NVS-restart UART test captures in detached run **20261022T125506-8e81073b PASS exit0**, completed **12:55:43 CEST**.
- Each run achieved 18/18 native functional markers, EDLG CRC, resource and G11 instrumentation checks, complete CBOR/JSON record encode/decode and both independent copying-sink paths. Per-phase WROOM stack minimum margin was **10,472 bytes** over these three executions. This is measured test-task margin only, not Logging product worst-case budget.
- Raw captures and flash/verify logs remain on RPI400 in `/srv/espressio/scratch/gpt6-logging-20261022-0919-v4/g4-g10-owned-full-root-physical-20261022`, manifest SHA256 `a239f8fae1fd0d0e3ed3844059db5b7c134f289a2ff87af7b37afc454cd3c17c`. Do not commit private captures, firmware binary or log dumps.

The physical main.cpp was compared line-for-line against the exact remote branch blob `31bc06e1aed37f9381396752c47de25ab74c7be4`; it differs **only** by the new test header import and single call to the new test. No other HIL source code was replaced.
