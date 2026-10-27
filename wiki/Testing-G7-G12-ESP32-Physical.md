# Physical ESP32 checkpoint and native Serial qualification

Status: three separate real ESP32-D0WD runs passed on 2026-10-16;
only a partial G7/G12 test-specific binding, NOT the final production sink.

Bootstrap selects existing EDP-Persistence-ESP-IDF NVS provider, bound
through EDP-Persistence KeyValueStorage and the EDP-Memory ByteOperations
abstraction, and EDP-Serial-ESP-IDF native UART FIFO writer under the
existing EDP-Serial bounded frame queue with EDP-Threading arbitration.
No Logging-to-Serial or Logging-to-Mesh link.

NVS first boot writes an exact EDLG frame and commits with NVS.
Second boot after actual esp_restart independently reads through the
generic Logging KeyValue checkpoint bridge, validates CRC and framing,
deletes the test namespace key and confirms ordinary Logging Memory
teardown. Then EDP-Serial transmits an actual raw 24-byte binary
EDLG frame to /dev/serial0 (not a printed hex frame). Host confirms
CRC, codec and payload using existing canonical decoder.

Each of three independent GPIO reset cycles: 11 of 11 app and host
milestones passed, zero FAIL, verified EDLG binary frame TRUE.
Full original flash (4,194,304 bytes) was privately saved before
flashing and restored with independent whole-chip digest matching
BEFORE original normal reboot, then lock released per Policy handoff.

The output and persisted input are TEST-ONLY tuple data. This is
restart recovery, NOT interrupted external power-loss recovery, nor a
rolling EDLS archive, published governed LogRecord schema, or source
authenticity proof. Exact current evidence:
tests/hil/g1-espidf/VALIDATION.MD and docs/EDLG-KEYVALUE-CHECKPOINT.MD.

## 2026-10-16 real SPIFFS FileStorage successor on the WROOM

Application-only disposable custom partition SPIFFS at 0x210000,
1984 KiB, selects the existing concrete ESP-IDF VfsFileStorage
through Bootstrap. Native Flash source readback confirms two
distinct EDLS generations 1 and 2; a third is refused with
NoEmptySlot rather than erasing historic data. Real filesystem
byte-tamper detection finds corrupted slot1 tail while slot0
remains CRC-clean. Three independent physical runs all pass
13/13 test and host checkpoints, with real NVS restart and
EDP-Serial UART CRC independently checked. Complete original
0x400000 firmware image restored and separately digest-verified
before normal firmware reboot. No external power interruption
or actual PowerLoss atomicity demonstrated; VFS provider reports
interruption atomicity None. See HIL VALIDATION.MD.

## 27 October 2026 — actual unacknowledged candidate write-activity VDD interruption

With RPI400 solely authorised and locked, `esp32dev-midwrite` native firmware (SHA256 of factory image `bd05436b05fd2e0455adbc020c7f069125d5b17e889b22ad6c675e9b1d0c05e6`) created and CRC-verified immutable EDLS g1/g2, committed separate NVS `ack=2`, and started actual ESP-IDF LittleFS VfsFileStorage writes solely to physical **unacknowledged slot2**. The host independently commanded GPIO17 VDD OFF **8.275 seconds**, then ON. Detached run **20261027T133518-b251b2aa PASS exit0**. Actual hardware reported ROM `SDIO_RESET` and app reset=10; therefore the supported status is **EXTERNALLY_VERIFIED_POWER_CUT**, explicitly *not* POR_CONFIRMED. After reboot the candidate was a **zero-length existing physical file** (`InspectEdlsSlot` Empty is not proof path absence), so protected rolling conservatively **refused the unusable candidate** without removing/retiring g1 or g2. Reboot firmware checked the exact two ACKed CRC-complete generations and their NVS watermark.

Second independent physical partition flash extraction (0x170000 + 0x90000) **20261027T134001-1ae1e8e5 PASS exit0**, SHA256 `d27f56ece7d5ee7f1c5ec613eac45c3520bb630018598adfcc82b2a2fc167d42`, shows exactly complete 64B CRC-protected g1/g2 and zero-byte unacknowledged slot2, no unexpected files. Offline parser `audit_midwrite_extracted_edls.py` independently rejects damaged protected frames, wrong identities, unexpected files, and symlinks. **58 host fault/CRC tests PASS**, C++20 foundation PASS, Clang finite rolling PASS, existing real EDP-Memory/Threading Task integration under GCC UBSan PASS. Sources and logs are retained on appliance without publishing private partition images or UART dumps.

**Coverage boundaries:** the accepted electrical cut occurred during an *ongoing candidate write activity window*, but UART/controller latency prevents proving the physical switch opened between instructions of one synchronous `fwrite` / `fsync` call. The 65536B disposable candidate is a test-only GC/write-pressure payload, not a normal EDLS segment. No general FileStorage PowerLoss atomicity, production provenance-authorized automatic torn-candidate disposal, flash endurance, or on-target production scheduling guarantee follows from this test. Logging remains **not production-ready** until separately outstanding gates pass. All versions 0.1.0.
