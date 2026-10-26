# G7 LittleFS real EDLS N+1 protected rolling / GPIO17 electrical-cut fixture

**Test-only ESP32-WROOM-32 application** under existing `EDP-Logging`, not a new FileStorage provider, retention owner or production deployable. Reuses `logging/EdlsArchiveRotation.hpp` (2026-10-26 immutable N+1 API), the existing `EDP-Persistence-ESP-IDF::LittleFsVfsMount` and conservative `VfsFileStorage`, canonical 40-byte EDLS CRC header and complete EDLG V1 CRC-bearing **CBOR framing**. Raw CBOR payload is a tiny deterministic test message (array [1,2,3]), **not** yet a typed full 15-field LogRecord projection via EDP-Serialisation.

## Finite geometry and crash phases

The application Bootstrap selects three static paths, `slot0` / `slot1` / `slot2`, with **two protected EDLS generations** and one empty physical successor name. Distinct device identity is 16 bytes of 0xA6, frames bounded by 32-byte payload and 1024-byte maximum segment. Each test segment is precisely 64 bytes (40-byte EDLS header + 24-byte EDLG/CBOR framed record). Initial Boot publishes/validates g1/g2 using the **new protected owner**. After 12 seconds and durable test-only NVS phase arm, it publishes fully committed/readback-verified g3 in an EMPTY spare using the existing guarded successor. Crucially, g3 is a complete N+1 candidate, but g1 is **not yet physically retired** when the host cuts power.

The borrowed official RPI400 `powercut_qualification.py` and `relay_cut_monitor.py` are reused as a **test-only host controller**, without weakening their raw reset+boot-epoch/controller OFF/ON/minimum elapsed hold requirements. The application emits established bounded qualification labels plus separately truthful `EDP_EDLS` labels: before cut g1/g2 are last ACK, g3 is complete but not ACKed; after cut, it independently confirms all three complete CRC-valid EDLS/EDLG files then deliberately calls `FinishEdlsProtectedRollingRetirement`. Only after g1 retires does it publish g4 via the new protected-rolling API, leaving precisely g3/g4 and one free physical filename. The host requires `LOWSPACE_ROLLED` (truthfully emitted for intermediate verified g2/g3) and final `RECOVERED newest=4 older=3` plus a new boot epoch and `REPLAY_VERIFIED`. No native reset code is treated as electrical proof; verdict separates `POR_CONFIRMED` and `EXTERNALLY_VERIFIED_POWER_CUT`.

## RPI400 electrical safeguards

Only the explicitly available authorised RPI400 under an exclusive `espressio-agent-lock` may run this. The borrowed `run_relay_cut_test.sh` first creates an exact **2MiB private full-chip backup** and separately verifies the native combined firmware image and 589824-byte dedicated LittleFS seeded test image at offset 0x170000. It never autoformats on mount. Set:

- `EDP_LITTLEFS_AGENT_ID=<current-held-owner>`
- `EDP_LITTLEFS_EVIDENCE_DIR=<unique-private-absolute-path>`
- `EDP_LFS_CUT_STAGE=lowspace`
- `EDP_LFS_BUILD_ENV=esp32dev-lowspace`
- `EDP_LFS_OFF_HOLD_S=8.0` (or another controller-confirmed, approved hold within the host's 5..60s range)

The wrapper traps exit to `esp32-integrated power-off` even on failure. Power interruptions are performed ONLY by the user-confirmed no-backpower GPIO17 relay. No proprietary flash image, raw UART, NVS state or firmware binary belongs in GitHub.

After a physical run, separately read the **actual** 0x170000 / 0x90000 partition into RPI400 private storage, extract it with the already installed `littlefs-python extract --block-size 4096`, and run `PYTHONPATH=EDP-Logging/tools python3 audit_extracted_edls.py <private-dir>`. It reuses the existing Python EDLS header CRC32C and EDLG frame scanner, requiring only seed.txt, g3 in physical slot2, and g4 in slot0, each complete with correct identity, incarnation, CRC, payload and byte length. **No generic FileStorage PowerLoss atomicity, throughput, physical GC-phase coverage or flash endurance limit follows.**

## Qualification limitations

This fixture tests a **complete immutable successor → physical VDD cut → CRC-verified reboot → oldest-eligible retirement → next verified roll** lifecycle. The power loss occurs between two logical operations; it is not evidence of a flash-operation mid-instruction cut. It does NOT cover torn candidate identification/repair, all low-space file allocation failures, full rate/heap/stack/capacity/wear bounds, or full LogRecord→canonical typed CBOR projection. Host `test_*.py` includes borrowed 32 preexisting controller/unit cases and eight new independently authored offline physical EDLS audit negatives. The canonical Logging owner remains 0.1.0; underlying VFS advertised traits remain Restart/None/MayModify and all earlier failure receipts remain unchanged.
